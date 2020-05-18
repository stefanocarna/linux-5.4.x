#ifndef _PMC_DETECTION_H
#define _PMC_DETECTION_H

#include <asm-generic/int-ll64.h>
#include <asm/msr-index.h>
#include <linux/percpu.h>
#include <linux/sched.h>

/* Debug level */
#define DEBUG_OFF			0
#define DEBUG_NORMAL			BIT(0)
#define DEBUG_PROFILED			BIT(1)
#define DEBUG_DETECTED			BIT(2)
#define DEBUG_STUFF			BIT(3)
#define DEBUG_FORK			BIT(4)
#define DEBUG_HIGH			BIT(5)
/* */
#define DEBUG_SC_L3FLUSH		BIT(6)
#define DEBUG_SC_BUSYLOOP		BIT(7)
#define DEBUG_SC_MIGRATE		BIT(8)
#define DEBUG_TE_MITIGATE		BIT(9)



/* Detection state bits */
#define PMC_D_PROFILING			BIT(0) /* Enabling profiling on thread */
#define PMC_D_ERR_SANITIZE		BIT(1) /* Cannot allocate vmem while sanitizing */
#define PMC_D_REQ_PROFILING		BIT(2) /* Request profiling at next schedule */
#define PMC_D_PMC_INIT			BIT(3) /* Thread PMC content has been clean */
#define PMC_D_MIGRATED			BIT(4) /* This thread has been migrated */

#define PSB_MEMORY 			PAGE_SIZE
#define PSB_SAMPLES_MEM			(PSB_MEMORY - sizeof(struct pmc_sample_block))
#define PSB_SAMPLES_CNT			(PSB_SAMPLES_MEM / sizeof(struct pmc_sample))

/* Performance Event Select Register 0 */
#define MSR_CORE_PERFEVTSEL_ADDRESS	0x00000186
#define MSR_CORE_PERFEVTSEL(x)		(MSR_CORE_PERFEVTSEL_ADDRESS + x)
#define MSR_CORE_PERFEVTSEL0		MSR_CORE_PERFEVTSEL(0)
#define MSR_CORE_PERFEVTSEL1		MSR_CORE_PERFEVTSEL(1)
#define MSR_CORE_PERFEVTSEL2		MSR_CORE_PERFEVTSEL(2)
#define MSR_CORE_PERFEVTSEL3		MSR_CORE_PERFEVTSEL(3)
#define MSR_CORE_PERFEVTSEL4		MSR_CORE_PERFEVTSEL(4)
#define MSR_CORE_PERFEVTSEL5		MSR_CORE_PERFEVTSEL(5)
#define MSR_CORE_PERFEVTSEL6		MSR_CORE_PERFEVTSEL(6)
#define MSR_CORE_PERFEVTSEL7		MSR_CORE_PERFEVTSEL(7)

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

#define MAX_ID_PMC			4

#define PERF_GLOBAL_CTRL_FIXED1_BIT	33
#define PERF_GLOBAL_CTRL_FIXED1_SHIFT	BIT(PERF_GLOBAL_CTRL_FIXED1_BIT)

#define pd_read_fixed_pmc(n,v)		rdmsrl(MSR_CORE_PERF_FIXED_CTR##n, v)
#define pd_read_pmc(n,v)		rdmsrl(MSR_CORE_PMC(n), v)

#define pd_write_fixed_pmc(n,v)		wrmsrl(MSR_CORE_PERF_FIXED_CTR##n, v)
#define pd_write_pmc(n,v)		wrmsrl(MSR_CORE_PMC(n), v)

#define pd_read_ts_fixed_pmc(n,ts)	pd_read_fixed_pmc(n, ts->fixed##n)
#define pd_read_ts_pmc(n,ts)		pd_read_pmc(n, ts->pmc##n)

#define pd_write_ts_fixed_pmc(n,ts)	pd_write_fixed_pmc(n, ts->fixed##n)
#define pd_write_ts_pmc(n,ts)		pd_write_pmc(n, ts->pmc##n)

#define PMC_WRITE_MASK	(BIT(48) - 1)

DECLARE_PER_CPU(u64, pcpu_sampling_window);

struct pmc_cfg {
	union {
		u64 perf_evt_sel;
		struct {
			u64 evt: 8, umask: 8, usr: 1, os: 1, edge: 1, pc: 1, pmi: 1,
			any: 1, en: 1, inv: 1, cmask: 8, reserved: 32;
		};
	};
} __attribute__((packed));

struct pmc_sample {
	u64 fixed0;
	u64 fixed1;
	u64 pmc0;
	u64 pmc1;
	u64 pmc2;
	u64 pmc3;
} __attribute__((packed));

struct pmc_sample_block {
	struct pmc_sample *ps;

	unsigned cnt;
	unsigned max;

	struct pmc_sample_block *next;
} __attribute__((packed));

/*
 * PMC sample collected when the sampling is enabled.
 * l(ast)a(llocated)_ps_ctn: keeps track of the last number of samples
 */
struct detection_data {
	struct pmc_sample_block *psb;
	struct pmc_sample_block *psb_stored;
	struct pmc_sample_block *psb_stored_tail;

	unsigned psb_cnt;
} __attribute__((packed));

/* These the threshold values for the metric 1 and 2 */
extern s64 tm1;
extern s64 tm2;
extern u64 pmc_events[MAX_ID_PMC];

extern bool pp_mode;
extern unsigned debug_level;

extern void save_pmc_data(struct task_struct *ts);
extern void restore_pmc_data(struct task_struct *ts);

extern asmlinkage void pmc_detection_interrupt(void);
extern void pmc_detection_init(void);
extern void init_pmc_detection_proc(void);

extern void register_pid_dir_proc(pid_t *tgid, pid_t *pidp, struct detection_data *dd);

extern struct detection_data *init_pid_detection_data(struct task_struct *ts);

extern void set_debug_pmc_detection(unsigned level);
extern void set_tm1_pmc_detection(unsigned long val);

extern void set_sampling_window(u64 new_sw);
extern u64 get_sampling_window(void);

extern void set_sampling_events(void);

#endif /* _PMC_DETECTION_H */