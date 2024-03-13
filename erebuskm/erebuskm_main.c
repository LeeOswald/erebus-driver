#include "erebuskm.h"
#include "shared.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/sched/cputime.h>
#include <linux/sched/signal.h>
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

static int g_major = -1;
LIST_HEAD(g_client_list);
static DEFINE_MUTEX(g_client_mutex);

static struct er_client_t *erk_find_client(struct task_struct *thread)
{
	struct er_client_t *client = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(client, &g_client_list, node) {
		if (client->client_thread == thread) {
			rcu_read_unlock();
			return client;
		}
	}
	rcu_read_unlock();

	return NULL;
}

static struct er_client_t *erk_allocate_client(struct task_struct *thread)
{
	struct er_client_t *client = NULL;
	
	er_verbose("Allocating clinet %p\n", thread);

	client = vmalloc(sizeof(struct er_client_t));
	if (!client) {
		pr_err("Failed to allocate a new client\n");
		return NULL;
	}

	client->client_thread = thread;

	return client;
}

static void erk_remove_client(struct er_client_t *client)
{
	er_verbose("Deallocating client %p\n", client->client_thread);

	list_del_rcu(&client->node);
	synchronize_rcu();
	
	vfree(client);
}


static int erebuskm_open(struct inode *inode, struct file *filp)
{
	int ret = -ENOTSUPP;
	struct task_struct *current_thread = current;
	struct er_client_t *client = NULL;

	mutex_lock(&g_client_mutex);

	client = erk_find_client(current_thread);
	if (!client) {
		client = erk_allocate_client(current_thread);
		ret = -ENOMEM;
		goto cleanup_open;

		list_add_rcu(&client->node, &g_client_list);
	} else {
		er_verbose("Already opened for cleint %p\n", current_thread);
		ret = -EINVAL;
		goto cleanup_open;
	}

	ret = 0;

cleanup_open:
	mutex_unlock(&g_client_mutex);
	return ret;
}

static int erebuskm_release(struct inode *inode, struct file *filp)
{
	int ret = -ENOTSUPP;

	struct task_struct *client_thread = filp->private_data;
	struct er_client_t *client = NULL;

	mutex_lock(&g_client_mutex);

	client = erk_find_client(client_thread);
	if (!client) {
		pr_err("Client %p does not exist\n", client_thread);
		ret = -EINVAL;
		goto cleanup_release;
	}

	erk_remove_client(client);
	ret = 0;
	
cleanup_release:
	mutex_unlock(&g_client_mutex);
	return ret;
}

static long erebuskm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTSUPP;

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

		// send results back
		process_list->count = count;

		if (count >= process_list->limit) {
			er_verbose("ERK_IOCTL_GET_PROCESS_LIST: not enough space (%d avail, %d required)\n", (int)process_list->limit, (int)count);

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
	int ret = 0;

	er_verbose("Loading Erebus\n");
	
	g_major = register_chrdev(0, ERK_DEVICE_NAME, &g_erebuskm_fops);
	if (g_major < 0) {
		pr_err("Faied to register the device: %d", g_major);
		ret = g_major;
		goto cleanup_init;
	}

cleanup_init:
	return ret;
}

static void __exit erebuskm_exit(void)
{
	er_verbose("Unloading Erebus\n");
	
	if (g_major >= 0) {
		unregister_chrdev(g_major, ERK_DEVICE_NAME);
		g_major = -1;
	}
	
}

module_init(erebuskm_init);
module_exit(erebuskm_exit);

module_param(g_verbose, int, 0444);
MODULE_PARM_DESC(g_verbose, "Enable verbose logging");
