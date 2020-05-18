#ifndef _DYNAMIC_PATCHES_H
#define _DYNAMIC_PATCHES_H

#include <linux/types.h> /* pid_t */
#include <linux/sched.h>

/* DEVELOP FUNCTIONS */
extern void set_flag_on_each_cpu(void *flag);

extern void suspect_task(struct task_struct *tsk);

/* trust_pid missing ??? */

extern bool is_suspected(struct mm_struct *mm);

extern void set_security_context(void);

#endif /* _DYNAMIC_PATCHES_H */