#pragma once

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#endif

#pragma pack(push, 1)


#define ERK_IOCTL_MAGIC              'e'

#define ERK_IOCTL_GET_PROCESS_LIST    _IO(ERK_IOCTL_MAGIC, 1)


struct erk_process_info {
	uint64_t pid;
	uint64_t utime;
	uint64_t stime;
};

#define ERK_MAX_PROCESS_LIST      1000000
struct erk_process_list {
	int64_t count;
	int64_t limit;
	struct erk_process_info entries[0];
};

#pragma pack(pop)
