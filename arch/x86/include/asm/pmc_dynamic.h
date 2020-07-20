/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_PMC_DYNAMIC_H
#define _ASM_X86_PMC_DYNAMIC_H

#include <asm/percpu.h>
#include <asm/msr-index.h>

#ifndef __ASSEMBLY__

#include <linux/pmc_dynamic.h>

struct gdb_track {
	unsigned stop;
	unsigned kin0;
	unsigned kin1;
	unsigned kout0;
	unsigned kout1;
};

DECLARE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_in);
DECLARE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_out);
DECLARE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_kside);

DECLARE_PER_CPU(struct gdb_track, pcpu_track);

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
#ifdef PMC_DYNAMIC_FULL
/*
 * @msr: msr address to read pmc value from
 * 
 * %rax contains the 64bit msr value
 * %rcx, %rax, %rdx are used by this macro
 */
#define msr_read(msr)					\
	movq	$##msr, %rcx;				\
	rdmsr;						\
	shlq	$0x20, %rdx;				\
	orq	%rdx, %rax		

/*
 * %rax contains the 64bit TSC value
 * %rcx, %rax, %rdx are used by this macro
 */
#define tsc_read					\
	rdtsc;						\
	shlq	$0x20, %rdx;				\
	orq	%rdx, %rax				

#define set_pmc_out(msr, pmc)						\
	msr_read(msr);								\
	/* Debug, move NEW to PMC0 */						\
	movq 	%rax, PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_pmc0);	\
	/* Snapshot_Kout - Snapshot_Kin */					\
	subq 	PER_CPU_VAR(pcpu_pmc_snapshot_in + PMC_SNAPSHOT_##pmc), %rax;	\
	/* Debug, move OLD to %rcx, then PMC1 */				\
	movq	PER_CPU_VAR(pcpu_pmc_snapshot_in + PMC_SNAPSHOT_##pmc), %rcx; 	\
	movq 	%rcx, PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_pmc1); \
	/* Debug, move DELTA to PMC2 */						\
	movq	%rax, PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_pmc2); \
	/* Add delta (%rax) to kernel counter */				\
	addq 	%rax, PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_##pmc);\
	/* Profile kernel in */							\
	incq 	PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_pmc3)


#define set_pmc_in(msr, pmc)						\
	msr_read(msr);								\
	/* Update pcpu_a */							\
	movq 	%rax, PER_CPU_VAR(pcpu_pmc_snapshot_in + PMC_SNAPSHOT_##pmc);	\
	/* Profile kernel in */							\
	incq 	PER_CPU_VAR(pcpu_pmc_snapshot_kside + PMC_SNAPSHOT_pmc_bitmap)
/*
 * @a: takes the last PMC value
 * @b: takes the previus PMC value
 * 
 * (in,out) -> entering the kernel mode (log to user)
 * (out,in) -> entering the user mode (log to kernel)
 */
#define log_pmc(type)								\
	pushq 	%rax;								\
	pushq 	%rcx;								\
	pushq 	%rdx;								\
	/* pushq 	%rdi; */								\
	/* movq 	PER_CPU_VAR(current_task), %rdi; */ 				\
	/* movq 	TASK_STRUCT_pmc_kernel(%rdi), %rdi;  */				\
	/* Read PMCs, Log pcpu PMCs value, Increment kvalue,  */		\
	set_pmc_##type(MSR_CORE_PERF_FIXED_CTR2, fixed2);			\
	/* set_pmc(MSR_CORE_PERF_FIXED_CTR1, fixed1, rdi, a, b); */			\
	/* set_pmc(MSR_CORE_PERF_FIXED_CTR0, fixed0, rdi, a, b); */			\
	/* set_pmc_out(MSR_CORE_PMC0, pmc0, rdi); */				\
	/* set_pmc_out(MSR_CORE_PMC1, pmc1, rdi); */				\
	/* set_pmc_out(MSR_CORE_PMC2, pmc2, rdi); */				\
	/* set_pmc_out(MSR_CORE_PMC3, pmc3, rdi); */				\
	/* Get the TSC value */							\
	/* popq 	%rdi; */								\
	popq 	%rdx;								\
	popq 	%rcx;								\
	popq 	%rax

#define pmc_snap_kernel_in	log_pmc(in)
#define pmc_snap_kernel_out	log_pmc(out)


#define gdb_track_in(x)								\
 	cmpl 	$1, PER_CPU_VAR(pcpu_track + GDB_TRACK_stop);			\
	je	oob##x;								\
	cmpl	$0, PER_CPU_VAR(pcpu_track + GDB_TRACK_kin0);			\
	jne 	ooa##x;								\
ooc##x:										\
	movl 	$x, PER_CPU_VAR(pcpu_track + GDB_TRACK_kin0);			\
	jmp	oob##x;								\
ooa##x:										\
	cmpl	$0, PER_CPU_VAR(pcpu_track + GDB_TRACK_kout0);			\
	jne	ooc##x;							\
	movl 	$x, PER_CPU_VAR(pcpu_track + GDB_TRACK_kin1);			\
	movl 	$1, PER_CPU_VAR(pcpu_track + GDB_TRACK_stop);			\
oob##x:										\
	nop


#define gdb_track_out(x)							\
	cmpl 	$1, PER_CPU_VAR(pcpu_track + GDB_TRACK_stop);			\
	je	iib##x;								\
	cmpl	$0, PER_CPU_VAR(pcpu_track + GDB_TRACK_kout0);			\
	jne 	iia##x;								\
iic##x:										\
	movl 	$x, PER_CPU_VAR(pcpu_track + GDB_TRACK_kout0);			\
	jmp	iib##x;								\
iia##x:										\
	cmpl	$0, PER_CPU_VAR(pcpu_track + GDB_TRACK_kin0);			\
	jne	iic##x;								\
	movl 	$x, PER_CPU_VAR(pcpu_track + GDB_TRACK_kout1);			\
	movl 	$1, PER_CPU_VAR(pcpu_track + GDB_TRACK_stop);			\
iib##x:										\
	nop
#else
#define pmc_snap_kernel_in	nop
#define pmc_snap_kernel_out	nop
#define gdb_track_in(x) 	nop
#define gdb_track_out(x) 	nop
#endif


#endif /* _ASM_X86_PMC_DYNAMIC_H */