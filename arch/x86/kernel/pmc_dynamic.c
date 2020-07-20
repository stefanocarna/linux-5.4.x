// #include <asm/pmc_dynamic.h>
#include <linux/pmc_dynamic.h>
#include <linux/interrupt.h>
#include <linux/fast_irq.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/irq_vectors.h>

struct gdb_track {
	unsigned stop;
	unsigned kin0;
	unsigned kin1;
	unsigned kout0;
	unsigned kout1;
};

DEFINE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_in) = { 0 };
EXPORT_SYMBOL(pcpu_pmc_snapshot_in);
DEFINE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_out) = { 0 };
EXPORT_SYMBOL(pcpu_pmc_snapshot_out);
DEFINE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_kside) = { 0 };
EXPORT_SYMBOL(pcpu_pmc_snapshot_kside);
DEFINE_PER_CPU(struct gdb_track, pcpu_track) = { 0 };
EXPORT_SYMBOL(pcpu_track);

void this_cpu_disable_pmc(void)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
}

void this_cpu_enable_pmc(void)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0xFULL | BIT(32) | BIT(33) | BIT(34));
}

/* This function is called just before the thread is switched out */
void save_pmc_data(struct task_struct *ts)
{
}

/* This function is called just before the thread is switched in */
void restore_pmc_data(struct task_struct *ts)
{
}

#define ACTIVE_FIXED(x)			((BIT(0) | BIT(1)) << (x * 4))
#define ACTIVE_FIXED_PMI(x)		(BIT(3) << (x * 4))
#define ACTIVE_FIXED_0			ACTIVE_FIXED(0)
#define ACTIVE_FIXED_1			ACTIVE_FIXED(1)
#define ACTIVE_FIXED_2			ACTIVE_FIXED(2)

void this_cpu_pmc_dynamic_init(void) {
#ifdef QEMU_DEBUG
	pr_info("QEMU DEBUG ON");
}
#else
	int k = 0;
	struct pmc_cfg cfg;

	/* Setup the LAPIC entry */
	apic_write(APIC_LVTPC, FAST_PMI);

	/* Fixed PMCs setup */
	/* IA32_FIXED_CTR0: INST_RETIRED.ANY */ 
	/* IA32_FIXED_CTR1: CPU_CLK_UNHALTED.THREAD */ 
	/* IA32_FIXED_CTR2: CPU_CLK_UNHALTED.REF_TSC */ 
	/* IA32_FIXED_CTR3: TOPDOWN.SLOTS */

	this_cpu_disable_pmc();

	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, ACTIVE_FIXED_2 | ACTIVE_FIXED_1 | ACTIVE_FIXED_0); // | ACTIVE_FIXED_PMI(2));
	// pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pd_write_fixed_pmc(0, 0ULL);
	pd_write_fixed_pmc(1, 0ULL);
	pd_write_fixed_pmc(2, 0ULL);

	/* PMCs setup */
	cfg.usr = 1;
	cfg.os 	= 1;
	cfg.pmi = 0;
	cfg.en 	= 1;

	for(k = 0; k < MAX_ID_PMC; ++k) {
		// pmc_events[k] |= cfg.perf_evt_sel;
		// pd_write_pmc(k, 0ULL);
		// wrmsrl(MSR_CORE_PERFEVTSEL(k), pmc_events[k]);
	}

	this_cpu_enable_pmc();
}
#endif

/* This function is called only by Core0 at boot time */
void pmc_dynamic_init(void)
{
	// Set pmi only once
	fast_vectors[0] = (unsigned long) pmi_dynamic;

	this_cpu_pmc_dynamic_init();	
}

static void take_pmc_snapshot(void)
{
	// TODO
}

/* This is a fast interrupt and does not take input parameters */
 __visible void pmi_dynamic(void)
{
	u64 global;

	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, global);

	/* This IRQ is not originated from PMC overflow */
	if(!(global & PERF_GLOBAL_CTRL_FIXED1_SHIFT)) {
		pr_warn("Something triggered pmc_detection_interrupt line\n\
		    MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", global);
		goto no_pmi;
	}

	/* 
	 * The current implementation of this function does not
	 * provide a sliding window for disceret samples collection.
	 * If a PMI arises, it means that there is a pmc multiplexing
	 * request. 	 
	 */ 

	/* Take a pmc sample snapshot */
	take_pmc_snapshot();

	/* Reset fixed PMCs */
	// pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pr_info("[%u:%u] fast pmi\n", smp_processor_id(), current->pid);
	pd_write_fixed_pmc(0, ~0xFFFFFF);

no_pmi:
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, global);

	apic_write(APIC_LVTPC, FAST_PMI);
}// handle_ibs_event


// DEFINE_PER_CPU(bool, busy_loop_cpus) = false;
// DEFINE_PER_CPU(struct cpu_stop_work, cpu_stop_works);

// void migrate_suspected(struct task_struct *tsk, unsigned no_cpu)
// {
//         int ret = 0;
// 	unsigned int cpu;
// 	cpumask_var_t cpu_mask;

//         if (!tsk || (tsk->detection_state & PMC_D_MIGRATED)) goto err;

// 	if (!alloc_cpumask_var(&cpu_mask, GFP_KERNEL)) {
//                 ret = -1;
//                 goto err;
//         }

//         // Retrievee available cpus for tsk
// 	cpuset_cpus_allowed(tsk, cpu_mask);

//         // Disable current cores and siblings
//         for_each_cpu(cpu, topology_sibling_cpumask(no_cpu)) {
// 		if (cpu == no_cpu) {
// 			pr_info("CPU %u is DISABLED for PID %u\n", cpu, tsk->pid);
// 		}
// 		pr_info("CPU %u (SIBLING of CPU %u)is DISABLED for PID %u\n", no_cpu, cpu, tsk->pid);
// 		cpumask_clear_cpu(cpu, cpu_mask);
// 	}

// 	// do_set_cpus_allowed(tsk, cpu_mask);
// 	// set_tsk_need_resched(tsk);

//         // ret = sched_setaffinity(tsk->pid, cpu_mask);
// 	if(ret) goto err_sched;

//         tsk->detection_state |= PMC_D_MIGRATED;
// 	pr_warn("[PID %u : %s] scheduled away \n", tsk->pid, tsk->comm);
// err_sched:
// 	free_cpumask_var(cpu_mask);
// err:
// 	return; // ret;
// }

// static int __busy_loop(void *data) {
// 	unsigned cpu = (unsigned) data;
// 	while (per_cpu(busy_loop_cpus, cpu));
// 	return 1;
// }

// struct cpu_stop_work csw;

// void cpu_busy_loop(unsigned cpu_id)
// {
// 	if (smp_processor_id() == cpu_id) return;
// 	this_cpu_write(busy_loop_cpus, true);
// 	stop_one_cpu_nowait(cpu_id, __busy_loop, cpu_id, per_cpu_ptr(&cpu_stop_works, cpu_id));
// 	pr_info("[CPU %u] turn on busy loop on CPU %u\n", smp_processor_id(), cpu_id);
// }

// void enter_busy_loop(void)
// {
// 	unsigned cpu, this_cpu = smp_processor_id();
//         // Disable current cores and siblings
//         for_each_cpu(cpu, topology_sibling_cpumask(this_cpu)) {
// 		if (cpu == this_cpu) continue;
// 		cpu_busy_loop(cpu);
// 	}
// }

// void exit_busy_loop(void)
// {
// 	this_cpu_write(busy_loop_cpus, false);
// 	pr_info("[CPU %u] exiting busy_loop\n", smp_processor_id());
// }

// bool is_busy_loop(void)
// {
// 	return this_cpu_read(busy_loop_cpus);
// }

static void sync_pmc_state_on_mate(void)
{
	unsigned cpu, this_cpu;

	preempt_disable();
	this_cpu = smp_processor_id();
	
	for_each_cpu(cpu, cpu_present_mask) {
		/* Skip current core */
		if (cpu == this_cpu) continue;
		// TODO all sync_function
	}

	preempt_enable();
}

static void sync_pmc_state_on_sibling(void)
{
	unsigned cpu, this_cpu;

	preempt_disable();
	this_cpu = smp_processor_id();
        
	// Disable current cores and siblings
        for_each_cpu(cpu, topology_sibling_cpumask(this_cpu)) {
		/* Skip current core */
		if (cpu == this_cpu) continue;
		// TODO all sync_function
	}

	preempt_enable();
}

void sync_pmc_state_on_cores()
{

}