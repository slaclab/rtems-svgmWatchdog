#include <rpc/rpc.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>


#include <signal.h>

#include "wd.h"

static void
usage(char *n)
{
	fprintf(stderr,"usage: %s [-r] hostname\n",n);
	fprintf(stderr,"          -r: reset now and quit\n");
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

	while ((ch=getopt(argc,argv,"rh")) >= 0) {
		switch (ch) {
			default:
				fprintf(stderr,"Unknown option '%c'\n",ch);
				/* fall thru */

			case 'h':
				usage(argv[0]);
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
	if (cst=callrpc(hostnm,WDPROG,WDVERS,WD_CONNECT,
			xdr_void,0,xdr_bool,&stat)) {
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
		if (cst=callrpc(hostnm,WDPROG,WDVERS,WD_RESET,
			xdr_void,0,xdr_bool,&stat)) {
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
		printf("clnt PET\n");
#endif
		if (cst=callrpc(hostnm,WDPROG,WDVERS,WD_PET,
			xdr_void,0,xdr_bool,&stat)) {
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
		}
	}

	if (cst=callrpc(hostnm,WDPROG,WDVERS,WD_DISCONNECT,
			xdr_void,0,xdr_bool,&stat)) {
		fprintf(stderr,"Unable to disconnect from watchdog on %s:",hostnm);
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		exit(1);
	}
	if (!stat) {
		fprintf(stderr,"watchdog refused DISCONNECT (not connected?)\n");
		exit(1);
	}
return 0;
}
