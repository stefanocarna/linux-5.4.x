#ifndef _PMC_DYNAMIC_H
#define _PMC_DYNAMIC_H

#include <asm-generic/int-ll64.h>
#include <asm/msr-index.h>
#include <linux/percpu.h>
#include <linux/sched.h>
// #include <asm/pmc_dynamic.h>

#define event_umask(event, umask)	(x | (y << 8))
/* Define events and related bit */
#define LLC_Reference			event_umask(0x2e, 0x4f)
#define LLC_Misses			event_umask(0x2e, 0x41)
#define MEM_LOAD_RETIRED_L1_HIT		event_umask(0xd1, 0x01)
#define MEM_LOAD_RETIRED_L2_HIT		event_umask(0xd1, 0x02)
#define MEM_LOAD_RETIRED_L3_HIT		event_umask(0xd1, 0x04)
#define MEM_LOAD_RETIRED_L1_MISS	event_umask(0xd1, 0x08)
#define MEM_LOAD_RETIRED_L2_MISS	event_umask(0xd1, 0x10)
#define MEM_LOAD_RETIRED_L3_MISS	event_umask(0xd1, 0x20)

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

#define MAX_ID_PMC			8

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

#define PMC_WRITE_MASK			(BIT(48) - 1)

struct pmc_cfg {
	union {
		u64 perf_evt_sel;
		struct {
			u64 evt: 8, umask: 8, usr: 1, os: 1, edge: 1, pc: 1, pmi: 1,
			any: 1, en: 1, inv: 1, cmask: 8, reserved: 32;
		};
	};
} __attribute__((packed));

struct pmc_snapshot {
	pid_t pid;
	u64 tsc;
	u64 fixed0;
	u64 fixed1;
	u64 fixed2;
	u64 pmc0;
	u64 pmc1;
	u64 pmc2;
	u64 pmc3;
	u64 pmc4;
	u64 pmc5;
	u64 pmc6;
	u64 pmc7;
} __attribute__((packed));

extern void save_pmc_data(struct task_struct *ts);
extern void restore_pmc_data(struct task_struct *ts);

extern asmlinkage void pmi_dynamic(void);
extern void pmc_dynamic_init(void);
extern void this_cpu_pmc_dynamic_init(void);

extern void sync_pmc_state_on_cores(void); 

#endif /* _PMC_DYNAMIC_H */