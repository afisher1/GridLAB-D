/** $Id$

 General purpose assert objects

 **/

#ifndef _g_assert_H
#define _g_assert_H

#include <stdarg.h>
#include "gridlabd.h"

#ifndef _isnan
#define _isnan isnan
#endif

class g_assert : public gld_object {
public:
	typedef enum {AS_INIT=0, AS_TRUE=1, AS_FALSE=2, AS_NONE=3} ASSERTSTATUS;

	GL_ATOMIC(ASSERTSTATUS,status); 
	GL_STRING(char1024,target);		
	GL_ATOMIC(PROPERTYCOMPAREOP,relation);
	GL_STRING(char1024,value);
	GL_STRING(char1024,value2);

private:
	ASSERTSTATUS evaluate_status(void);

public:
	/* required implementations */
	g_assert(MODULE *module);
	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP commit(TIMESTAMP t1, TIMESTAMP t2);
	int postnotify(PROPERTY *prop, char *value);

public:
	static CLASS *oclass;
	static g_assert *defaults;
};
#endif