#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lee Oswald");

static int __init erebuskm_init(void)
{
	printk("erebuskm: Hello, world!\n");
	return 0;
}

static void __exit erebuskm_exit(void)
{
	printk("erebuskm: Goodbye, world!\n");
}

module_init(erebuskm_init);
module_exit(erebuskm_exit);
