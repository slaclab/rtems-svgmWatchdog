#include <rpc/rpc.h>
#include <stdio.h>
#include <netdb.h>


#include <signal.h>

#include "wd.h"

static void
usage(char *n)
{
	fprintf(stderr,"usage: %s hostname\n",n);
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
bool_t stat;
enum clnt_stat cst;
	if (argc<2) {
		usage(argv[0]);
		exit(1);
	}
	if (cst=callrpc(argv[1],WDPROG,WDVERS,WD_CONNECT,
			xdr_void,0,xdr_bool,&stat)) {
		fprintf(stderr,"Unable to connect to watchdog on %s:",argv[1]);
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		exit(1);
	}
	if (!stat) {
		fprintf(stderr,"watchdog on %s refused connection (already connected?)\n",argv[1]);
		exit(1);
	}
	fprintf(stderr,"Connected to watchdog on %s\n",argv[1]);

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
		if (cst=callrpc(argv[1],WDPROG,WDVERS,WD_RESET,
			xdr_void,0,xdr_bool,&stat)) {
		fprintf(stderr,"Unable to force target reset (rpc failed)");
		clnt_perrno(cst);
		fprintf(stderr,"\n");
		terminate=0;
		}
		}
		if (!terminate) {
#ifdef DEBUG
		printf("clnt PET\n");
#endif
		if (cst=callrpc(argv[1],WDPROG,WDVERS,WD_PET,
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

	if (cst=callrpc(argv[1],WDPROG,WDVERS,WD_DISCONNECT,
			xdr_void,0,xdr_bool,&stat)) {
		fprintf(stderr,"Unable to disconnect from watchdog on %s:",argv[1]);
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
