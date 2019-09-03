
/* Watchdog hardware support for PCs with IPMI/SMIC watchdog.
 * by Till Straumann <strauman@slac.stanford.edu>, Jan. 2007
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include "rtemsBspWatchdog.h"

#define DEBUG	0

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif

extern unsigned short wArmWatchdog(unsigned);
extern unsigned short wResetWatchdog();

void
wdInit(unsigned long us)
{
	/* wArmWatchdog interval is in 100ms units */
	us = us/100000 + 1;
	wArmWatchdog(us);
	/* must pet once to start */
	wResetWatchdog();
}

void
wdHalt(void)
{
	wArmWatchdog(0);
}

void
wdPet(void)
{
	wResetWatchdog();
}

void
wdSysReset(void)
{
#if RTEMS_VERSION_ATLEAST(4,9,99)
	bsp_reset();
#else
	rtemsReboot();
#endif
}
