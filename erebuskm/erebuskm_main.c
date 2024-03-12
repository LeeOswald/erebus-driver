#include "erebuskm.h"
#include "shared.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/sched/cputime.h>
#include <linux/vmalloc.h>



#ifndef CONFIG_HAVE_SYSCALL_TRACEPOINTS
#error The kernel must have HAVE_SYSCALL_TRACEPOINTS
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lee Oswald");

static int erebuskm_open(struct inode *inode, struct file *filp);
static int erebuskm_release(struct inode *inode, struct file *filp);
static long erebuskm_ioctl(struct file *f, unsigned int cmd, unsigned long arg);


static const struct file_operations g_erebuskm_fops = {
	.open = erebuskm_open,
	.release = erebuskm_release,
	.unlocked_ioctl = erebuskm_ioctl,
	.owner = THIS_MODULE,
};

static int erebuskm_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int erebuskm_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long erebuskm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (cmd == ERK_IOCTL_GET_PROCESS_LIST) {

		struct erk_process_list *process_list = NULL;
		u32 memsize = 0;
		struct erk_process_list request;
		struct task_struct *process = NULL;
		struct task_struct *thread = NULL;
		u64 count = 0;

		// retrieve args
		if (copy_from_user(&request, (void *)arg, sizeof(request))) {
			ret = -EINVAL;
			goto ioctl_cleanup;
		}

		if (request.limit < 0 || request.limit > ERK_MAX_PROCESS_LIST)
		{
			er_verbose("ERK_IOCTL_GET_PROCESS_LIST: invalid limit %llu\n", request.limit);
			ret = -EINVAL;
			goto ioctl_cleanup_procinfo;
		}

		er_verbose("ERK_IOCTL_GET_PROCESS_LIST limit=%d\n", (int)request.limit);

		memsize = sizeof(struct erk_process_list) + sizeof(struct erk_process_info) * request.limit;
		process_list = vmalloc(memsize);
		if (!process_list) {
			ret = -ENOMEM;
			goto ioctl_cleanup;
		}

		process_list->limit = request.limit;

		// enumerate tasks
		rcu_read_lock();
		
		for_each_process_thread(process, thread) {

			u64 utime = 0;
			u64 stime = 0;

			task_cputime_adjusted(thread, &utime, &stime);
			process_list->entries[count].pid = thread->pid;
			process_list->entries[count].utime = nsec_to_clock_t(utime);
			process_list->entries[count].stime = nsec_to_clock_t(stime);
		}

		rcu_read_unlock();

		process_list->count = count;

		if (count >= process_list->limit) {
			er_verbose("ERK_IOCTL_GET_PROCESS_LIST: not enough space (%d avail, %d required)\n",
				(int)process_list->limit,
				(int)count);

			if (copy_to_user((void *)arg, process_list, sizeof(struct erk_process_list))) {
				ret = -EINVAL;
				goto ioctl_cleanup_procinfo;
			}

			ret = -ENOSPC;
			goto ioctl_cleanup_procinfo;
		}
		else {
			memsize = sizeof(struct erk_process_list) + sizeof(struct erk_process_info) * count;

			if (copy_to_user((void *)arg, process_list, memsize)) {
				ret = -EINVAL;
				goto ioctl_cleanup_procinfo;
			}
		}

		ret = 0;
ioctl_cleanup_procinfo:
		vfree((void *)process_list);
		goto ioctl_cleanup;
	}

ioctl_cleanup:
	return ret;
}


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
