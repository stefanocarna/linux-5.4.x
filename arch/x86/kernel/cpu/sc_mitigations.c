#include <linux/sc_mitigations.h>
#include <linux/cpuset.h>
#include <linux/sched.h>
#include <linux/stop_machine.h>
#include <linux/percpu-defs.h>

DEFINE_PER_CPU(bool, busy_loop_cpus) = false;
DEFINE_PER_CPU(struct cpu_stop_work, cpu_stop_works);

void migrate_suspected(struct task_struct *tsk, unsigned no_cpu)
{
        int ret = 0;
	unsigned int cpu;
	cpumask_var_t cpu_mask;

        if (!tsk || (tsk->detection_state & PMC_D_MIGRATED)) goto err;

	if (!alloc_cpumask_var(&cpu_mask, GFP_KERNEL)) {
                ret = -1;
                goto err;
        }

        // Retrievee available cpus for tsk
	cpuset_cpus_allowed(tsk, cpu_mask);

        // Disable current cores and siblings
        for_each_cpu(cpu, topology_sibling_cpumask(no_cpu)) {
		if (cpu == no_cpu) {
			pr_info("CPU %u is DISABLED for PID %u\n", cpu, tsk->pid);
		}
		pr_info("CPU %u (SIBLING of CPU %u)is DISABLED for PID %u\n", no_cpu, cpu, tsk->pid);
		cpumask_clear_cpu(cpu, cpu_mask);
	}

	// do_set_cpus_allowed(tsk, cpu_mask);
	// set_tsk_need_resched(tsk);

        // ret = sched_setaffinity(tsk->pid, cpu_mask);
	if(ret) goto err_sched;

        tsk->detection_state |= PMC_D_MIGRATED;
	pr_warn("[PID %u : %s] scheduled away \n", tsk->pid, tsk->comm);
err_sched:
	free_cpumask_var(cpu_mask);
err:
	return; // ret;
}

static int __busy_loop(void *data) {
	unsigned cpu = (unsigned) data;
	while (per_cpu(busy_loop_cpus, cpu));
	return 1;
}

struct cpu_stop_work csw;

void cpu_busy_loop(unsigned cpu_id)
{
	if (smp_processor_id() == cpu_id) return;
	this_cpu_write(busy_loop_cpus, true);
	stop_one_cpu_nowait(cpu_id, __busy_loop, cpu_id, per_cpu_ptr(&cpu_stop_works, cpu_id));
	pr_info("[CPU %u] turn on busy loop on CPU %u\n", smp_processor_id(), cpu_id);
}

void enter_busy_loop(void)
{
	unsigned cpu, this_cpu = smp_processor_id();
        // Disable current cores and siblings
        for_each_cpu(cpu, topology_sibling_cpumask(this_cpu)) {
		if (cpu == this_cpu) continue;
		cpu_busy_loop(cpu);
	}
}

void exit_busy_loop(void)
{
	this_cpu_write(busy_loop_cpus, false);
	pr_info("[CPU %u] exiting busy_loop\n", smp_processor_id());
}

bool is_busy_loop(void)
{
	return this_cpu_read(busy_loop_cpus);
}