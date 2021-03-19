#include <asm/current.h>
#include <asm/intel_pmc_workaround.h>
#include <asm/msr.h>

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h>

/**
 * TODO FUTURE
 * Make this values variables and fill them at startup according to the 
 * machine's characteristics.
 */
#ifdef CONFIG_HYPERVISOR_GUEST
#define NR_FIXED_PMCS   1
#define NR_GENERAL_PMCS 0
#else
#define NR_FIXED_PMCS   3
#define NR_GENERAL_PMCS 4
#endif

void pmc_stop_on_cpu(void *dummy)
{
	// per_cpu(pcpu_pmcs_active, get_cpu()) = false;
        get_cpu();
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
	put_cpu();
}

void pmc_start_on_cpu(void *dummy)
{
	// per_cpu(pcpu_pmcs_active, get_cpu()) = true;
        get_cpu();
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0xFFULL | BIT(32) | BIT(33) | BIT(34));
	put_cpu();
}


u64 pmc_cpl_filter_enabled = 0;

// DEFINE_PER_CPU(u64, pcpu_pmc_cpl_filtered) = 0;
EXPORT_SYMBOL(pmc_cpl_filter_enabled);

// This sturcts can be allocated at runtime upon cpl filtering activation
DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_os_activity);
DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_usr_activity);

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_last_snapshot);
DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_ctx_snapshot);
DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_usr_since_ctx);

bool struct_init = false;

static inline void reset_cpl_filter_on_cpu(void *pmcs_to_be_filtered_p)
{
        u64 pmc_ctr;
        unsigned pmc;
        unsigned pmcs_to_be_filtered = *(u64 *)pmcs_to_be_filtered_p;

        for (pmc = 0; pmc < NR_FIXED_PMCS; ++pmc) {
                if (pmcs_to_be_filtered & BIT_ULL(pmc)) {
                
                #ifdef CONFIG_HYPERVISOR_GUEST
                        pmc_ctr = (u64)rdtsc_ordered();
                #else
                        pmc_ctr = READ_FIXED_PMC(pmc);
                #endif

                        this_cpu_write(pcpu_pmc_usr_since_ctx.pmcs[pmc], 0);
                        this_cpu_write(pcpu_pmc_ctx_snapshot.pmcs[pmc], pmc_ctr);
                        this_cpu_write(pcpu_pmc_last_snapshot.pmcs[pmc], pmc_ctr);
                }
        }

        for (; pmc < NR_FIXED_PMCS + NR_GENERAL_PMCS; ++pmc) {
                if (pmcs_to_be_filtered & BIT_ULL(pmc)) {
                #ifdef CONFIG_HYPERVISOR_GUEST
                        pmc_ctr = (u64)rdtsc_ordered();
                #else
                        pmc_ctr = READ_GENERAL_PMC(pmc);
                #endif
                        this_cpu_write(pcpu_pmc_usr_since_ctx.pmcs[pmc], 0);
                        this_cpu_write(pcpu_pmc_ctx_snapshot.pmcs[pmc], pmc_ctr);
                        this_cpu_write(pcpu_pmc_last_snapshot.pmcs[pmc], pmc_ctr);
                }
        }
}

void enable_pmc_cpl_filter(void)
{
#ifndef CONFIG_HYPERVISOR_GUEST
        u64 pmcs = 1;
#endif
        if (!struct_init) {
                unsigned cpu, i;

                for_each_possible_cpu(cpu) {
                        for (i = 0; i < 12; ++i) {
                                this_cpu_write(pcpu_pmc_os_activity.pmcs[i], 0);
                                this_cpu_write(pcpu_pmc_usr_activity.pmcs[i], 0);
                                this_cpu_write(pcpu_pmc_last_snapshot.pmcs[i], 0);
                                this_cpu_write(pcpu_pmc_ctx_snapshot.pmcs[i], 0);
                                this_cpu_write(pcpu_pmc_usr_since_ctx.pmcs[i], 0);
                        }
                }
                
                struct_init = true;
        }

#ifndef CONFIG_HYPERVISOR_GUEST
        on_each_cpu(reset_cpl_filter_on_cpu, &pmcs, 1);
#endif

        pmc_cpl_filter_enabled = 1;
}
EXPORT_SYMBOL(enable_pmc_cpl_filter);

void disable_pmc_cpl_filter(void)
{
        pmc_cpl_filter_enabled = 0;
}
EXPORT_SYMBOL(disable_pmc_cpl_filter);

bool last = false;

static void single_pmc_filter_switch(unsigned pmc, bool general)
{
        u64 os;
        u64 last_pmc_value;
        
#ifdef CONFIG_HYPERVISOR_GUEST
        last_pmc_value = rdtsc_ordered();
#else
        if (general) {
                last_pmc_value = READ_GENERAL_PMC(pmc);
                /* Adjust the value according to the cpl_filterpcpu_pmc_usr_since_ctx convention */
                pmc += CPL_FILTER_GENERAL_PMC_OFFSET;
        } else {
                last_pmc_value = READ_FIXED_PMC(pmc);
        }
#endif

        // if (current->comm[0] == 's' && current->comm[2] == 'r' && current->comm[4] == 's') {
        //         last = true;
        //         goto end;
        // }

        // if (last) {
        //         last = false;
        // } else {
        //         goto end;
        // }

        /* Update the USR activity, it is always computed at the kernel entry */
        // this_cpu_write(pcpu_pmc_usr_activity.pmcs[pmc],
        //         this_cpu_read(pcpu_pmc_usr_since_ctx.pmcs[pmc]));
        this_cpu_add(pcpu_pmc_usr_activity.pmcs[pmc],
                this_cpu_read(pcpu_pmc_usr_since_ctx.pmcs[pmc]));

        /* Compute the OS activity value since the last mode switch */
        os = last_pmc_value - this_cpu_read(pcpu_pmc_last_snapshot.pmcs[pmc]);

        /* Update the OS activity since the last context switch */
        os += last_pmc_value -
                this_cpu_read(pcpu_pmc_ctx_snapshot.pmcs[pmc]) - 
                this_cpu_read(pcpu_pmc_usr_since_ctx.pmcs[pmc]);

        this_cpu_add(pcpu_pmc_os_activity.pmcs[pmc], os);
        // this_cpu_write(pcpu_pmc_os_activity.pmcs[pmc], os);

        pr_info("[%u] [PMC: %u] [OS: %llx, USR: %llx]\n *** CTX *** DELTA: %llx, OS: %llx, USR: %llx \n *** *** DELTA: %llx\n\n",
                current->pid,
                pmc,
                this_cpu_read(pcpu_pmc_os_activity.pmcs[pmc]),
                this_cpu_read(pcpu_pmc_usr_activity.pmcs[pmc]),

                last_pmc_value - this_cpu_read(pcpu_pmc_ctx_snapshot.pmcs[pmc]),
                os,
                this_cpu_read(pcpu_pmc_usr_since_ctx.pmcs[pmc]),
                last_pmc_value - this_cpu_read(pcpu_pmc_last_snapshot.pmcs[pmc]));

end:
        /* Reset the pcpu variable for the next context */
        this_cpu_write(pcpu_pmc_usr_since_ctx.pmcs[pmc], 0);
        this_cpu_write(pcpu_pmc_ctx_snapshot.pmcs[pmc], last_pmc_value);
        this_cpu_write(pcpu_pmc_last_snapshot.pmcs[pmc], last_pmc_value);
}

void pmc_cpl_filter_switch(void)
{
        unsigned pmc;

        if (!pmc_cpl_filter_enabled)
                return;

#ifndef CONFIG_HYPERVISOR_GUEST
        // TODO shutdown PMCs
        pmc_stop_on_cpu(NULL);
#endif

        for (pmc = 0; pmc < NR_FIXED_PMCS; ++pmc) {
                if (pmc_cpl_filter_enabled & BIT_ULL(pmc)) {
                        single_pmc_filter_switch(pmc, false);
                }
        }

        for (; pmc < NR_FIXED_PMCS + NR_GENERAL_PMCS; ++pmc) {
                if (pmc_cpl_filter_enabled & BIT_ULL(pmc)) {
                        single_pmc_filter_switch(pmc, true);
                }
        }

#ifndef CONFIG_HYPERVISOR_GUEST
        // TODO turn on PMCs
        pmc_start_on_cpu(NULL);
#endif
}