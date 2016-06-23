/*
 *  linux/arch/arm/kernel/devtree.c
 *
 *  Copyright (C) 2009 Canonical Ltd. <jeremy.kerr@canonical.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <stdarg.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/threads.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/initrd.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/kexec.h>
#include <linux/debugfs.h>
#include <linux/irq.h>
#include <linux/memblock.h>

#include <asm/page.h>
#include <asm/processor.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/cputype.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/smp_plat.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	memblock_add(base, size);
}

void * __init early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return __va(memblock_alloc(size, align));
}

#ifdef CONFIG_EARLY_PRINTK
char *stdout;

int __init early_init_dt_scan_chosen_serial(unsigned long node,
				const char *uname, int depth, void *data)
{
	int size;
	const char *p;

	pr_debug("%s: depth: %d, uname: %s\n", __func__, depth, uname);

	if (depth == 1 && (strcmp(uname, "chosen") == 0 ||
				strcmp(uname, "chosen@0") == 0)) {
		p = of_get_flat_dt_prop(node, "linux,stdout-path", &size);
		if (p != NULL && size > 0)
			stdout = "linux,stdout-path"; /* store pointer to stdout-path */
	}

	if (stdout && strstr(stdout, uname)) {
		p = of_get_flat_dt_prop(node, "compatible", &size);
		pr_debug("Compatible string: %s\n", p);

		if ((strncmp(p, "xlnx,xps-uart16550", 18) == 0) ||
			(strncmp(p, "xlnx,axi-uart16550", 18) == 0)) {
			unsigned int addr;

			*(u32 *)data = UART16550;

			addr = *(u32 *)of_get_flat_dt_prop(node, "reg", &size);
			addr += *(u32 *)of_get_flat_dt_prop(node,
							"reg-offset", &size);
			/* clear register offset */
			return be32_to_cpu(addr) & ~3;
		}
		if ((strncmp(p, "xlnx,xps-uartlite", 17) == 0) ||
				(strncmp(p, "xlnx,opb-uartlite", 17) == 0) ||
				(strncmp(p, "xlnx,axi-uartlite", 17) == 0) ||
				(strncmp(p, "xlnx,mdm", 8) == 0)) {
			unsigned int *addrp;

			*(u32 *)data = UARTLITE;

			addrp = of_get_flat_dt_prop(node, "reg", &size);
			return be32_to_cpup(addrp); /* return address */
		}
	}
	return 0;
}

/* this function is looking for early console - Microblaze specific */
int __init of_early_console(void *version)
{
	return of_scan_flat_dt(early_init_dt_scan_chosen_serial, version);
}
#endif

void __init early_init_devtree(void *params)
{
	pr_debug(" -> early_init_devtree(%p)\n", params);

	/* Setup flat device-tree pointer */
	initial_boot_params = params;

	/* Retrieve various informations from the /chosen node of the
	 * device-tree, including the platform type, initrd location and
	 * size, TCE reserve, and more ...
	 */
	of_scan_flat_dt(early_init_dt_scan_chosen, cmd_line);

	/* Scan memory nodes and rebuild MEMBLOCKs */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);

	/* Save command line for /proc/cmdline and then parse parameters */
	strlcpy(boot_command_line, cmd_line, COMMAND_LINE_SIZE);
	parse_early_param();

	memblock_allow_resize();

	pr_debug("Phys. mem: %lx\n", (unsigned long) memblock_phys_mem_size());

	pr_debug(" <- early_init_devtree()\n");
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init early_init_dt_setup_initrd_arch(unsigned long start,
		unsigned long end)
{
	initrd_start = (unsigned long)__va(start);
	initrd_end = (unsigned long)__va(end);
	initrd_below_start_ok = 1;
}
#endif


void * __init early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return alloc_bootmem_align(size, align);
}

/*
 * arm_dt_init_cpu_maps - Function retrieves cpu nodes from the device tree
 * and builds the cpu logical map array containing MPIDR values related to
 * logical cpus
 *
 * Updates the cpu possible mask with the number of parsed cpu nodes
 */
void __init arm_dt_init_cpu_maps(void)
{
	/*
	 * Temp logical map is initialized with UINT_MAX values that are
	 * considered invalid logical map entries since the logical map must
	 * contain a list of MPIDR[23:0] values where MPIDR[31:24] must
	 * read as 0.
	 */
	struct device_node *cpu, *cpus;
	u32 i, j, cpuidx = 1;
	u32 mpidr = is_smp() ? read_cpuid_mpidr() & MPIDR_HWID_BITMASK : 0;

	u32 tmp_map[NR_CPUS] = { [0 ... NR_CPUS-1] = MPIDR_INVALID };
	bool bootcpu_valid = false;
	cpus = of_find_node_by_path("/cpus");

	if (!cpus)
		return;

	for_each_child_of_node(cpus, cpu) {
		u32 hwid;

		if (of_node_cmp(cpu->type, "cpu"))
			continue;

		pr_debug(" * %s...\n", cpu->full_name);
		/*
		 * A device tree containing CPU nodes with missing "reg"
		 * properties is considered invalid to build the
		 * cpu_logical_map.
		 */
		if (of_property_read_u32(cpu, "reg", &hwid)) {
			pr_debug(" * %s missing reg property\n",
				     cpu->full_name);
			return;
		}

		/*
		 * 8 MSBs must be set to 0 in the DT since the reg property
		 * defines the MPIDR[23:0].
		 */
		if (hwid & ~MPIDR_HWID_BITMASK)
			return;

		/*
		 * Duplicate MPIDRs are a recipe for disaster.
		 * Scan all initialized entries and check for
		 * duplicates. If any is found just bail out.
		 * temp values were initialized to UINT_MAX
		 * to avoid matching valid MPIDR[23:0] values.
		 */
		for (j = 0; j < cpuidx; j++)
			if (WARN(tmp_map[j] == hwid, "Duplicate /cpu reg "
						     "properties in the DT\n"))
				return;

		/*
		 * Build a stashed array of MPIDR values. Numbering scheme
		 * requires that if detected the boot CPU must be assigned
		 * logical id 0. Other CPUs get sequential indexes starting
		 * from 1. If a CPU node with a reg property matching the
		 * boot CPU MPIDR is detected, this is recorded so that the
		 * logical map built from DT is validated and can be used
		 * to override the map created in smp_setup_processor_id().
		 */
		if (hwid == mpidr) {
			i = 0;
			bootcpu_valid = true;
		} else {
			i = cpuidx++;
		}

		if (WARN(cpuidx > nr_cpu_ids, "DT /cpu %u nodes greater than "
					       "max cores %u, capping them\n",
					       cpuidx, nr_cpu_ids)) {
			cpuidx = nr_cpu_ids;
			break;
		}

		tmp_map[i] = hwid;
	}

	if (!bootcpu_valid) {
		pr_warn("DT missing boot CPU MPIDR[23:0], fall back to default cpu_logical_map\n");
		return;
	}

	/*
	 * Since the boot CPU node contains proper data, and all nodes have
	 * a reg property, the DT CPU list can be considered valid and the
	 * logical map created in smp_setup_processor_id() can be overridden
	 */
	for (i = 0; i < nr_cpu_ids; i++) {
		if (i < cpuidx) {
			set_cpu_possible(i, true);
			cpu_logical_map(i) = tmp_map[i];
			pr_debug("cpu logical map 0x%x\n", cpu_logical_map(i));
		} else {
			set_cpu_possible(i, false);
		}
	}
}

bool arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return phys_id == cpu_logical_map(cpu);
}

static void print_device_tree_node(struct device_node *node, int depth) {
  int i = 0;
  struct device_node *child;
  struct property    *properties;
  char                indent[255] = "";

  for(i = 0; i < depth * 3; i++) {
    indent[i] = ' ';
  }
  indent[i] = '\0';
  ++depth;

  for_each_child_of_node(node, child) {
    printk(KERN_INFO "%s{ name = %s\n", indent, child->name);
    printk(KERN_INFO "%s  type = %s\n", indent, child->type);
    for (properties = child->properties; properties != NULL; properties = properties->next) {
      printk(KERN_INFO "%s  %s (%d)\n", indent, properties->name, properties->length);
    }
    print_device_tree_node(child, depth);
    printk(KERN_INFO "%s}\n", indent);
  }
}

/**
 * setup_machine_fdt - Machine setup when an dtb was passed to the kernel
 * @dt_phys: physical address of dt blob
 *
 * If a dtb was passed to the kernel in r2, then use it to choose the
 * correct machine_desc and to setup the system.
 */
const struct machine_desc * __init setup_machine_fdt(unsigned int dt_phys)
{
	struct boot_param_header *devtree;
	const struct machine_desc *mdesc, *mdesc_best = NULL;
	unsigned int score, mdesc_score = ~1;
	unsigned long dt_root;
	const char *model;

#ifdef CONFIG_ARCH_MULTIPLATFORM
	DT_MACHINE_START(GENERIC_DT, "Generic DT based system")
	MACHINE_END

	mdesc_best = &__mach_desc_GENERIC_DT;
#endif

	if (!dt_phys)
		return NULL;

	devtree = phys_to_virt(dt_phys);

	/* check device tree validity */
	if (be32_to_cpu(devtree->magic) != OF_DT_HEADER)
		return NULL;

	/* Search the mdescs for the 'best' compatible value match */
	initial_boot_params = devtree;
	dt_root = of_get_flat_dt_root();
	for_each_machine_desc(mdesc) {
		score = of_flat_dt_match(dt_root, mdesc->dt_compat);
		if (score > 0 && score < mdesc_score) {
			mdesc_best = mdesc;
			mdesc_score = score;
		}
	}
	if (!mdesc_best) {
		const char *prop;
		int size;

		early_print("\nError: unrecognized/unsupported "
			    "device tree compatible list:\n[ ");

		prop = of_get_flat_dt_prop(dt_root, "compatible", &size);
		while (size > 0) {
			early_print("'%s' ", prop);
			size -= strlen(prop) + 1;
			prop += strlen(prop) + 1;
		}
		early_print("]\n\n");

		dump_machine_table(); /* does not return */
	}

	model = of_get_flat_dt_prop(dt_root, "model", NULL);
	if (!model)
		model = of_get_flat_dt_prop(dt_root, "compatible", NULL);
	if (!model)
		model = "<unknown>";
	pr_info("Machine: %s, model: %s\n", mdesc_best->name, model);

	/* Retrieve various information from the /chosen node */
	of_scan_flat_dt(early_init_dt_scan_chosen, boot_command_line);
	/* Initialize {size,address}-cells info */
	of_scan_flat_dt(early_init_dt_scan_root, NULL);
	/* Setup memory, calling early_init_dt_add_memory_arch */
	of_scan_flat_dt(early_init_dt_scan_memory, NULL);

	/* Change machine number to match the mdesc we're using */
	__machine_arch_type = mdesc_best->nr;

	return mdesc_best;
}

/*******
 *
 * New implementation of the OF "find" APIs, return a refcounted
 * object, call of_node_put() when done.  The device tree and list
 * are protected by a rw_lock.
 *
 * Note that property management will need some locking as well,
 * this isn't dealt with yet.
 *
 *******/

#if defined(CONFIG_DEBUG_FS) && defined(DEBUG)
static struct debugfs_blob_wrapper flat_dt_blob;

static int __init export_flat_device_tree(void)
{
	struct dentry *d;

	flat_dt_blob.data = initial_boot_params;
	flat_dt_blob.size = initial_boot_params->totalsize;

	d = debugfs_create_blob("device-tree", S_IFREG | S_IRUSR,
				of_debugfs_root, &flat_dt_blob);
	if (!d)
		return 1;

	return 0;
}
device_initcall(export_flat_device_tree);
#endif
