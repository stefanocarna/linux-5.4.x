#include <linux/khooks.h>
#include <linux/spinlock.h>
// #include <linux/spinlock_types.h>
#include <linux/printk.h>
#include <linux/export.h>

// #include "../../include/linux/khooks.h"

unsigned long hook_places[LAST_HOOK];
unsigned hook_places_deleting[LAST_HOOK];
atomic_t hook_places_refcnt[LAST_HOOK];
raw_spinlock_t lock_hook;
// DEFINE_SPINLOCK(lock_hook);

int install_hook(unsigned long func, unsigned place, struct module *owner)
{
	int err = 0;
	unsigned long flags;

	if (place > LAST_HOOK) {
		pr_warn("Cannot install hook at place %u. Invalid\n", place);
		return -1;
	}

	raw_spin_lock_irqsave(&lock_hook, flags);
	if (!!hook_places[place]) {
		pr_warn("Cannot install hook at place %u. Busy\n", place);
		err = -1;
		goto end;
	}

	hook_places_deleting[place] = 0;
	hook_places_refcnt[place].counter = 0;
	// atomic_inc(&owner->refcnt);
	hook_places[place] = func;
	pr_warn("Hook at place %u. Installed\n", place);

end:
	raw_spin_unlock_irqrestore(&lock_hook, flags);
	return err;
}
EXPORT_SYMBOL(install_hook);

int uninstall_hook(unsigned long func, unsigned place, struct module *owner)
{
	int err = 0;
	unsigned long flags;

	hook_places_deleting[place] = 1;
	while(hook_places_refcnt[place].counter);

	if (place > LAST_HOOK) {
		pr_warn("Cannot uninstall hook at place %u. Invalid\n", place);
		return -1;
	}

	raw_spin_lock_irqsave(&lock_hook, flags);
	if (!hook_places[place]) {
		pr_warn("Cannot uninstall hook at place %u. Empty\n", place);
		err = -1;
		goto end;
	}

	if (hook_places[place] != func) {
		pr_warn("Cannot uninstall hook at place %u. Functions not matching\n", place);
		err = -1;
		goto end;
	}

	// atomic_dec_and_test(&owner->refcnt);
	hook_places[place] = EMPTY_HOOK;
	pr_warn("Hook at place %u. Uninstalled\n", place);
end:
	raw_spin_unlock_irqrestore(&lock_hook, flags);
	return err;
}
EXPORT_SYMBOL(uninstall_hook);
