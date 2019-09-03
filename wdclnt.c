#include <rpc/rpc.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "rtemsBspWatchdog.h"

static void
usage(char *n)
{
	fprintf(stderr,"Git revision %s\n", PACKAGE_VERSION);
	fprintf(stderr,"usage: %s [-rvh] target\n",n);
	fprintf(stderr,"          -r: reset now and quit\n");
	fprintf(stderr,"          -v: print version number and quit\n");
	fprintf(stderr,"          -h: print this message and quit\n");
	fprintf(stderr,"       While this program is running, the target\n");
	fprintf(stderr,"       system is 'watched/petted'.\n");
	fprintf(stderr,"       Killing (INT signal) disconnects from the target.\n");
	fprintf(stderr,"       Sending USR1 signal requests target to reset.\n");
    fprintf(stderr,"       Suspending this program eventually results in a target reset.\n");
}

static int terminate=0, reset=0;

static void
sigHandler(int signal)
{
	if (SIGUSR1==signal) reset=1;
	terminate=1;
}

int
main(int argc, char **argv)
{
bool_t			stat;
enum clnt_stat	cst;
int				ch;
char			*hostnm;
int             attempt = 0; /* for debugging */
CLIENT          *clnt   = 0;
struct timeval  tout;

	while ((ch=getopt(argc,argv,"rhv")) >= 0) {
		switch (ch) {
			default:
				fprintf(stderr,"Unknown option '%c'\n",ch);
				/* fall thru */

			case 'h':
				usage(argv[0]);
				exit(0);

			case 'v':
				fprintf(stderr,"%s\n","$Revision$");
				exit(0);

			case 'r':
				reset = 1;
				break;
		}
	}
	if (optind>=argc) {
		usage(argv[0]);
		exit(1);
	}
	hostnm=argv[optind];

	if ( ! (clnt = clnt_create(hostnm, WDPROG, WDVERS, "udp")) ) {
		clnt_pcreateerror("Unable to create rpc client");
		exit(1);	
	}

	tout.tv_sec  =      0;
	tout.tv_usec = 100000; 
	clnt_control(clnt, CLSET_RETRY_TIMEOUT, (char*)&tout);

	tout.tv_sec  =     1;
	tout.tv_usec =     0;

	if ( (cst = clnt_call(clnt, WD_CONNECT,
			(xdrproc_t)xdr_void,0,(xdrproc_t)xdr_bool,(char*)&stat,
			tout)) ) {
		fprintf(stderr,"Unable to connect to watchdog on %s:",hostnm);
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		exit(1);
	}
	if (!stat) {
		fprintf(stderr,"watchdog on %s refused connection (already connected?)\n",hostnm);
		exit(1);
	}
	fprintf(stderr,"Connected to watchdog on %s\n",hostnm);

	{
	struct sigaction sa;
	memset(&sa,0,sizeof(struct sigaction));
	sa.sa_handler = sigHandler;
	if (sigaction(SIGINT,&sa,0)||sigaction(SIGUSR1,&sa,0)) {
		perror("unable to install signal handler");
		terminate=1;
	}
	}

	while (!terminate) {
		sleep(3);
		if (reset) {
#ifdef DEBUG
		printf("clnt RESET\n");
#endif
		if ( (cst = clnt_call(clnt, WD_RESET,
			(xdrproc_t)xdr_void,0,(xdrproc_t)xdr_bool,(char*)&stat,
			tout)) ) {
		fprintf(stderr,"Unable to force target reset (rpc failed)");
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		terminate=0;
		} else {
			/* target is dead already, disconnect will fail */
			exit(0);	
		}
		}
		if (!terminate) {
#ifdef DEBUG
		printf("clnt PET %i\n", attempt);
#endif
		if ( (cst = clnt_call(clnt, WD_PET,
			(xdrproc_t)xdr_int,(char*)&attempt,(xdrproc_t)xdr_bool,(char*)&stat,
			tout)) ) {
		fprintf(stderr,"Unable to pet watchdog");
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		terminate=1;
		} else {
		if (!stat) {
		fprintf(stderr,"watchdog refused PET (already connected?)\n");
		terminate=1;
	}
		}
		attempt++;
		}
	}

	if ( (cst = clnt_call(clnt, WD_DISCONNECT,
			(xdrproc_t)xdr_void,0,(xdrproc_t)xdr_bool,(char*)&stat,
			tout)) ) {
		fprintf(stderr,"Unable to disconnect from watchdog on %s:",hostnm);
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		exit(1);
	}
	if (!stat) {
		fprintf(stderr,"watchdog refused DISCONNECT (not connected?)\n");
		exit(1);
	}
	clnt_destroy(clnt);
return 0;
}
