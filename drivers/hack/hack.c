#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/pmc_detection.h>

static __init int hack_init(void)
{
        pr_info("HACK module loaded\n");

        set_debug_pmc_detection(true);

	return 0;
}

void __exit hack_exit(void)
{
        set_debug_pmc_detection(false);
        pr_info("HACK module removed\n");
}

module_init(hack_init);
module_exit(hack_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna");