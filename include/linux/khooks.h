#ifndef _LINUX_KHOOKS_H
#define _LINUX_KHOOKS_H
/*
 *  Kernel Hooks (KHooks)
 *  include/linux/khooks.h
 *
 * 2020-Feb	Created by Stefan Carnà <carna@lockless.it> Kernel
 *		Hooks initial implementation.
 */
#include <linux/module.h>
#include <asm/atomic.h>

typedef int hook_func_t(void);

#define EMPTY_HOOK 0UL
#define HOOK_AFTER_CSWITCH 0
#define LAST_HOOK 1

extern unsigned long hook_places[LAST_HOOK];
extern unsigned hook_places_deleting[LAST_HOOK];
extern atomic_t hook_places_refcnt[LAST_HOOK];

#define place_hook(place) ({ 						\
	if (!!hook_places[place] && !hook_places_deleting[place]) { 	\
		arch_atomic_fetch_add(1, hook_places_refcnt + place);	\
		if (hook_places_deleting[place]) goto skip;		\
		(*(hook_func_t *)(hook_places[place]))();		\
	skip:								\
		arch_atomic_fetch_add(-1, hook_places_refcnt + place);	\
	}\
})

extern int install_hook(unsigned long func, unsigned place, struct module *owner);

extern int uninstall_hook(unsigned long func, unsigned place, struct module *owner);

#endif /* _LINUX_KHOOKS_H */