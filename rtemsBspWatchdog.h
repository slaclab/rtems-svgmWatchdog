#ifndef WATCHDOG_SERVICE_H
#define WATCHDOG_SERVICE_H

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

#endif
