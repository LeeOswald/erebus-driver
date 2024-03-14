#include "erebuskm.h"
#include "shared.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/sched/cputime.h>
#include <linux/sched/signal.h>
#include <linux/vmalloc.h>



#ifndef CONFIG_HAVE_SYSCALL_TRACEPOINTS
#error The kernel must have HAVE_SYSCALL_TRACEPOINTS
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lee Oswald");

struct er_device_t {
	dev_t dev;
	struct cdev cdev;
	wait_queue_head_t read_queue;
};


struct er_client_t {
	struct list_head node;
	struct task_struct *client_thread;
};

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
static struct class *g_class = NULL;
static unsigned g_numdevs = 0;
static unsigned g_created_devices = 0;
static struct er_device_t *g_devs = NULL;

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
	
	er_verbose(ERK "Allocating client %p\n", thread);

	client = vmalloc(sizeof(struct er_client_t));
	if (!client) {
		pr_err(ERK "Failed to allocate a new client\n");
		return NULL;
	}

	client->client_thread = thread;

	return client;
}

static void erk_remove_client(struct er_client_t *client)
{
	er_verbose(ERK "Deallocating client %p\n", client->client_thread);

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
		if (!client) {
			ret = -ENOMEM;
			goto cleanup_open;
		}

		list_add_rcu(&client->node, &g_client_list);
		filp->private_data = current_thread;
	} else {
		er_verbose(ERK "Already opened for client %p\n", current_thread);
		ret = -EINVAL;
		goto cleanup_open;
	}

	ret = 0;
	er_verbose(ERK "Opened for client %p\n", current_thread);

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
		pr_err(ERK "Client %p does not exist\n", client_thread);
		ret = -EINVAL;
		goto cleanup_release;
	}

	erk_remove_client(client);
	ret = 0;
	er_verbose(ERK "Released by client %p\n", client_thread);
	
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

		/* retrieve args */
		if (copy_from_user(&request, (void *)arg, sizeof(request))) {
			ret = -EINVAL;
			goto ioctl_cleanup;
		}

		if (request.limit < 0 || request.limit > ERK_MAX_PROCESS_LIST)
		{
			er_verbose(ERK "ERK_IOCTL_GET_PROCESS_LIST: invalid limit %llu\n", request.limit);
			ret = -EINVAL;
			goto ioctl_cleanup_procinfo;
		}

		er_verbose(ERK "ERK_IOCTL_GET_PROCESS_LIST limit=%d\n", (int)request.limit);

		memsize = sizeof(struct erk_process_list) + sizeof(struct erk_process_info) * request.limit;
		process_list = vmalloc(memsize);
		if (!process_list) {
			er_verbose(ERK "Not enough memory\n");
			ret = -ENOMEM;
			goto ioctl_cleanup;
		}

		process_list->limit = request.limit;

		/* enumerate tasks */
		rcu_read_lock();
		
		for_each_process_thread(process, thread) {

			u64 utime = 0;
			u64 stime = 0;

			task_cputime_adjusted(thread, &utime, &stime);
			process_list->entries[count].pid = thread->pid;
			process_list->entries[count].utime = nsec_to_clock_t(utime);
			process_list->entries[count].stime = nsec_to_clock_t(stime);

			++count;
		}

		rcu_read_unlock();

		/* send results back */
		process_list->count = count;

		if (count >= process_list->limit) {
			er_verbose(ERK "ERK_IOCTL_GET_PROCESS_LIST: not enough space (%d avail, %d required)\n", (int)process_list->limit, (int)count);

			/* return expected entry count to client */
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

			er_verbose(ERK "Copied %d bytes (%d entries) to client\n", (int)memsize, (int)count);
		}

		ret = 0;
ioctl_cleanup_procinfo:
		vfree((void *)process_list);
		goto ioctl_cleanup;
	}

ioctl_cleanup:
	return ret;
}

static char *erebuskm_devnode(struct device *dev, umode_t *mode)
{
	if (mode) {
		*mode = S_IRUSR;

		if (dev) {
			#if ERK_DISABLE_PERMISSIONS_CHECKS
			*mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
			#else
			*mode = S_IRUSR | S_IWUSR;
			#endif
		}
	}

	return NULL;
}

static void global_cleanup(void)
{
	unsigned j = 0;
	for (j = 0; j < g_created_devices; ++j) {
		device_destroy(g_class, g_devs[j].dev);
		cdev_del(&g_devs[j].cdev);
		er_verbose(ERK "Deleted device %d\n", j);
	}

	g_created_devices = 0;
	
	if (g_devs) {
		kfree(g_devs);
		g_devs = NULL;
	}
	
	if (g_class) {
		class_destroy(g_class);
		g_class = NULL;
	}

	if (g_major >= 0) {
		unregister_chrdev_region(MKDEV(g_major, 0), g_numdevs);
		g_major = -1;
	}
}

static int __init erebuskm_init(void)
{
	int ret = 0;
	dev_t dev = 0;
	unsigned j = 0;
	
	er_verbose(ERK "Initializing...\n");
#if ERK_DISABLE_PERMISSIONS_CHECKS
	pr_info(ERK "NOTE: permissons checks disabled\n");
#endif
	
	ret = alloc_chrdev_region(&dev, 0, 1, ERK_DEVICE_NAME);
	if (ret < 0) {
		pr_err(ERK "Could not allocate major number for %s: %d\n", ERK_DEVICE_NAME, ret);
		goto exit_init;
	}

	g_class = class_create(THIS_MODULE, ERK_DEVICE_NAME);
	if (IS_ERR(g_class)) {
		pr_err(ERK "Failed to allocate the device class\n");
		ret = -EFAULT;
		goto cleanup_init;
	}

	g_class->devnode = erebuskm_devnode;

	g_major = MAJOR(dev);
	g_numdevs = 1;

	g_devs = kmalloc(g_numdevs * sizeof(struct er_device_t), GFP_KERNEL);
	if (!g_devs) {
		pr_err(ERK "Failed to allocate devices\n");
		ret = -ENOMEM;
		goto cleanup_init;
	}

	for (j = 0; j < g_numdevs; ++j) {
		struct device *device = NULL;
		
		cdev_init(&g_devs[j].cdev, &g_erebuskm_fops);
		g_devs[j].dev = MKDEV(g_major, j);

		device = device_create(
			g_class,
			NULL, /* no parent device */
			g_devs[j].dev,
			NULL, /* no additional data */
			ERK_DEVICE_NAME "%d",
			j
		);

		if(IS_ERR(device)) {
			pr_err(ERK "Failed to create the device for  %s\n", ERK_DEVICE_NAME);
			cdev_del(&g_devs[j].cdev) ;
			ret = -EFAULT ;
			goto cleanup_init;
		}

		if (cdev_add(&g_devs[j].cdev, g_devs[j].dev, 1) < 0) {
			pr_err(ERK "Failed to allocate cdev for %s\n", ERK_DEVICE_NAME);
			ret = -EFAULT;
			goto cleanup_init;
		}

		er_verbose(ERK "Created device %d:%d\n", g_major, j);

		init_waitqueue_head(&g_devs[j].read_queue);
		++g_created_devices;
	}

	goto exit_init;

cleanup_init:
	global_cleanup();
	
exit_init:
	return ret;
}

static void __exit erebuskm_exit(void)
{
	er_verbose(ERK "Unloading...\n");

	global_cleanup();
}

module_init(erebuskm_init);
module_exit(erebuskm_exit);

module_param(g_verbose, int, 0444);
MODULE_PARM_DESC(g_verbose, "Enable verbose logging");
