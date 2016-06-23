/*
 * cpu reset routines
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2015 Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/cpufeature.h>
#include <asm/alternative.h>
#include <asm/sysreg.h>
#include <asm/virt.h>
.text
.pushsection    .idmap.text, "ax"
.align 5




/*
 * __cpu_soft_restart(el2_switch, entry, arg0, arg1, arg2) - Helper for
 * cpu_soft_restart.
 *
 * @el2_switch: Flag to indicate a swich to EL2 is needed.
 * @entry: Location to jump to for soft reset.
 * arg0: First argument passed to @entry.
 * arg1: Second argument passed to @entry.
 * arg2: Third argument passed to @entry.
 *
 * Put the CPU into the same state as it would be if it had been reset, and
 * branch to what would be the reset vector. It must be executed with the
 * flat identity mapping.
 */
ENTRY(__cpu_soft_restart)
	/* Clear sctlr_el1 flags. */
	/* Save address of cpu_reset() and reset address */
	mov	x19, x0
	mov	x20, x1

	/* Turn Dcache off */
	bl	cpu_cache_off

	/* Push out all dirty data, and ensure cache is empty */
	bl	flush_cache_all

	mov	x0, x20
	ret	x19
ENDPROC(__cpu_soft_restart)

.popsection
