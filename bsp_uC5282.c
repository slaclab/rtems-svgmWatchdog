
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

#include "rtemsBspWatchdog.h"

#define DEBUG	0

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* This global variable selects
 * the type of watchdog timer:
 */

#define WD_MAJOR_HW		0x00
/* DMA Timer; minor number selects timer 0..3 */
#define WD_MAJOR_DMA	0x10

#define WD_MAJOR_SHIFT	4
#define WD_MINOR_MASK	0xf

unsigned uC5282WatchdogType = WD_MAJOR_DMA | 0 /* DMA timer 0 */;

struct wdops_ {
	void (*init)(unsigned long);	
	void (*halt)(void);
	void (*pet) (void);
};

static unsigned
us2ticks(unsigned long us, unsigned presc)
{
/* FIXME: get system clock rate from somewhere */
#define SYSCLK_MHZ 64
unsigned long long tmp = us * SYSCLK_MHZ;
unsigned rval;

	tmp/=presc;

	if ( (rval=(unsigned)tmp) < 1 )
		rval = 1;
	
	return rval;
}

static void
hwWdInit(unsigned long us)
{
unsigned long tmp;

#define PRESC      8192

	tmp = us2ticks(us, PRESC);

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

static void
hwWdHalt(void)
{
	/* disable watchdog -- don't know if this really works */
	/* UPDATE: is DOESN't work. The enable bit is sticky... :-( */
	MCF5282_WTM_WCR &= ~MCF5282_WTM_WCR_EN;
}

static void
hwWdPet(void)
{
	MCF5282_WTM_WSR = 0x5555;
	MCF5282_WTM_WSR = 0xAAAA;
}

/* Use DMA timer output hardwired to RESET (needs
 * apropriate board layout / blue-wires).
 */
static void
dmaWdInit(unsigned long us)
{
unsigned minor = uC5282WatchdogType & WD_MINOR_MASK;
unsigned presc = 127;
unsigned short dtmr;

	if ( minor > 3 ) {
		fprintf(stderr,"dmaWdInit: unsupported minor #%i\n",minor);
		return;
	}

	MCF5282_TIMER_DTMR(minor)  = 0;
	MCF5282_TIMER_DTXMR(minor) = 0;

	switch ( minor ) {
		case 0:
			MCF5282_GPIO_PTDPAR |= MCF5282_GPIO_PTDPAR_PTDPA0(3);
			break;
		case 1:
			MCF5282_GPIO_PTDPAR |= MCF5282_GPIO_PTDPAR_PTDPA2(3);
			break;
		case 2:
			MCF5282_GPIO_PTCPAR |= MCF5282_GPIO_PTCPAR_PTCPA0(3);
			break;
		case 3:
			MCF5282_GPIO_PTCPAR |= MCF5282_GPIO_PTCPAR_PTCPA2(3);
			break;
	}

	dtmr =
		MCF5282_TIMER_DTMR_PS(presc) |
		MCF5282_TIMER_DTMR_OM        | /* toggle output; 15ns pulse would be too short */
		MCF5282_TIMER_DTMR_FRR       | /* toggle output; 15ns pulse would be too short */
		MCF5282_TIMER_DTMR_CLK_DIV1;

	MCF5282_TIMER_DTMR(minor)  =  dtmr;
	MCF5282_TIMER_DTRR(minor)  =  us2ticks(us, presc+1);
	/* start */
	MCF5282_TIMER_DTMR(minor)  = dtmr | MCF5282_TIMER_DTMR_RST;		
}

static void
dmaWdHalt(void)
{
unsigned minor = uC5282WatchdogType & WD_MINOR_MASK;

	if ( minor > 3 )
		return;

	MCF5282_TIMER_DTMR(minor)  = 0;
}

static void
dmaWdPet(void)
{
unsigned minor = uC5282WatchdogType & WD_MINOR_MASK;

	if ( minor > 3 )
		return;

	/* reset timer */
	MCF5282_TIMER_DTCN(minor)  = 0;
}

static struct wdops_ wdtypes[]={
	{ hwWdInit,   hwWdHalt,  hwWdPet },
	{ dmaWdInit, dmaWdHalt, dmaWdPet },
};

#define NumberOf(arr)	(sizeof(arr)/sizeof(arr[0]))

#define WD_DO(what,arg) \
	do {                                                     \
		unsigned idx = uC5282WatchdogType >> WD_MAJOR_SHIFT; \
	    if ( idx < NumberOf(wdtypes) ) {                     \
	    	wdtypes[idx].what(arg);                          \
	    }                                                    \
	} while (0)

void
wdInit(unsigned long period)
{
	WD_DO(init, period);
}

void
wdHalt(void)
{
	WD_DO(halt, );
}

void
wdPet(void)
{
	WD_DO(pet, );
}

void
wdSysReset(void)
{
	bsp_reset(0);
}
