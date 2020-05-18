#ifndef _SC_MITIGATIONS_H
#define _SC_MITIGATIONS_H

#include <linux/sched.h>

void migrate_suspected(struct task_struct *tsk, unsigned no_cpu);

void cpu_busy_loop(unsigned cpu_id);

void enter_busy_loop(void);

void exit_busy_loop(void);

bool is_busy_loop(void);

#endif /* _SC_MITIGATIONS_H */
