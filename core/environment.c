/** $Id: environment.c 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file environment.c
	@addtogroup environment Environment control
	@ingroup core
	
	Environment function manage the interface with the user environment.
	The default user environment is \p batch.  The \p matlab environment
	is currently under development is has only a minimal interface.

	@todo Finish the \p matlab environment (ticket #18)
 @{
 **/

#include <stdlib.h>
#include <string.h>

#include "environment.h"
#include "exec.h"
#include "save.h"
#include "matlab.h"

/** Starts the environment selected by the global_environment variable
 **/
STATUS environment_start(int argc, /**< the number of arguments to pass to the environment */
						 char *argv[]) /**< the arguments to pass to the environment */
{
	if (strcmp(global_environment,"batch")==0)
	{
		/* do the run */
		if (exec_start()==FAILED)
		{
			output_fatal("shutdown after simulation stopped prematurely");
			if (global_dumpfile[0]!='\0')
			{
				if (!saveall(global_dumpfile))
					output_error("dump to '%s' failed", global_dumpfile);
				else
					output_message("dump to '%s' complete", global_dumpfile);
			}
			// exit(5);
		}
		return SUCCESS;
	}
	else if (strcmp(global_environment,"matlab")==0)
	{
		output_verbose("starting Matlab");
		return matlab_startup(argc,argv);
	}
	else
	{
		output_fatal("%s environment not recognized or supported",global_environment);
		return FAILED;
	}
}

/**@}*/
