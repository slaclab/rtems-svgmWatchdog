#ifndef WATCHDOG_SERVICE_H
#define WATCHDOG_SERVICE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define WDPROG	((unsigned int)0xb0aab0aa)
#define WDVERS	((unsigned int)1)

#define WD_CONNECT	1
#define WD_DISCONNECT	2
#define WD_PET		3
#define WD_RESET	4

void wdInit(unsigned long interval_us);
void wdHalt(void);
void wdPet(void);
void wdSysReset(void);
int  wdStop(void);

#ifdef __rtems__
#include <rtems.h>
#include <bsp.h>
#ifndef RTEMS_VERSION_ATLEAST
#error "Missing include -- RTEMS_VERSION_ATLEAST undefined!"
#endif
#if RTEMS_VERSION_ATLEAST(4,9,99)
#include <bsp/bootcard.h>
#endif
#endif

#endif
