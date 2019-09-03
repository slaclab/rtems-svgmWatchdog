
/* Watchdog hardware support for BOOKE PPCs.
 * by Till Straumann <strauman@slac.stanford.edu>, Jan. 2007
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include "rtemsBspWatchdog.h"

#ifdef __rtems__
#include <libcpu/spr.h>
#endif

#ifndef BOOKE_TCR_WRC_NO_RESET
#define	BOOKE_TCR_WRC_NO_RESET 1
#endif

#ifndef BOOKE_TCR_WRC_RESET
#define	BOOKE_TCR_WRC_RESET 2
#endif

#define DEBUG	0
#undef  WD_EXCEPTION

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif

SPR_RW(BOOKE_TCR)
SPR_RW(BOOKE_TSR)

#ifdef DEBUG
uint32_t wdRead_TCR() { return _read_BOOKE_TCR(); }
uint32_t wdRead_TSR() { return _read_BOOKE_TSR(); }
void wdWrite_TCR(uint32_t x) { _write_BOOKE_TCR(x); }
void wdWrite_TSR(uint32_t x) { _write_BOOKE_TSR(x); }
#endif

static inline int ld2u(uint32_t x)
{
	asm volatile("cntlzw %0, %0":"=r"(x):"0"(x));
	return 31 - x;
}

void
wdInit(unsigned long us)
{
uint64_t              ticks;
uint32_t              u,l;
int                   shft;
rtems_interrupt_level k;

	/* convert to timebase ticks */

	ticks  = (uint64_t)BSP_bus_frequency * (uint64_t)us;
	ticks /= (uint64_t)BSP_time_base_divisor;
	ticks /= 1000ULL;

	u = (uint32_t)(ticks>>32);
	l = (uint32_t)ticks;

	/* compute shift count */
	if ( (shft = ld2u(u)) < 0 )
		shft = ld2u(l);
	else
		shft+=32;
	
	if ( shft < 0 )
		shft=0;

	/* we have now the rounded-down shift count;
	 * round up: shft++ (0..64) but WD has to 
	 * expire twice in order to reset -> we leave
	 * it as is.
	 */
#ifdef DEBUG
	printf("Shift is %i\n",shft);
#endif

	/* motorola bits call MSB '0' */
	shft = 63-shft;
	
	rtems_interrupt_disable(k);
	u  = _read_BOOKE_TCR();
	/* clear old interval; no point clearing WRC because
	 * once set it cannot be changed (but it always reads zero
	 * anyways (weird design)
	 */
	u &= ~ (BOOKE_TCR_WP(3) | BOOKE_TCR_WPEXT(0xf));
#ifdef WD_EXCEPTION
	/* for testing purposes we can let the watchdog raise a
	 * machine-check exception (on second timeout) instead of
	 * resetting...
	 */
	u |= BOOKE_TCR_WIE | BOOKE_TCR_WRC(BOOKE_TCR_WRC_NO_RESET);
#else
	u |= BOOKE_TCR_WRC(BOOKE_TCR_WRC_RESET);
#endif
	u |= BOOKE_TCR_WP( shft ) | BOOKE_TCR_WPEXT( shft >> 2 );
	_write_BOOKE_TCR(u);
	rtems_interrupt_enable(k);
}

void
wdHalt(void)
{
uint32_t              u;
rtems_interrupt_level k;
	wdPet();
	/* We cannot really stop the watchdog - but we can
	 * make the timeout 2^63 clicks which (at 133MHz bus freq)
	 * amounts to 2^63/(133/4)/3600/24/365 ~ 17500 years!
	 */
	fprintf(stderr,"Cannot halt BookE watchdog; will reset eventually (in ~17500 years ;-)\n");
	rtems_interrupt_disable(k);
	u  = _read_BOOKE_TCR();
	u &= ~ (BOOKE_TCR_WP(3) | BOOKE_TCR_WPEXT(0xf));
	_write_BOOKE_TCR(u);
	rtems_interrupt_enable(k);
}

void
wdPet(void)
{
	/* TSR is 'write-one-to-clear' */
	_write_BOOKE_TSR( BOOKE_TSR_ENW | BOOKE_TSR_WIS );
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
