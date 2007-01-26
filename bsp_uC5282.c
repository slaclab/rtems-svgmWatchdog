/* $Id$ */

/* watchdog hardware support for uC5282
 * by Till Straumann <strauman@slac.stanford.edu>, Jan. 2007
 */

#if defined(VXWORKS) && defined(__rtems__)
#undef VXWORKS
#endif

#ifdef __rtems__
#include <rtems.h>
#include <bsp.h>
#include <mcf5282/mcf5282.h>
#endif


#include <stdio.h>

#include "wd.h"

#define DEBUG	0

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif

void
wdInit(unsigned long us)
{
unsigned long tmp;

	/* FIXME: get system clock rate from somewhere */
#define PRESC      8192
#define SYSCLK_MHZ 64
	tmp = us / (PRESC/SYSCLK_MHZ);

	if ( tmp < 1 )
		tmp = 1;
	if ( tmp > 0xffff )
		tmp = 0xffff;

	MCF5282_WTM_WMR  = tmp;
	MCF5282_WTM_WCR |= MCF5282_WTM_WCR_EN;

	if ( ! (MCF5282_WTM_WCR_EN & MCF5282_WTM_WCR) ) {
		printf("Warning: Watchdog could NOT be enabled; this bit can only\n");
		printf("         be changed ONCE and the firmware has probably   \n");
		printf("         disabled the watchdog already -- sorry...       \n");
	}
}

void
wdHalt(void)
{
	/* disable watchdog -- don't know if this really works */
	/* UPDATE: is DOESN't work. The enable bit is sticky... :-( */
	MCF5282_WTM_WCR &= ~MCF5282_WTM_WCR_EN;
}

void
wdPet(void)
{
	MCF5282_WTM_WSR = 0x5555;
	MCF5282_WTM_WSR = 0xAAAA;
}

void
wdSysReset(void)
{
	bsp_reset(0);
}
