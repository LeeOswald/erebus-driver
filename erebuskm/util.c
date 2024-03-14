#include "erebuskm.h"

#include <linux/time.h>
#include <linux/param.h>

#if ERK_VERBOSE
int g_verbose = 1;
#else
int g_verbose = 0;
#endif

u64 nsec_to_clock_t(u64 x)
{
#if (NSEC_PER_SEC % USER_HZ) == 0
    return div_u64(x, NSEC_PER_SEC / USER_HZ);
#elif (USER_HZ % 512) == 0
    return div_u64(x * USER_HZ / 512, NSEC_PER_SEC / 512);
#else
    /*
    * max relative error 5.7e-8 (1.8s per year) for USER_HZ <= 1024,
    * overflow after 64.99 years
    * exact for HZ=60, 72, 90, 120, 144, 180, 300, 600, 900, ...
    */
    return div_u64(x * 9, (9ull * NSEC_PER_SEC + (USER_HZ / 2)) / USER_HZ);
#endif
}


