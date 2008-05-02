/* $Id$ */

/* watchdog hardware support for Synergy VGM BSP
 * by Till Straumann <strauman@slac.stanford.edu>, Oct. 2000
 *    9/09/2002: RTEMS port, 
 */

#if defined(VXWORKS) && defined(__rtems__)
#undef VXWORKS
#endif

#ifdef VXWORKS
#include <vxWorks.h>
#include <fppLib.h>
#include <bootLib.h>
#include <moduleLib.h>
#include <loadLib.h>
#include <sysLib.h>
#endif

#ifdef __rtems__
#include <rtems.h>
#include <bsp.h>
#endif


#include <stdio.h>

#include "rtemsBspWatchdog.h"

#ifdef VXWORKS

extern unsigned long		readPCI();
extern void					writePCI();

extern unsigned long		mpicMemBaseAdrs;

#elif defined(__rtems__)

#include <bsp/openpic.h>
#include <libcpu/io.h>

#define mpicMemBaseAdrs		((unsigned long)OpenPIC)
#define readPCI(adrs) 		in_le32((void*)(adrs))
#define writePCI(adrs,val)	out_le32((void*)(adrs),(val))

#endif


#define DEBUG	0

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif


/* this leaves the timer stopped */
STATIC  unsigned long wdInterval=0x80000000;

#define MPIC_VP_DISABLE			0x80000000

#define MPIC_PPR_CTP_3			0x23080
#define MPIC_T3_BASE_ADR_OFF	0x11d0
#define MPIC_T3_VP_ADR_OFF		0x11e0
#define MPIC_T3_DEST_ADR_OFF	0x11f0


void
wdInit(unsigned long us)
{
unsigned long tmp;
	/* mask processor 3 interrupts */
	writePCI(MPIC_PPR_CTP_3 + mpicMemBaseAdrs, 0xf);
	/* route T3 interrupt to processor3 (-> reset hw) */
	writePCI(MPIC_T3_DEST_ADR_OFF + mpicMemBaseAdrs,8);

	/* MSB==0 starts the timer */
	/* took this count calculation from synergy */

	wdInterval = us*33/8+us*3/80+us*3/800+us*3/8000;

	wdInterval &= 0x7fffffff;

	/* load timer counter */
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),0x80000000);
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),wdInterval);

	/* enable irq */
	tmp = readPCI((MPIC_T3_VP_ADR_OFF+mpicMemBaseAdrs));
	tmp &= ~MPIC_VP_DISABLE;
	tmp |= 0x000f0007; /* taken from example in their manual */
	writePCI((MPIC_T3_VP_ADR_OFF+mpicMemBaseAdrs),tmp);
	/* enable irqs at the fake 'processor 3' */
	writePCI(MPIC_PPR_CTP_3 + mpicMemBaseAdrs, 0x0);
}

void
wdHalt(void)
{
unsigned long tmp;
	/* disable watchdog */
	/* disable irqs at the fake 'processor 3' */
	writePCI(MPIC_PPR_CTP_3 + mpicMemBaseAdrs, 0xf);
	tmp = readPCI((MPIC_T3_VP_ADR_OFF+mpicMemBaseAdrs));
	tmp |= MPIC_VP_DISABLE;
	writePCI((MPIC_T3_VP_ADR_OFF+mpicMemBaseAdrs),tmp);
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),0x80000000);
  	wdInterval=0x80000000;
}

void
wdPet(void)
{
	/* load timer counter */
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),0x80000000);
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),wdInterval);
}

void
wdSysReset(void)
{
	rtemsReboot();
}
