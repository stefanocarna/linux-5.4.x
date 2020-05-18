#include <asm/dynamic_patches.h>
#include <linux/dynamic_patches.h>
#include <linux/sched/coredump.h>	/* MMF_HAS_SUSPECTED_MASK */
#include <linux/gfp.h>

#include <asm/pgtable.h>
#include <asm/cpufeatures.h>

DEFINE_PER_CPU(u32, cpu_dynamic_patches) = 0;
EXPORT_SYMBOL(cpu_dynamic_patches);

void set_flag_on_each_cpu(void *flag)
{
	unsigned long bit = *((unsigned long*) flag);
	this_cpu_write(cpu_dynamic_patches, this_cpu_read_stable(cpu_dynamic_patches) | (1UL << bit));
}
EXPORT_SYMBOL(set_flag_on_each_cpu);

void set_resched(void *mm) {
	if (mm && current->mm == (struct mm_struct*) mm) {
		pr_warn("[PID %u] Dyn KPTI required resched on CPU %u\n", current->pid, smp_processor_id());
		set_tsk_need_resched(current);
	}
}

/**
 * pti_enable - enable the PTI mechanism for a given pid
 * @pid: the pid we classified as suspected
 */
static void pti_enable(struct task_struct *tsk) //pid_t pid)
{
	/* Kernel View PGD */
	// pgd_t *k_pgdp;
	cpumask_var_t cpu_mask;

	// BUG_ON((pid != current->pid) && !current->mm);

	/* Lock all other CPUs */
	// stop_all_cpus_but_this();

	if (!alloc_cpumask_var(&cpu_mask, GFP_KERNEL)) return;

	cpumask_full(cpu_mask);
	cpumask_clear_cpu(smp_processor_id(), cpu_mask);

	/* Set the SUSPECTED BIT in mm->flags */
	current->mm->flags |= MMF_HAS_SUSPECTED_MASK;

	on_each_cpu_mask(cpu_mask, set_resched, tsk->mm, 1);

	/* Until now this thread always worked on kernel view */
	// k_pgdp = tsk->mm->pgd;

	/* Scan for all present pgd entries and set the NX bit */
	// while (pgdp_maps_userspace(k_pgdp)) {

	// 	if ((k_pgdp->pgd & (_PAGE_USER | _PAGE_PRESENT)) ==
	// 	    (_PAGE_USER | _PAGE_PRESENT) &&
	// 	    (__supported_pte_mask & _PAGE_NX))
	// 	    	k_pgdp->pgd |= _PAGE_NX;

	// 	++k_pgdp;
	// }


	pr_warn("[PID %u : %s] Dyn KPTI enabled\n", tsk->pid, tsk->comm);

	/* Unlock all other CPUs */
	// resume_all_cpus();
}

static void set_ssb_mitigation(bool enable)
{
	u64 msr;

	if (!boot_cpu_has(X86_FEATURE_MSR_SPEC_CTRL)) return;

	rdmsrl(MSR_IA32_SPEC_CTRL, msr);

	if (enable) {
		msr |= SPEC_CTRL_SSBD;
	} else {
		msr &= ~SPEC_CTRL_SSBD;
	}
	wrmsrl(MSR_IA32_SPEC_CTRL, msr);
}

/**
 * This function is called when a PID is classified as suspected
 * Here all the security mitigation patches are enabled
 * according to the CPU's capabilities.
 */
static void enable_bug_patches(struct task_struct *tsk)
{
	/* Enable PTI */
	pti_enable(tsk);



}

static void set_bug_patches(bool enable)
{
	unsigned flags = 0;
	/* Store Bypass mitigation */
	set_ssb_mitigation(enable);

	// TODO Check this

	flags = DCP_RETPOLINE_SHIFT | DCP_RSB_CTXSW_SHIFT |
	    DCP_USE_IBPB_SHIFT | DCP_COND_STIBP_SHIFT | DCP_COND_IBPB_SHIFT |
	    DCP_ALWAYS_IBPB_SHIFT;
	/* Enable dynamic mitigations */
	set_flag_on_each_cpu(&flags);
}

static void enter_trusted_context(void)
{
	set_bug_patches(false);
}

static void enter_suspect_context(void)
{
	set_bug_patches(true);
}

bool is_suspected(struct mm_struct *mm)
{
	return mm && (mm->flags & MMF_HAS_SUSPECTED_MASK);
}

void suspect_task(struct task_struct *tsk)
{
	// TODO this should activate the other patches is the current thread is the one suspected
	if (tsk && !is_suspected(tsk->mm)) {
		enable_bug_patches(tsk);
		pr_warn("[PID %u : %s] is suspected as malicious process\n", tsk->pid, tsk->comm);
	}
}

/**
 * According to the current pid classification
 * enable or not the mitigations
 */
void set_security_context(void)
{
	if (is_suspected(current->mm))
		enter_suspect_context();
	enter_trusted_context();
}