/* SPDX-License-Identifier: GPL-2.0 */
#ifndef	_ASM_X86_INTEL_PMC_WORKAROUND_H
#define	_ASM_X86_INTEL_PMC_WORKAROUND_H

#include <asm/percpu.h>
#include <asm/msr-index.h>

#define CPL_FILTER_GENERAL_PMC_OFFSET	8

/* Performance Event Counter Register 0 */
#define MSR_CORE_PMC_ADDRESS		MSR_IA32_PERFCTR0
#define MSR_CORE_PMC(x)			(MSR_CORE_PMC_ADDRESS + x)

#define READ_FIXED_PMC(x)		native_read_msr(MSR_CORE_PERF_FIXED_CTR0 + x)
#define READ_GENERAL_PMC(x)		native_read_msr(MSR_IA32_PERFCTR0 + x)

#ifdef __ASSEMBLY__

#define PMC_OFFSET(x)	x * SIZEOF_pmc

/**
 * %rax contains the 64bit TSC value
 * %rcx, %rax, %rdx are used by this macro
 */
#define tsc_read					\
	rdtsc;						\
	shlq	$0x20, %rdx;				\
	orq	%rdx, %rax

#define msr_read(msr)					\
	movq	$##msr, %rcx;				\
	rdmsr;						\
	shlq	$0x20, %rdx;				\
	orq	%rdx, %rax		

/* 
 * This macro saves the pmc value and, if required, updates the usr activity value with the 
 * delta from the last mode switch. It is used when entering or exiting the kernel mode.
 * @base: base msr for fixed of general pmc
 * @pmc: pmc that has to be saved
 * @type: enter or exit kernel mode
 */
.macro SINGLE_PMC_SNAPSHOT type base pmc
	/* 
	 * Skip this PMC if it is not active.
	 * pmc_cpl_filter_enabled is divided so:
	 * 0-7 bits: defines the active bit for fixed pmcs
	 * 8-63 bits: defines the active bit for general pmcs
	 */
	.Lpmc=\pmc
	.if \base == MSR_CORE_PERF_FIXED_CTR0
	btq	$(.Lpmc), pmc_cpl_filter_enabled
	.else
	.Lpmc=.Lpmc+CPL_FILTER_GENERAL_PMC_OFFSET
	btq	$(.Lpmc), pmc_cpl_filter_enabled
	.endif
	jz	.Lend_\@

	.if \type == entry
		/* Get old PMC i value from PCPU var [i] */
		movq	PER_CPU_VAR(pcpu_pmc_last_snapshot + PMCS_SNAPSHOT_pmcs + PMC_OFFSET(.Lpmc)), %rbx
	.endif
	
#ifdef CONFIG_HYPERVISOR_GUEST
	tsc_read
#else
	/* Read PMC i */
	msr_read(\base + \pmc)
#endif

	/* Save PMC i value into PCPU var [i] */
	movq	%rax, PER_CPU_VAR(pcpu_pmc_last_snapshot + PMCS_SNAPSHOT_pmcs + PMC_OFFSET(.Lpmc))

	.if \type == entry
		/* Compute the kernel time (delta) since last kernel exit (%rax) */
		subq 	%rbx, %rax
		/* Update USR activity with delta (%rax) */
		addq 	%rax, PER_CPU_VAR(pcpu_pmc_usr_since_ctx + PMCS_SNAPSHOT_pmcs + PMC_OFFSET(.Lpmc))
	.endif
.Lend_\@:
.endm

/* It uses %rax without saving it  */
.macro PMC_SNAPSHOT_AT_KERNEL type
	/* For each active pmc, create a snapshot */
	.if \type == entry
		pushq 	%rbx
	.endif
	pushq 	%rcx
	pushq 	%rdx

	.Lpmc=0
	.rept 4
	SINGLE_PMC_SNAPSHOT type=\type base=MSR_CORE_PERF_FIXED_CTR0 pmc=.Lpmc
	.Lpmc=.Lpmc+1
	.endr
	.Lpmc=0
	.rept 8
	SINGLE_PMC_SNAPSHOT type=\type base=MSR_CORE_PMC_ADDRESS pmc=.Lpmc
	.Lpmc=.Lpmc+1
	.endr

	popq 	%rdx
	popq 	%rcx
	.if \type == entry
		popq 	%rbx
	.endif
.endm

.macro PMC_SNAPSHOT_KERNEL type
	pushq 	%rax
	/* Check if there is some active PMC */
	movq	pmc_cpl_filter_enabled, %rax
	testq 	%rax, %rax
	jz 	.Lend_\@

	PMC_SNAPSHOT_AT_KERNEL type=\type
.Lend_\@:
	popq 	%rax
.endm

.macro PMC_SNAPSHOT_KERNEL_ENTRY
	/* THE PROBLEM IS HERE */
	PMC_SNAPSHOT_KERNEL type=entry
.endm

.macro PMC_SNAPSHOT_KERNEL_EXIT
	PMC_SNAPSHOT_KERNEL type=exit
.endm

#else /* ...!ASSEMBLY */

struct pmcs_snapshot {
	// u32 nr_fixed; /* TODO FUTURE For a dynamic allocation */
	// u32 nr_general; /* TODO FUTURE For a dynamic allocation */
	u64 pmcs[12]; // Assume 4 Fixed PMCs and 8 General PMCs
	// u64 *pmcs; /* TODO FUTURE For a dynamic allocation */
};

/** 
 * We have to keep track of the active pmcs to speedup the entry/exit kernel
 * routines. We fastly lookup at this variable to know which pmc has to be
 * snap shot.
 */
extern u64 pmc_cpl_filter_enabled;
// DECLARE_PER_CPU(u64, pcpu_pmc_cpl_filtered);

/**
 * At every Kernel Mode ENTRY/EXIT the cpu has to save the value of the active
 * pmcs to compute their contribute for both the execution modes (USR and OS).
 * Furthermore, a special snapshot has to be taken at context switch to compute
 * the final value once the thread is scheduled out.
 */
DECLARE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_last_snapshot);
DECLARE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_ctx_snapshot);

extern void pmc_cpl_filter_switch(void);
extern void enable_pmc_cpl_filter(void);
extern void disable_pmc_cpl_filter(void);

#endif /* __ASSEMBLY__ */

#endif /* _ASM_X86_INTEL_PMC_WORKAROUND_H */