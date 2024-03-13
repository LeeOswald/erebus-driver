#pragma once

#include <linux/version.h>
#include <linux/types.h>



extern int g_verbose;


#define er_verbose(fmt, ...)	\
	do {	\
		if (g_verbose)	\
			pr_info(fmt, ##__VA_ARGS__);	\
	} while (0)


u64 nsec_to_clock_t(u64 x);

struct er_client_t {
	struct list_head node;
	struct task_struct *client_thread;
};
