#pragma once

#include <linux/version.h>
#include <linux/types.h>

#if 1
	#define ERK_VERBOSE 1
	#define ERK_DISABLE_PERMISSIONS_CHECKS 1
#endif


extern int g_verbose;


#define er_verbose(fmt, ...)	\
	do {	\
		if (g_verbose)	\
			pr_info(fmt, ##__VA_ARGS__);	\
	} while (0)

#define ERK "Erebus: "

u64 nsec_to_clock_t(u64 x);

