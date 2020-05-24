// #include <asm/pmc_dynamic.h>
#include <linux/pmc_dynamic.h>
#include <linux/interrupt.h>
#include <linux/fast_irq.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/irq_vectors.h>

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
	int k = 0;
	struct pmc_cfg cfg;

	/* Setup the LAPIC entry */
	apic_write(APIC_LVTPC, FAST_PMI);

	/* Fixed PMCs setup */
	/* IA32_FIXED_CTR0: INST_RETIRED.ANY */ 
	/* IA32_FIXED_CTR1: CPU_CLK_UNHALTED.THREAD */ 
	/* IA32_FIXED_CTR2: CPU_CLK_UNHALTED.REF_TSC */ 
	/* IA32_FIXED_CTR3: TOPDOWN.SLOTS */

	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, ACTIVE_FIXED_2 | ACTIVE_FIXED_1 | ACTIVE_FIXED_0 | ACTIVE_FIXED_PMI(1));

	// pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pd_write_fixed_pmc(0, 0ULL);
	pd_write_fixed_pmc(1, ~0xFFFFFF);
	pd_write_fixed_pmc(2, 0ULL);

	/* PMCs setup */
	cfg.usr = 1;
	cfg.os 	= 0;
	cfg.pmi = 0;
	cfg.en 	= 1;

	for(k = 0; k < MAX_ID_PMC; ++k){
		// pmc_events[k] |= cfg.perf_evt_sel;
		// pd_write_pmc(k, 0ULL);
		// wrmsrl(MSR_CORE_PERFEVTSEL(k), pmc_events[k]);
	}

	this_cpu_enable_pmc();
}

/* This function is called only by Core0 at boot time */
void pmc_dynamic_init(void)
{
	// Set pmi only once
	fast_vectors[0] = (unsigned long) pmi_dynamic;

	this_cpu_pmc_dynamic_init();	
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

	/* Reset fixed PMCs */
	// pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pr_info("[%u:%u] fast pmi\n", smp_processor_id(), current->pid);
	pd_write_fixed_pmc(0, ~0xFFFFFF);

no_pmi:
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, global);

	apic_write(APIC_LVTPC, FAST_PMI);
}// handle_ibs_event

void sync_pmc_state_on_cores()
{

}