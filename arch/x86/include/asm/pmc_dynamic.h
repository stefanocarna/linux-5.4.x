/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_PMC_DYNAMIC_H
#define _ASM_X86_PMC_DYNAMIC_H

#include <asm/percpu.h>
#include <asm/msr-index.h>

#ifndef __ASSEMBLY__

// DECLARE_PER_CPU(u32, cpu_dynamic_patches);

// #define check_cpu_dynamic_flag(f) (!!(*this_cpu_ptr(&(cpu_dynamic_patches)) & f))

#endif /* __ASSEMBLY__ */

/* Performance Event Counter Register 0 */
#define MSR_CORE_PMC_ADDRESS		0x000000C1
#define MSR_CORE_PMC(x)			(MSR_CORE_PMC_ADDRESS + x)
#define MSR_CORE_PMC0			MSR_CORE_PMC(0)
#define MSR_CORE_PMC1			MSR_CORE_PMC(1)
#define MSR_CORE_PMC2			MSR_CORE_PMC(2)
#define MSR_CORE_PMC3			MSR_CORE_PMC(3)
#define MSR_CORE_PMC4			MSR_CORE_PMC(4)
#define MSR_CORE_PMC5			MSR_CORE_PMC(5)
#define MSR_CORE_PMC6			MSR_CORE_PMC(6)
#define MSR_CORE_PMC7			MSR_CORE_PMC(7)

/* ASM macro */

/*
 * @msr: msr address to read pmc value from
 * @var: var address to store read data
 * 
 * %rcx, %eax, %edx are used by this macro
 */
#ifndef QEMU_DEBUG
#define pmc_save(msr, loc)				\
	movq	$##msr, %rax;				\
	movq 	$0, %rdx;				\
	shlq	$0x20, %rdx;				\
	orq	%rax, %rdx;				\
	movq	%rax, loc
#else
#define pmc_save(msr, loc)				\
	movq	$##msr, %rcx;				\
	rdmsr;						\
	shlq	$0x20, %rdx;				\
	orq	%rax, %rdx;				\
	movq	%rax, loc
#endif

/* Save each PMC value */
/* TODO pmc_bitmap - PMC_SNAPSHOT_pmc_bitmap */
#define pmc_snapshot(type)						\
	pushq 	%rax;							\
	pushq 	%rcx;							\
	pushq 	%rdx;							\
	pushq 	%rdi;							\
	mov 	PER_CPU_VAR(current_task), %rdi; 			\
	mov 	TASK_STRUCT_pmc_##type(%rdi), %rdi; 			\
	pmc_save(MSR_CORE_PERF_FIXED_CTR0, PMC_SNAPSHOT_fixed0(%rdi));	\
	pmc_save(MSR_CORE_PERF_FIXED_CTR1, PMC_SNAPSHOT_fixed1(%rdi));	\
	pmc_save(MSR_CORE_PERF_FIXED_CTR2, PMC_SNAPSHOT_fixed2(%rdi));	\
	pmc_save(MSR_CORE_PMC0, PMC_SNAPSHOT_pmc0(%rdi));		\
	pmc_save(MSR_CORE_PMC1, PMC_SNAPSHOT_pmc1(%rdi));		\
	pmc_save(MSR_CORE_PMC2, PMC_SNAPSHOT_pmc2(%rdi));		\
	pmc_save(MSR_CORE_PMC3, PMC_SNAPSHOT_pmc3(%rdi));		\
	popq 	%rdi; 							\
	popq 	%rdx;							\
	popq 	%rcx;							\
	popq 	%rax

#define pmc_snapshot_user	pmc_snapshot(user)
#define pmc_snapshot_kernel	pmc_snapshot(kernel)

#endif /* _ASM_X86_PMC_DYNAMIC_H */