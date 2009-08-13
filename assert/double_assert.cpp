/* double_assert

   Very simple test that compares double values to any corresponding double value.  If the test 
   fails at any time, it throws a 'zero' to the commit function and breaks the simulator out with 
   a failure code.
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <complex.h>

#include "double_assert.h"

CLASS *double_assert::oclass = NULL;
double_assert *double_assert::defaults = NULL;

double_assert::double_assert(MODULE *module)
{
	if (oclass==NULL)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		oclass = gl_register_class(module,"double_assert",sizeof(double_assert),PC_PRETOPDOWN|PC_BOTTOMUP|PC_POSTTOPDOWN);

		if (gl_publish_variable(oclass,
			// TO DO:  publish your variables here
			PT_enumeration,"status",PADDR(status),
				PT_KEYWORD,"ASSERT_TRUE",ASSERT_TRUE,
				PT_KEYWORD,"ASSERT_FALSE",ASSERT_FALSE,
				PT_KEYWORD,"ASSERT_NONE",ASSERT_NONE,
			PT_double, "value", PADDR(value),
			PT_double, "within", PADDR(within),
			PT_char32, "target", PADDR(target),			
			NULL)<1) GL_THROW("unable to publish properties in %s",__FILE__);

		defaults = this;
		status = ASSERT_TRUE;
		within = 0.0;
		value = 0.0;
	}
}

/* Object creation is called once for each object that is created by the core */
int double_assert::create(void) 
{
	memcpy(this,defaults,sizeof(*this));

	return 1; /* return 1 on success, 0 on failure */
}

int double_assert::init(OBJECT *parent)
{
	if (within <= 0.0)
		GL_THROW ("A non-positive value has been specified for within.");
		/*  TROUBLESHOOT
		Within is the range in which the check is being performed.  Please check to see that you have
		specified a value for "within" and it is positive.
		*/
	return 1;
}
TIMESTAMP double_assert::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	return TS_NEVER;
}
complex *double_assert::get_complex(OBJECT *obj, char *name)
{
	PROPERTY *p = gl_get_property(obj,name);
	if (p==NULL || p->ptype!=PT_complex)
		return NULL;
	return (complex*)GETADDR(obj,p);
}

EXPORT int create_double_assert(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(double_assert::oclass);
		if (*obj!=NULL)
		{
			double_assert *my = OBJECTDATA(*obj,double_assert);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_double_assert: %s", msg);
	}
	return 1;
}



EXPORT int init_double_assert(OBJECT *obj, OBJECT *parent) 
{
	try 
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,double_assert)->init(parent);
	}
	catch (char *msg)
	{
		gl_error("init_double_assert(obj=%d;%s): %s", obj->id, obj->name?obj->name:"unnamed", msg);
	}
	return 0;
}

EXPORT TIMESTAMP sync_double_assert(OBJECT *obj, TIMESTAMP t0)
{
	double_assert *my = OBJECTDATA(obj,double_assert);
	TIMESTAMP t1 = my->postsync(obj->clock, t0);
	obj->clock = t0;
	return t1;
}
EXPORT int commit_double_assert(OBJECT *obj)
{
	//OBJECT *obj;
	char buff[64];
	double_assert *da = OBJECTDATA(obj,double_assert);

	//bj = OBJECTHDR(this);
	
		
		double *x = (double*)gl_get_double_by_name(obj->parent,da->target);
		if (x==NULL) 
		{
			GL_THROW("Specified target %s for %s is not valid.",da->target,gl_name(obj->parent,buff,64));
			/*  TROUBLESHOOT
			Check to make sure the target you are specifying is a published variable for the object
			that you are pointing to.  Refer to the documentation of the command flag --modhelp, or 
			check the wiki page to determine which variables can be published within the object you
			are pointing to with the assert function.
			*/
			return 0;
		}
		else if (da->status == da->ASSERT_TRUE)
		{
			double m = abs(*x-da->value);
			if (_isnan(m) || m>da->within)
			{				
				gl_verbose("Assert failed on %s: %s %g not within %f of given value %g", 
					gl_name(obj->parent, buff, 64), da->target, *x, da->within, da->value);
				return 0;
			}
			gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
			return 1;
		}
		else if (da->status == da->ASSERT_FALSE)
		{
			double m = abs(*x-da->value);
			if (_isnan(m) || m<da->within)
			{				
				gl_verbose("Assert failed on %s: %s %g is within %f of given value %g", 
					gl_name(obj->parent, buff, 64), da->target, *x, da->within, da->value);
				return 0;
			}
			gl_verbose("Assert passed on %s", gl_name(obj->parent, buff, 64));
			return 1;
		}
		else
		{
			gl_verbose("Assert test is not being run on %s", gl_name(obj->parent, buff, 64));
			return 1;
		}

}