
/* watchdog hardware support for beatnik
 * by Till Straumann <strauman@slac.stanford.edu>, Jan. 2007
 */

#ifdef __rtems__
#include <rtems.h>
#include <bsp.h>
#include <bsp/gt_timer.h>
#endif

#include <stdio.h>

#include "rtemsBspWatchdog.h"

#ifdef DEBUG
#define STATIC
#else
#define STATIC static
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void
wdInit(unsigned long us)
{
	BSP_watchdog_enable(us);
}

void
wdHalt(void)
{
	BSP_watchdog_disable();
}

void
wdPet(void)
{
	BSP_watchdog_pet();
}

void
wdSysReset(void)
{
#if RTEMS_VERSION_ATLEAST(4,9,0)
	bsp_reset();
#else
	rtemsReboot();
#endif
}
