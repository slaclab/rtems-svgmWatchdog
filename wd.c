/* $Id$ */

/* watchdog RPC service
 * by Till Straumann <strauman@slac.stanford.edu>, Oct. 2000
 *    9/09/2002: RTEMS port, 
 */

#if defined(VXWORKS) || defined(__rtems__)
#include "wrap.h"
#else
#define  PTASK_DECL(entry,arg)	int entry(void *arg)
#define  PTASK_LEAVE do {} while (0)
#ifndef USE_SIGHANDLER
#define USE_SIGHANDLER
#endif
#endif

#ifdef HAVE_CEXP
#include <cexpHelp.h>
#endif

#if defined(VXWORKS) && defined(__rtems__)
#undef VXWORKS
#endif

#ifdef VXWORKS
#include <vxWorks.h>
#include <rpcLib.h>
#include <fppLib.h>
#include <bootLib.h>
#include <moduleLib.h>
#include <loadLib.h>
#include <sysLib.h>
#if 0
#if 0
#include <svgm1.h>
#else
#define REG_WATCHDOG_PET ((UCHAR *)0xffefff48)
#endif
#endif
#endif

#ifdef __rtems__
#include <rtems.h>
#endif


#include <sys/types.h>

#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>

#include <rpc/svc.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>

#include "wd.h"

#ifdef VXWORKS

extern unsigned long		readPCI();
extern void					writePCI();

extern unsigned long		mpicMemBaseAdrs;

#define NOTASK_ID			ERROR

#elif defined(__rtems__)

#include <bsp/openpic.h>
#include <libcpu/io.h>

#define mpicMemBaseAdrs		((unsigned long)OpenPIC)
#define readPCI(adrs) 		in_le32((void*)(adrs))
#define writePCI(adrs,val)	out_le32((void*)(adrs),(val))
#define	rpcInit()			do {} while(0)
#define rpcTaskInit()		rtems_rpc_task_init()
#define sysReset()			rtemsReboot()

#define NOTASK_ID			0

#endif


#ifdef SYNERGYTARGET
/* hardware watchdog pet interval
 * 1000000 * PET_S + PET_US us
 */
#define PET_S		0
#define PET_US		500000
/* how often must we get a pet
 * from the client while connected
 */
#define CLNT_PET_US	5000000
#define WD_INTERVAL	1200000
#else
#define PET_S		1
#define PET_US		0
#define CLNT_PET_US	5000000
#endif


#define DEBUG	0

#if	DEBUG > 0
#define STATIC
#else
#define STATIC static
#endif


STATIC	SVCXPRT *wdSvc=0;

#if defined(VXWORKS) || defined(__rtems__)
/* make these public for convenience */
PTaskId	wdTaskId=NOTASK_ID;
#endif
int		wdRunning=0;

#ifdef SYNERGYTARGET
/* this leaves the timer stopped */
STATIC  unsigned long wdInterval=0x80000000;

#define MPIC_VP_DISABLE			0x80000000

#define MPIC_PPR_CTP_3			0x23080
#define MPIC_T3_BASE_ADR_OFF	0x11d0
#define MPIC_T3_VP_ADR_OFF		0x11e0
#define MPIC_T3_DEST_ADR_OFF	0x11f0


static void
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

static void
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
}

static void
pet(void)
{
	/* load timer counter */
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),0x80000000);
	writePCI((MPIC_T3_BASE_ADR_OFF+mpicMemBaseAdrs),wdInterval);
}

#else
static void
pet(void)
{
printf("pet\n");
}
#endif

static void
wd_dispatch(struct svc_req *req, SVCXPRT *xprt);

STATIC int	connected=0;
STATIC int	ticks=1;
STATIC int	TICKS;

STATIC void wdCleanup(void);

#ifdef USE_SIGHANDLER
static jmp_buf jmpEnv;

static void
sigHandler(int sig)
{
#ifdef __rtems__
extern void	CPU_print_stack();
	CPU_print_stack();
#endif
	longjmp(jmpEnv,1);
}

static void
installSignalHandler(sigset_t *mask)
{
#ifndef __rtems__
        struct sigaction sa;

        memset(&sa,0,sizeof(sa));

        sigemptyset(mask);
        sigaddset(mask,SIGINT);

        /* we just need the signal to interrupt select */
        sa.sa_handler=sigHandler;
        sigaddset(&sa.sa_mask,SIGINT);
#ifdef VXWORKS
        sa.sa_flags|=SA_INTERRUPT;
#endif

        if (sigaction(SIGINT,&sa,0))
                perror("sigaction");
#endif
}
#endif

STATIC PTASK_DECL(wdServer, unused)
{
	int	rval=0;
	fd_set	fdset;

	/* tell vxWorks that this task will to RPC
	 * (vxWorks tasks must/can not share rpc context :-(
	 */
#ifdef SYNERGYTARGET
	rpcTaskInit();
#endif

	TICKS = CLNT_PET_US / (1000000 * PET_S + PET_US);

	/* create service handle */
	if (!(wdSvc = svcudp_bufcreate (RPC_ANYSOCK, 1500, 1500)))
	{
		fprintf(stderr, "unable to create RPC service\n");
		rval = -1;
		goto leave;
	}
	/* advertise our service */
	if (0 ==
		svc_register (wdSvc, WDPROG, WDVERS, wd_dispatch, IPPROTO_UDP))
	{
		svc_destroy (wdSvc);
		wdSvc = 0;
		fprintf (stderr, "unable to register the watchdog RPC service\n");
#if !defined(__rtems__) && !defined(VXWORKS)
		fprintf (stderr, "use 'rpcinfo -d' as root to unregister stale svc\n");
#endif
		rval = -1;
		goto leave;
	}
#ifdef SYNERGYTARGET
	wdInit(WD_INTERVAL);
#endif
#ifdef USE_SIGHANDLER
	if ( 0 == setjmp(jmpEnv)) {
		sigset_t block;
		/* install a signal handler */
		installSignalHandler(&block);


#endif

#if 1
		for (wdRunning=1; wdRunning; ) {
			struct timeval tout;
			int	max=wdSvc->xp_sock;
			int	sval;

#if defined(VXWORKS) || defined(__rtems__)
			/* vxWorks 5.4 does not export svc_fdset */
			FD_ZERO(&fdset);
			FD_SET(max,&fdset);
#else
			fdset=svc_fdset;
#endif
			tout.tv_sec=PET_S;
			tout.tv_usec=PET_US;

			/* hope xp_sock was the only one in the set */
			/* allow signals while we select */
			if ((sval=select(max+1,&fdset,0,0,&tout)) >= 0 ) {
				if (sval>0)
					svc_getreqset(&fdset);
				/* else it timed out */

				if (ticks-->0)
					pet();
#ifndef SYNERGYTARGET
				else {
					printf("WATCHDOG TIMEOUT, resetting...\n");	
					connected=0;
				}
#endif
				if (!connected)
					ticks=1;
			}
		}
#else
		/* run the server; the simple approach will not
		 * prevent from the server being interrupted while
		 * processing requests
		 */
		svc_run();
#endif
#ifdef USE_SIGHANDLER
	} else {
		/* signal handler jumps in here */
		fprintf(stderr,"(longjump) terminating server.\n");
	}
#endif



leave:
	/* vxWorks: we MUST cleanup in the server context */
	wdCleanup();
#ifdef SYNERGYTARGET
	wdTaskId=NOTASK_ID;
#endif
	PTASK_LEAVE;
}

STATIC void
wdCleanup(void)
{
  if (wdSvc)
    {
      svc_unregister (WDPROG, WDVERS);
      svc_destroy (wdSvc);
    }
#ifdef SYNERGYTARGET
  wdHalt();
  wdInterval=0x80000000;
#endif
}

#if defined(VXWORKS) || defined(__rtems__)
int
wdStart(int nativePrio)
{
	if (NOTASK_ID!=wdTaskId) {
		fprintf(stderr,"wd already running\n");
		return -1;
	}

	if (0==nativePrio) {
		fprintf(stderr,"usage: wdStart(priority)\n");
		return -1;
	}

	rpcInit();

	/* RTEMS: newlibc has a bug: setjmp always stores the FP context;
	 *        I have patched it, however...
	 */
	if (pTaskSpawn("wdog", NATIVE2SCALED(nativePrio), 5000, 0, wdServer, 0, &wdTaskId)) {
		wdTaskId=NOTASK_ID;
		fprintf(stderr,"Unable to spawn WD server task\n");
		return -1;
	} else {
		printf("Watchdog started; (wdTaskId) ID 0x%08x\n",wdTaskId);
#ifndef SYNERGYTARGET
		printf("THIS IS A TESTVERSION - DOESN't TALK TO ANY HARDWARE\n");
#endif
	}
	return 0;
}

int
wdStop(void)
{
	/* clear the 'running' flag; it is polled by the server */
	wdRunning=0;
	return 0;
}

static int
_cexpModuleFinalize(void *mod)
{
int polls = 5;

	wdStop();

	for (polls = 5; NOTASK_ID != (volatile PTaskId)wdTaskId && polls>=0; polls--)
			sleep(1);

	if (polls<0) {
		fprintf(stderr,"Watchdog won't die, refusing to unload\n");
		return -1;
	}
	return 0;
}

#else
int
main(int argc, char **argv)
{
	wdServer(0);
	return 0;
}

#endif

static void
wd_dispatch(struct svc_req *req, SVCXPRT *xprt)
{
	switch (req->rq_proc) {
		case WD_CONNECT:
			{
				bool_t rval = !connected;
				/* pet once in case sending the
				 * reply takes a long time...
				 */
				pet();
#if !defined(SYNERGYTARGET) || DEBUG > 1
				printf("CONNECT\n");
#endif
				svc_sendreply(xprt,xdr_bool,(char*)&rval);
				if (!connected) {
					connected=1;
					ticks = TICKS;
				}
			}
			break;
		case WD_DISCONNECT:
			{
				bool_t rval = connected;
				/* pet once in case sending the
				 * reply takes a long time...
				 */
				pet();
#if !defined(SYNERGYTARGET) || DEBUG > 1
				printf("DISCONNECT\n");
#endif
				svc_sendreply(xprt,xdr_bool,(char*)&rval);
				if (connected) {
					connected=0;
					ticks=1;
				}
			}
			break;
		case WD_PET:
			{
				bool_t rval = connected;
				/* pet once in case sending the
				 * reply takes a long time...
				 */
				pet();
#if !defined(SYNERGYTARGET) || DEBUG > 1
				printf("PET\n");
#endif
				svc_sendreply(xprt,xdr_bool,(char*)&rval);
				if (connected) {
					ticks=TICKS;
				}
			}
			break;
		case WD_RESET:
			{
				bool_t rval = connected;
				/* pet once in case sending the
				 * reply takes a long time...
				 */
				pet();
#if !defined(SYNERGYTARGET) || DEBUG > 1
				printf("RESET\n");
#endif
				svc_sendreply(xprt,xdr_bool,(char*)&rval);
				if (connected) {
					/* leave in the connected state,
					 * so the watchdog times out
					 */
#ifdef SYNERGYTARGET
					sleep(1);	/* give the network task time to send the reply before we die
								 * - either by a watchdog timeout or by the sysReset
								 */
					sysReset(); /* force hard reset now */
#endif
					ticks=0;
				}
			}
			break;
		default:
			svcerr_noproc(xprt);
	}
}
