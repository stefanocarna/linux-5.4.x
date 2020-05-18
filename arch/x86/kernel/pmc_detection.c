#include <linux/pmc_detection.h>
#include <linux/sc_mitigations.h>
#include <linux/dynamic_patches.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/irq_vectors.h>

DEFINE_PER_CPU(u64, pcpu_sampling_window) = (~0xFFFFFF & PMC_WRITE_MASK);

u64 pmc_events[MAX_ID_PMC] = {0x81D0, 0x20D1, 0x8d1, 0x2008};

s64 tm1 = 0;
s64 tm2 = 20;

bool pp_mode = true;

unsigned debug_level = DEBUG_OFF;

void this_cpu_disable_pmc(void);
void this_cpu_enable_pmc(void);

#define DEFAULT_BLOCKS_PER_SESSION	20
#define PRECISION			1000LL

static void set_sampling_events_on_cpu(void *arg)
{
	pmc_detection_init();
}

void set_sampling_events(void)
{
	on_each_cpu(set_sampling_events_on_cpu, NULL, 1);
}
EXPORT_SYMBOL(set_sampling_events);

static void set_sampling_window_on_cpu(void *sw)
{
	u64 nsw = ((u64)sw) & PMC_WRITE_MASK;
	this_cpu_write(pcpu_sampling_window, nsw);
}

void set_sampling_window(u64 new_sw)
{
	on_each_cpu(set_sampling_window_on_cpu, (void*)(~new_sw), 1);
}
EXPORT_SYMBOL(set_sampling_window);

u64 get_sampling_window(void)
{
	return (~this_cpu_read(pcpu_sampling_window)) & PMC_WRITE_MASK;
}
EXPORT_SYMBOL(get_sampling_window);

/*
 * Used to allocate memory for pmc_samples at context switch to avoid
 * the memory allocation during pmc interrupt handling. We use some params
 * to dynamiclly adjust the amount of allocated memory
 */
bool prealloc_memory_sample(struct task_struct *ts)
{
	unsigned long required_psb = 0;
	struct pmc_sample_block *new_psb, *tmp_psb;
	struct detection_data *ddp = ts->detection_data;

	BUG_ON(!ddp);

	/* Consitency check */
	if (!ddp->psb && ddp->psb_cnt) {
		pr_warn("BUG @pid:%u ddp->psb:%llx ddp->psb_cnt:%u\n",
		    ts->pid, (u64)ddp->psb, ddp->psb_cnt);
	}
	// BUG_ON(!ddp->psb && ddp->psb_cnt);

	required_psb = DEFAULT_BLOCKS_PER_SESSION - ddp->psb_cnt;

	tmp_psb = ddp->psb;
	if (!tmp_psb) goto skip_iter;
	/* Advance to the right block */
	for (; tmp_psb->next; tmp_psb = tmp_psb->next);

skip_iter:
	/* Populate block list */
	while (required_psb--) {
		new_psb = vmalloc(PSB_MEMORY);
		if (!new_psb) goto err;

		new_psb->ps = (struct pmc_sample*)(&new_psb[1]);
		new_psb->cnt = 0;
		new_psb->max = PSB_SAMPLES_CNT;
		new_psb->next = NULL;

		/* Link block and increase pointer */
		if (!tmp_psb)
			ddp->psb = new_psb;
		else
			tmp_psb->next = new_psb;

		tmp_psb = new_psb;
	}

	/* Adjust the blocks counter */
	ddp->psb_cnt = DEFAULT_BLOCKS_PER_SESSION;

	return true;
err:
	/*
	 * Clean up the available blocks but the first.
	 * That one may contain some valid sample
	 */
	if (ddp->psb) {
		new_psb = ddp->psb->next;
		while (new_psb) {
			tmp_psb = new_psb;
			new_psb = new_psb->next;
			vfree(tmp_psb);
		}
	}
	return false;
}

/* This function is called just before the thread is switched out */
void save_pmc_data(struct task_struct *ts)
{
	pd_read_ts_fixed_pmc(0, ts);
	pd_read_ts_fixed_pmc(1, ts);

	pd_read_ts_pmc(0, ts);
	pd_read_ts_pmc(1, ts);
	pd_read_ts_pmc(2, ts);
	pd_read_ts_pmc(3, ts);

}

/* This function is called just before the thread is switched in */
void restore_pmc_data(struct task_struct *ts)
{
	u64 sw = this_cpu_read(pcpu_sampling_window);

	pd_write_ts_fixed_pmc(0, ts);

	if (ts->fixed1 > sw)
		pd_write_ts_fixed_pmc(1, ts);
	else
		pd_write_fixed_pmc(1, sw);

	pd_write_ts_pmc(0, ts);
	pd_write_ts_pmc(1, ts);
	pd_write_ts_pmc(2, ts);
	pd_write_ts_pmc(3, ts);


	// pr_warn("@BEFORE allocate memory for pid %u, on cpu %u. Profiling OFF [State: %x]\n", ts->pid, smp_processor_id(), ts->detection_state);

	if ((ts->detection_state & PMC_D_PROFILING) &&
	    !prealloc_memory_sample(ts)) {
		/* Profiling shutdown */
		ts->detection_state &= ~PMC_D_PROFILING;
		pr_warn("Impossible to allocate memory for pid %u, on cpu %u.\
			Profiling OFF [State: %x]\n", ts->pid, smp_processor_id(), ts->detection_state);
	}
}

void pmc_detection_init(void)
{
	int k = 0;
	struct pmc_cfg cfg;

	/* Setup the LAPIC entry */
	apic_write(APIC_LVTPC, FAST_PMI);

	/* Fixed PMCs setup */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, BIT(1) | BIT(5) | BIT(7));
	pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pd_write_fixed_pmc(0, 0ULL);

	/* PMCs setup */
	cfg.usr = 1;
	cfg.os 	= 0;
	cfg.pmi = 0;
	cfg.en 	= 1;

	for(k = 0; k < MAX_ID_PMC; ++k){
		pmc_events[k] |= cfg.perf_evt_sel;
		pd_write_pmc(k, 0ULL);
		wrmsrl(MSR_CORE_PERFEVTSEL(k), pmc_events[k]);
	}

	this_cpu_enable_pmc();
}

void this_cpu_disable_pmc(void)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
}

void this_cpu_enable_pmc(void)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0xFULL | BIT(32) | BIT(33) | BIT(34));
}

void set_debug_pmc_detection(unsigned level)
{
	debug_level = level;
}
EXPORT_SYMBOL(set_debug_pmc_detection);

void set_tm1_pmc_detection(unsigned long val)
{
	tm1 = val;
}
EXPORT_SYMBOL(set_tm1_pmc_detection);

struct detection_data *init_pid_detection_data(struct task_struct *ts)
{
	struct detection_data *dd;

	if (!ts || (ts->detection_state & PMC_D_PROFILING))
		goto err;

	dd = vzalloc(sizeof(struct detection_data));
	if (!dd) goto err;

	// dd->psb_kernel = vmalloc(EXTRA_MEMORY);
	// if (!dd->psb_kernel) goto no_psb;

	// dd->psb_kernel->ps = (struct pmc_sample*)(&dd[1]);
	// dd->psb_kernel->cnt = 0;
	// dd->psb_kernel->max = EXTRA_SAMPLES;
	// dd->psb_kernel->next = NULL;

	ts->detection_data = dd;

	/* Flag and create an entry */
	ts->detection_state |= PMC_D_REQ_PROFILING;

	return dd;

// no_psb:
// 	vfree(dd);
err:
	return NULL;
}

/* This is a fast interrupt and does not take input parameters */
 __visible void pmc_detection_interrupt(void)
{
	int i;
	s64 m1 = 0, m2 = 0; //, m3 = 0, m4 = 0;
	u64 global;
	u64 pmcs[MAX_ID_PMC];
	bool suspected = false;
	struct pmc_sample *sample;
	struct detection_data *ddp;
	struct pmc_sample_block *psbp;

	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, global);

	/* This IRQ is not originated from PMC overflow */
	if(!(global & PERF_GLOBAL_CTRL_FIXED1_SHIFT)) {
		pr_warn("Something triggered pmc_detection_interrupt line\n\
		    MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", global);
		goto no_pmi;
	}

	/* Log and Reset PMCs */
	for(i = 0; i < MAX_ID_PMC; i++) {
		pd_read_pmc(i, pmcs[i]);
		pd_write_pmc(i, 0ULL);
	}

	/* Check if sampling is enabled */
	if (!(current->detection_state & PMC_D_PROFILING))
		goto samp_off;

	if (debug_level & DEBUG_NORMAL) {
		pr_info("DEBUG [%u] Collecting sample", current->pid);
	}

	ddp = current->detection_data;
	/* This is never true (context switch populates it) */
	BUG_ON(!ddp);

	/* This shuld not happen */
	psbp = ddp->psb;
	if(!psbp) {
		pr_warn("Cannot collect sample: #cpu:%u @pid:%u\n",
		    smp_processor_id(), current->pid);
		return;
	}

	/* You cannot have a full block here */
	BUG_ON(psbp->cnt >= psbp->max);

	sample = psbp->ps + psbp->cnt;
	psbp->cnt++;


	/* Log fixed PMCs */
	pd_read_fixed_pmc(0, sample->fixed0);
	// This does not make sense, it is always ~0
	pd_read_fixed_pmc(1, sample->fixed1);
	sample->pmc0 = pmcs[0];
	sample->pmc1 = pmcs[1];
	sample->pmc2 = pmcs[2];
	sample->pmc3 = pmcs[3];

	/*
	 * Use extra var @psbp because someone may be reading, thus
	 * we avoid a possible read of empty blocks
	 */
	if (psbp->cnt == psbp->max) {

		if (debug_level & DEBUG_NORMAL) {
			pr_info("DEBUG [%u] Block filled", current->pid);
		}

		/* Extract the full block and cut link */
		psbp = ddp->psb;
		ddp->psb = psbp->next;
		psbp->next = NULL;

		/* Decrese the number of blocks */
		ddp->psb_cnt--;

		if (!ddp->psb_stored_tail) {
			ddp->psb_stored = psbp;
			ddp->psb_stored_tail = psbp;
		} else {
			ddp->psb_stored_tail->next = psbp;
			ddp->psb_stored_tail = ddp->psb_stored_tail->next;
		}
	}

samp_off:
	/* Reset fixed PMCs */
	pd_write_fixed_pmc(1, this_cpu_read(pcpu_sampling_window));
	pd_write_fixed_pmc(0, 0ULL);

	/* %TLB Hit  */
	// m1 = PRECISION - (((s64) pmcs[3]) * PRECISION) / ((s64)pmcs[0] + 1);

	/* %Cache Hit  */
	// m2 = PRECISION - (((s64) pmcs[1]) * PRECISION) / ((s64)pmcs[0] + 1);

	// if (pp_mode && m1 < tm1 && m2 > tm2) {
	// 	pr_warn("[@DETECTED %u] ALLload:%llu, L1miss:%llu, L3miss:%llu, TLBmiss:%llu, MET1:%lld, MET2:%lld, TM1:%llu, TM2:%llu\n",
	// 	    current->pid, pmcs[0], pmcs[2], pmcs[1], pmcs[3], m1, m2, tm1, tm2);
	// }

	// Already supected
	if (is_suspected(current->mm)) goto skip_det;

	m1 = (((s64) pmcs[3] - (s64) pmcs[1])*100)/((s64)pmcs[0]+1);

	m2 = (((s64) pmcs[2] - (s64) pmcs[1])*100)/((s64)pmcs[0]+1);

	if ((current->detection_state & PMC_D_PROFILING) &&
	    (debug_level & DEBUG_PROFILED)) {
		pr_info("[@DEBUG %u] LOADS:%llu, L1_%s:%llu, L3_MISS:%llu, TLB_MISS:%llu, M1:%lld, M2:%lld, TM1:%llu, TM2:%llu\n",
		    current->pid, pmcs[0], pp_mode ? "MISS" : "REPL", pmcs[2], pmcs[1], pmcs[3], m1, m2, tm1, tm2);
	}

	if (debug_level & DEBUG_HIGH) {
		pr_info("DEBUG [%u] BRMiss:%llu, BRCon:%llu, BR:%llu, MET1:%llu\n",
		    current->pid, pmcs[0], pmcs[1], pmcs[3], m1);
	}

	if (pp_mode && m1 < tm1 && m2 > tm2) {
		pr_warn("[@SUSPECT %u] ALLload:%llu, L1miss:%llu, L3miss:%llu, TLBmiss:%llu, MET1:%lld, MET2:%lld, TM1:%llu, TM2:%llu\n",
		    current->pid, pmcs[0], pmcs[2], pmcs[1], pmcs[3], m1, m2, tm1, tm2);
		    suspected = true;
	}

	if (!pp_mode && m1 < tm1 && m2 < tm2) {
		pr_warn("[@SUSPECT %u] ALLload:%llu, L1miss:%llu, L3miss:%llu, TLBmiss:%llu, MET1:%lld, MET2:%lld, TM1:%llu, TM2:%llu\n\n",
		    current->pid, pmcs[0], pmcs[2], pmcs[1], pmcs[3], m1, m2, tm1, tm2);
		    suspected = true;
	}

	/**
	 * Enables countermeasures:
	 * 1. Migrate current to another core
	 * 2. Transient Execution Attacks' mitigations
	 */
	if (suspected) {

		current->detection_state |= PMC_D_ERR_SANITIZE;
		pr_warn("[PID %u : %s] flagged as suspected\n", current->pid, current->comm);

		set_tsk_need_resched(current);
	}

skip_det:
no_pmi:
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, global);

	apic_write(APIC_LVTPC, FAST_PMI);
}// handle_ibs_event
