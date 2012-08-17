/** $Id: main.c 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file main.c
	@author David P. Chassin

 @{
 **/
#define _MAIN_C

//#define USE_MPI

#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <direct.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "globals.h"
#include "legal.h"
#include "cmdarg.h"
#include "module.h"
#include "output.h"
#include "environment.h"
#include "test.h"
#include "random.h"
#include "realtime.h"
#include "save.h"
#include "local.h"
#include "exec.h"
#include "kml.h"
#include "kill.h"

#if defined WIN32 && _DEBUG 
/** Implements a pause on exit capability for Windows consoles
 **/
void pause_at_exit(void) 
{
	if (global_pauseatexit)
		system("pause");
}
#endif

void delete_pidfile(void)
{
	unlink(global_pidfile);
}

/** The main entry point of GridLAB-D
 Exit codes
 - \b 0 run completed ok
 - \b 1 command-line processor stopped
 - \b 2 environment startup failed
 - \b 3 test procedure failed
 - \b 4 user rejects conditions of use
 - \b 5 simulation stopped before completing
 - \b 6 initialization failed
 **/
int main(int argc, /**< the number entries on command-line argument list \p argv */
		 char *argv[]) /**< a list of pointers to the command-line arguments */
{
	int rv = 0;
	time_t t_load = time(NULL);
	char *pd1, *pd2;
	int i, pos=0;
	
	char *browser = getenv("GLBROWSER");
	char *buildinfo = strstr(BUILD,":");
	
	/* set the default timezone */
	timestamp_set_tz(NULL);

	/* set the version info */
	global_version_build = buildinfo ? atoi(strstr(BUILD,":")+1) : 0;
	strcpy(global_version_branch,BRANCH);
	global_process_id = getpid();

	/* specify the default browser */
	if ( browser!= NULL )
		strncpy(global_browser,browser,sizeof(global_browser)-1);

#if defined WIN32 && _DEBUG 
	atexit(pause_at_exit);
#endif

#ifdef WIN32
	kill_starthandler();
	atexit(kill_stophandler);
#endif

	/* capture the execdir */
	strcpy(global_execname,argv[0]);
	strcpy(global_execdir,argv[0]);
	pd1 = strrchr(global_execdir,'/');
	pd2 = strrchr(global_execdir,'\\');
	if (pd1>pd2) *pd1='\0';
	else if (pd2>pd1) *pd2='\0';

	/* determine current working directory */
	getcwd(global_workdir,1024);

	/* capture the command line */
	for (i=0; i<argc; i++)
	{
		if (pos < (int)(sizeof(global_command_line)-strlen(argv[i])))
			pos += sprintf(global_command_line+pos,"%s%s",pos>0?" ":"",argv[i]);
	}

	/* main initialization */
	if (!output_init(argc,argv) || !exec_init())
		exit(6);

	/* set thread count equal to processor count if not passed on command-line */
	if (global_threadcount == 0)
		global_threadcount = processor_count();
	output_verbose("detected %d processor(s)", processor_count());
	output_verbose("using %d helper thread(s)", global_threadcount);

	/* process command line arguments */
	if (cmdarg_load(argc,argv)==FAILED)
	{
		output_fatal("shutdown after command line rejected");
		/*	TROUBLESHOOT
			The command line is not valid and the system did not
			complete its startup procedure.  Correct the problem
			with the command line and try again.
		 */
		exit(1);
	}

	/* initialize scheduler */
	sched_init(0);

	/* recheck threadcount in case user set it 0 */
	if (global_threadcount == 0)
	{
		global_threadcount = processor_count();
		output_verbose("using %d helper thread(s)", global_threadcount);
	}

	/* see if newer version is available */
	if ( global_check_version )
		check_version(1);

	/* setup the random number generator */
	random_init();

	/* pidfile */
	if (strcmp(global_pidfile,"")!=0)
	{
		FILE *fp = fopen(global_pidfile,"w");
		if (fp==NULL)
		{
			output_fatal("unable to create pidfile '%s'", global_pidfile);
			/*	TROUBLESHOOT
				The system must allow creation of the process id file at
				the location indicated in the message.  Create and/or
				modify access rights to the path for that file and try again.
			 */
			exit(1);
		}
#ifdef WIN32
#define getpid _getpid
#endif
		fprintf(fp,"%d\n",getpid());
		output_verbose("process id %d written to %s", getpid(), global_pidfile);
		fclose(fp);
		atexit(delete_pidfile);
	}

	/* do legal stuff */
#ifdef LEGAL_NOTICE
	if (strcmp(global_pidfile,"")==0 && legal_notice()==FAILED)
		exit(4);
#endif
	
	/* start the processing environment */
	output_verbose("load time: %d sec", time(NULL) - t_load);
	output_verbose("starting up %s environment", global_environment);
	if (environment_start(argc,argv)==FAILED)
	{
		output_fatal("environment startup failed: %s", strerror(errno));
		/*	TROUBLESHOOT
			The requested environment could not be started.  This usually
			follows a more specific message regarding the startup problem.
			Follow the recommendation for the indicated problem.
		 */
		rv = 2;
	}

	/* save the model */
	if (strcmp(global_savefile,"")!=0)
	{
		if (saveall(global_savefile)==FAILED)
			output_error("save to '%s' failed", global_savefile);
	}

	/* do module dumps */
	if (global_dumpall!=FALSE)
	{
		output_verbose("dumping module data");
		module_dumpall();
	}

	/* KML output */
	if (strcmp(global_kmlfile,"")!=0)
		kml_dump(global_kmlfile);

	/* terminate */
	module_termall();

	/* wrap up */
	output_verbose("shutdown complete");

	/* profile results */
	if (global_profiler)
	{
		class_profiles();
		module_profiles();
	}

#ifdef DUMP_SCHEDULES
	/* dump a copy of the schedules for reference */
	schedule_dumpall("schedules.txt");
#endif

	/* restore locale */
	locale_pop();

	/* compute elapsed runtime */
	output_verbose("elapsed runtime %d seconds", realtime_runtime());

	exit(rv);
}

/** @} **/
