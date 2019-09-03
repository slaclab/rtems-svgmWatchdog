
/* watchdog RPC service
 * by Till Straumann <strauman@slac.stanford.edu>, Oct. 2000
 *    9/09/2002: RTEMS port, 
 */

#include <rtems.h>
#include <unistd.h>
#include "wrap.h"
#include <cexpHelp.h>
#include "rtemsBspWatchdog.h"

int
_cexpModuleFinalize(void *mod)
{
extern rtems_id wdTaskId;

int polls = 5;

	wdStop();

	for (polls = 5; 0 != wdTaskId && polls>=0; polls--)
			sleep(1);

	if (polls<0) {
		fprintf(stderr,"Watchdog won't die, refusing to unload\n");
		return -1;
	}
	return 0;
}
