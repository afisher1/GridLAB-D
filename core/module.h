/* module.h
 *	Copyright (C) 2008 Battelle Memorial Institute
 */

#ifndef _MODULE_H
#define _MODULE_H

#include <stdio.h>
#include <float.h>
#include "object.h"
#include "gui.h"
#include "transform.h"

#include "test_callbacks.h"

struct s_module_list {
	void *hLib;
	char name[1024];
	CLASS *oclass;
	unsigned short major;
	unsigned short minor;
	void* (*getvar)(const char *varname,char *value,unsigned int size);
	int (*setvar)(const char *varname,char *value);
	int (*import_file)(const char *file);
	int (*export_file)(const char *file);
	int (*check)();
#ifndef _NO_CPPUNIT
	int (*module_test)(TEST_CALLBACKS *callbacks,int argc,char* argv[]);
#endif
	int (*cmdargs)(int,char**);
	int (*kmldump)(FILE*fp,OBJECT*);
	void (*test)(int argc, char *argv[]);	
	MODULE *(*subload)(char *, MODULE **, CLASS **, int, char **);
	PROPERTY *globals;
	struct s_module_list *next;
}; /* MODULE */

#ifndef _MODULE_DEFINED_
#define _MODULE_DEFINED_
typedef struct s_module_list MODULE;
#endif

typedef CLASS *(*LIBINIT)(const CALLBACKS*,void*,int,char*[]);

#ifdef __cplusplus
extern "C" {
#endif
	int module_get_exe_path(char *buf, int len);
	int module_get_path(char *buf, int len, MODULE *mod);
	MODULE *module_find(char *module_name);
	MODULE *module_load(const char *file, int argc, char *argv[]);
	void* module_getvar(MODULE *mod, const char *varname, char *value, unsigned int size);
	double *module_getvar_addr(MODULE *mod, const char *varname);
	int module_depends(char *name, unsigned char major, unsigned char minor, unsigned short build);
	int module_setvar(MODULE *mod, const char *varname, char *value);
	int module_import(MODULE *mod, const char *filename);
	int module_check(MODULE *mod);
	int module_checkall();
	int module_saveall(FILE *fp);
	int module_saveall_xml(FILE *fp);
	int module_dumpall();
	void module_libinfo(const char *module_name);
#ifndef _NO_CPPUNIT
	int module_test(TEST_CALLBACKS *callbacks,int argc,char* argv[]);
#endif
	int module_cmdargs(int argc, char **argv);
	int module_saveobj_xml(FILE *fp, MODULE *mod);
	MODULE *module_get_first();
	void *module_malloc(size_t size);
	void module_free(void *ptr);

#ifdef WIN32
	// added in module.c because it has WIN32 API
	void sched_init(void);
	void sched_clear(void);
	void sched_print(void);
	void sched_update(TIMESTAMP clock, enumeration status);
	void sched_pkill(int pid);
	unsigned short sched_get_cpuid();
	unsigned short sched_get_procid();
#endif

	int module_load_function_list(char *libname, char *fnclist);
	TRANSFORMFUNCTION module_get_transform_function(char *function);

	int module_compile(char *name, char *code, int flags, char *file, int line);
#define MC_NONE		0x0000 /**< default module_compile options */
#define MC_CLEAN	0x0001 /**< module_compile rebuilds everything (no object reuse/incremental link/etc.) */
#define MC_KEEPWORK	0x0002 /**< module_compile keeps intermediate files */
#define MC_DEBUG	0x0010 /**< module_compile builds a debug version */
#define MC_VERBOSE	0x0020 /**< module_compile shows work */

#ifdef __cplusplus
}
#endif

#endif
