#include "erebuskm.h"
#include "shared.h"

#include <linux/sched/cputime.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>


long erebuskm_ioctl_get_processlist(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	struct erk_process_list *process_list = NULL;
	u32 memsize = 0;
	struct erk_process_list request;
	struct task_struct *process = NULL;
	struct task_struct *thread = NULL;
	u64 count = 0;

	/* retrieve args */
	if (copy_from_user(&request, (void *)arg, sizeof(request))) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (request.limit < 0 || request.limit > ERK_MAX_PROCESS_LIST)
	{
		er_verbose(ERK "ERK_IOCTL_GET_PROCESS_LIST: invalid limit %llu\n", request.limit);
		ret = -EINVAL;
		goto cleanup;
	}

	er_verbose(ERK "ERK_IOCTL_GET_PROCESS_LIST limit=%d\n", (int)request.limit);

	memsize = sizeof(struct erk_process_list) + sizeof(struct erk_process_info) * request.limit;
	process_list = vmalloc(memsize);
	if (!process_list) {
		er_verbose(ERK "Not enough memory\n");
		ret = -ENOMEM;
		goto cleanup;
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
			goto cleanup;
		}

		ret = -ENOSPC;
		goto cleanup;
	}
	else {
		memsize = sizeof(struct erk_process_list) + sizeof(struct erk_process_info) * count;

		if (copy_to_user((void *)arg, process_list, memsize)) {
			ret = -EINVAL;
			goto cleanup;
		}

		er_verbose(ERK "Copied %d bytes (%d entries) to client\n", (int)memsize, (int)count);
	}

	ret = 0;
cleanup:
	vfree((void *)process_list);

	return 0;
}
