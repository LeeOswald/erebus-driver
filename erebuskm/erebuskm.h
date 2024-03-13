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

unsigned cpu_count(void);
