/* watchdog service
 * by Till Straumann <strauman@slac.stanford.edu>, Oct. 2000
 */

#ifdef VXWORKS
#include <vxWorks.h>
#include <rpcLib.h>
#include <fppLib.h>
#include <bootLib.h>
#include <moduleLib.h>
#include <loadLib.h>
#include <sysLib.h>
#if 0
#include <svgm1.h>
#else
#define REG_WATCHDOG_PET ((UCHAR *)0xffefff48)
#endif
#endif

#include <rpc/svc.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>

#include "wd.h"

#ifdef VXWORKS
/* priority of the watchdog task */
#define WD_PRIO		3
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


#define DEBUG

#ifdef DEBUG
#define STATIC
#else
#define STATIC static
#endif


STATIC	SVCXPRT *wdSvc=0;
#ifdef VXWORKS
STATIC	int	wdTaskId=ERROR;
/* this leaves the timer stopped */
STATIC  unsigned long wdInterval=0x80000000;

extern unsigned long readPCI();
extern void writePCI();

extern unsigned long mpicMemBaseAdrs;

#define MPIC_VP_DISABLE		0x80000000

#define MPIC_T3_BASE_ADR_OFF	0x11d0
#define MPIC_T3_VP_ADR_OFF	0x11e0
#define MPIC_T3_DEST_ADR_OFF	0x11f0


static void
wdInit(unsigned long us)
{
unsigned long tmp;
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
}

static void
wdStop(void)
{
unsigned long tmp;
	/* disable watchdog */
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

#ifndef VXWORKS
static jmp_buf jmpEnv;

static void
sigHandler(int sig)
{
	longjmp(jmpEnv,1);
}

static void
installSignalHandler(sigset_t *mask)
{
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

}

#endif

static int
wdServer(void)
{
	int	rval=0;
	fd_set	fdset;
	/* tell vxWorks that this task will to RPC
	 * (vxWorks tasks must/can not share rpc context :-(
	 */
#ifdef VXWORKS
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
		rval = -1;
		goto leave;
	}
#ifdef VXWORKS
	wdInit(WD_INTERVAL);
#else
	if ( 0 == setjmp(jmpEnv)) {
		sigset_t block;
		/* install a signal handler */
		installSignalHandler(&block);


#endif

#if 1
		do {
			struct timeval tout;
			int	max=wdSvc->xp_sock;
			int	sval;

#ifdef VXWORKS
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
#ifndef VXWORKS
				else {
					printf("WATCHDOG TIMEOUT, resetting...\n");	
					connected=0;
				}
#endif
				if (!connected)
					ticks=1;
			}
		} while (1);
#else
		/* run the server; the simple approach will not
		 * prevent from the server being interrupted while
		 * processing requests
		 */
		svc_run();
#endif
#ifndef VXWORKS
	} else {
		/* signal handler jumps in here */
		fprintf(stderr,"(longjump) terminating...\n");
	}
#endif



leave:
	/* vxWorks: we MUST cleanup in the server context */
	wdCleanup();
#ifdef VXWORKS
	wdTaskId=ERROR;
#endif
	return -1;
}

STATIC void
wdCleanup(void)
{
  if (wdSvc)
    {
      svc_unregister (WDPROG, WDVERS);
      svc_destroy (wdSvc);
    }
#ifdef VXWORKS
  wdStop();
  wdInterval=0x80000000;
#endif
}

#ifdef VXWORKS
void
wdStart(void)
{
	if (ERROR!=wdTaskId) {
		fprintf(stderr,"wd already running\n");
		return;
	}
	rpcInit();
	wdTaskId=taskSpawn("tWatchdog",WD_PRIO,0,5000,wdServer,
			0,0,0,0,0,0,0,0,0,0);
	if (ERROR==wdTaskId) {
		fprintf(stderr,"Unable to spawn WD server task\n");
	}

}

#else
int
main(int argc, char **argv)
{
	wdServer();
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
#ifndef VXWORKS
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
#ifndef VXWORKS
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
#ifndef VXWORKS
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
#ifndef VXWORKS
				printf("RESET\n");
#endif
				svc_sendreply(xprt,xdr_bool,(char*)&rval);
				if (connected) {
					/* leave in the connected state,
					 * so the watchdog times out
					 */
#ifdef VXWORKS
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
