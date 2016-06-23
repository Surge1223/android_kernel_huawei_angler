/*
 * kexec for arm64
 */

#include <asm/assembler.h>
#include <asm/kexec.h>
#include <asm/memory.h>
#include <asm/page.h>

/*
 * kexec_cpu_spin - Spin the CPU as described in the arm64/booting.txt document.
 *
 * Prototype: void kexec_cpu_spin(phys_addr_t release_addr, phys_addr_t signal);
 *
 * The caller must initialize release_addr to zero or a valid address
 * prior to calling kexec_cpu_spin.  Note that if the MMU will be turned on
 * or off while the CPU is spinning here this code must be in an identity
 * mapped page.  The value written to release_addr must be in little endian
 * order.
 */

.align 3

.globl kexec_cpu_spin
kexec_cpu_spin:

	/* Signal that this cpu has entered. */
1:
	ldxr    x2, [x1]
	sub     x2, x2, 1
	stxr    w3, x2, [x1]
	cbnz    w3, 1b


	/* Spin while release_addr is zero. */
1:
	wfe
	ldr	x4, [x0]
	cbz	x4, 1b

	/* Convert LE to CPU. */

#if defined(__AARCH64EB__)
	rev	x4, x4
#endif

	/* Jump to new kernel. */

	mov	x0, xzr
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr

	br	x4

.align 3

.kexec_cpu_spin_end:

/*
 * kexec_cpu_spin_size - Byte count for copy operations.
 */

.globl kexec_cpu_spin_size
kexec_cpu_spin_size:
	.quad .kexec_cpu_spin_end - kexec_cpu_spin


/*
 * arm64_relocate_new_kernel - Put the 2nd stage kernel image in place and boot it.
 *
 * The memory that the old kernel occupies may be overwritten when coping the
 * new kernel to its final location.  To assure that the arm64_relocate_new_kernel
 * routine which does that copy is not overwritten, all code and data needed
 * by arm64_relocate_new_kernel must be between the symbols arm64_relocate_new_kernel and
 * arm64_relocate_new_kernel_end.  The machine_kexec() routine will copy
 * arm64_relocate_new_kernel to the kexec control_code_page, a special page which
 * has been set up to be preserved during the kernel copy operation.
 */

/* These definitions correspond to the kimage_entry flags in linux/kexec.h */

#define IND_DESTINATION_BIT 0
#define IND_INDIRECTION_BIT 1
#define IND_DONE_BIT        2
#define IND_SOURCE_BIT      3

.align 3

.globl arm64_relocate_new_kernel
arm64_relocate_new_kernel:

	/* Signal secondary cpus to enter the memreserve spin code. */

	ldr	x1, kexec_signal_addr
	ldr	x2, kexec_spinner_count
	str	x2, [x1]

	/* Wait for all secondary cpus to enter. */
1:
	ldr	x2, [x1]
	cbnz	x2, 1b

	/* Copy the new kernel image. */

	ldr	x10, kexec_kimage_head		/* x10 = entry */

	/* Check if the new kernel needs relocation. */

	cbz	x10, .done
	tbnz	x10, IND_DONE_BIT, .done

	/* Setup loop variables. */

	mov	x12, xzr			/* x12 = ptr */
	mov	x13, xzr			/* x13 = dest */

.loop:
	/* addr = entry & PAGE_MASK */

	and	x14, x10, PAGE_MASK		/* x14 = addr */

	/* switch (entry & IND_FLAGS) */

.case_source:
	tbz	x10, IND_SOURCE_BIT, .case_indirection

	/* copy_page(x20 = dest, x21 = addr) */

	mov x20, x13
	mov x21, x14

	prfm	pldl1strm, [x21, #64]
1:	ldp	x22, x23, [x21]
	ldp	x24, x25, [x21, #16]
	ldp	x26, x27, [x21, #32]
	ldp	x28, x29, [x21, #48]
	add	x21, x21, #64
	prfm	pldl1strm, [x21, #64]
	stnp	x22, x23, [x20]
	stnp	x24, x25, [x20, #16]
	stnp	x26, x27, [x20, #32]
	stnp	x28, x29, [x20, #48]
	add	x20, x20, #64
	tst	x21, #(PAGE_SIZE - 1)
	b.ne	1b

	/* dest = PAGE_SIZE */

	add	x13, x13, PAGE_SIZE
	b	.next_entry

.case_indirection:
	tbz	x10, IND_INDIRECTION_BIT, .case_destination

	/* ptr = addr */

	mov	x12, x14
	b	.next_entry

.case_destination:
	tbz	x10, IND_DESTINATION_BIT, .next_entry

	/* dest = addr */

	mov	x13, x14

.next_entry:
	/* entry = *ptr */

	ldr	x10, [x12]
	add	x12, x12, 8

	/* while (!(entry & IND_DONE)) */

	tbz	x10, IND_DONE_BIT, .loop

.done:
	/* Jump to new kernel. */

	ldr	x0, kexec_dtb_addr
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr

	ldr	x4, kexec_kimage_start
	br	x4

.align 3

/* The machine_kexec routines set these variables. */

/*
 * kexec_signal_addr - Physical address of the spin signal variable.
 */

.globl kexec_signal_addr
kexec_signal_addr:
	.quad	0x0

/*
 * kexec_spinner_count - Count of spinning cpus.
 */

.globl kexec_spinner_count
kexec_spinner_count:
	.quad	0x0

/*
 * kexec_dtb_addr - The address of the new kernel's device tree.
 */

.globl kexec_dtb_addr
kexec_dtb_addr:
	.quad	0x0

/*
 * kexec_kimage_head - Copy of image->head, the list of kimage entries.
 */

.globl kexec_kimage_head
kexec_kimage_head:
	.quad	0x0

/*
 * kexec_kimage_start - Copy of image->start, the entry point of the new kernel.
 */

.globl kexec_kimage_start
kexec_kimage_start:
	.quad	0x0

.arm64_relocate_new_kernel_end:

/*
 * arm64_relocate_new_kernel_size - Byte count to copy to kimage control_code_page.
 */

.globl arm64_relocate_new_kernel_size
arm64_relocate_new_kernel_size:
	.quad .arm64_relocate_new_kernel_end - arm64_relocate_new_kernel
