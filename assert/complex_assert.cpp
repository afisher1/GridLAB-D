/* complex_assert

   Very simple test that compares complex values to any corresponding complex value.  It breaks the
   tests down into a test for the real value and a test for the imagniary portion.  If either test 
   fails at any time, it throws a 'zero' to the commit function and breaks the simulator out with 
   a failure code.
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <complex.h>

#include "complex_assert.h"

EXPORT_CREATE(complex_assert);
EXPORT_INIT(complex_assert);
EXPORT_COMMIT(complex_assert);
EXPORT_NOTIFY(complex_assert);

CLASS *complex_assert::oclass = NULL;
complex_assert *complex_assert::defaults = NULL;

complex_assert::complex_assert(MODULE *module)
{
	if (oclass==NULL)
	{
		// register to receive notice for first top down. bottom up, and second top down synchronizations
		oclass = gl_register_class(module,"complex_assert",sizeof(complex_assert),0x00);
		if (oclass==NULL)
			throw "unable to register class complex_assert";
		else
			oclass->trl = TRL_PROVEN;

		if (gl_publish_variable(oclass,
			// TO DO:  publish your variables here
			PT_enumeration,"status",get_status_offset(),
				PT_KEYWORD,"ASSERT_TRUE",ASSERT_TRUE,
				PT_KEYWORD,"ASSERT_FALSE",ASSERT_FALSE,
				PT_KEYWORD,"ASSERT_NONE",ASSERT_NONE,
			PT_enumeration, "once", get_once_offset(),
				PT_KEYWORD,"ONCE_FALSE",ONCE_FALSE,
				PT_KEYWORD,"ONCE_TRUE",ONCE_TRUE,
				PT_KEYWORD,"ONCE_DONE",ONCE_DONE,
			PT_enumeration, "operation", get_operation_offset(),
				PT_KEYWORD,"FULL",FULL,
				PT_KEYWORD,"REAL",REAL,
				PT_KEYWORD,"IMAGINARY",IMAGINARY,
				PT_KEYWORD,"MAGNITUDE",MAGNITUDE,
				PT_KEYWORD,"ANGLE",ANGLE, // If using, please specify in radians
			PT_complex, "value", get_value_offset(),
			PT_double, "within", get_within_offset(),
			PT_char1024, "target", get_target_offset(),	
			NULL)<1){
				char msg[256];
				sprintf(msg, "unable to publish properties in %s",__FILE__);
				throw msg;
		}

		defaults = this;
		status = ASSERT_TRUE;
		within = 0.0;
		value = 0.0;
		once = ONCE_FALSE;
		once_value = 0;
		operation = FULL;
	}
}

/* Object creation is called once for each object that is created by the core */
int complex_assert::create(void) 
{
	memcpy(this,defaults,sizeof(*this));

	return 1; /* return 1 on success, 0 on failure */
}

int complex_assert::init(OBJECT *parent)
{
	if (within <= 0.0){
		throw "A non-positive value has been specified for within.";
		/*  TROUBLESHOOT
		Within is the range in which the check is being performed.  Please check to see that you have
		specified a value for "within" and it is positive.
		*/
	}
	return 1;
}

TIMESTAMP complex_assert::commit(TIMESTAMP t1, TIMESTAMP t2)
{
	// handle once mode
	if (get_once() == ONCE_TRUE)
	{
		set_once_value(get_value());
		set_once(ONCE_DONE);
	} 
	else if (get_once() == ONCE_DONE)
	{
		if(get_once_value() == get_value()){
			gl_verbose("Assert skipped with ONCE logic");
			return TS_NEVER;
		} else {
			set_once_value(get_value());
		}
	}

	// get the target property
	gld_property target(get_parent(),get_target());
	if ( !target.is_valid() || target.get_type()!=PT_complex ) {
		gl_error("Specified target %s for %s is not valid.",get_target(),get_parent()->get_name());
		/*  TROUBLESHOOT
		Check to make sure the target you are specifying is a published variable for the object
		that you are pointing to.  Refer to the documentation of the command flag --modhelp, or 
		check the wiki page to determine which variables can be published within the object you
		are pointing to with the assert function.
		*/
		return 0;
	}

	// test the target value
	complex x; target.getp(x);
	if (get_status() == ASSERT_TRUE)
	{
		if (get_operation() == FULL 
			|| get_operation() == REAL 
			|| get_operation() == IMAGINARY
			)
		{	
			complex error = x - get_value();
			double real_error = error.Re();
			double imag_error = error.Im();
	
			if ((_isnan(real_error) || fabs(real_error)>get_within()) 
				&& (get_operation() == FULL || get_operation() == REAL)
				)
			{
				gl_verbose("Assert failed on %s: real part of %s %g not within %f of given value %g", 
				get_parent()->get_name(), get_target(), x.Re(), get_within(), get_value().Re());

				return 0;
			}
			if ((_isnan(imag_error) || fabs(imag_error)>get_within()) && (get_operation() == FULL || get_operation() == IMAGINARY))
			{
				gl_verbose("Assert failed on %s: imaginary part of %s %+gi not within %f of given value %+gi", 
				get_parent()->get_name(), get_target(), x.Im(), get_within(), get_value().Im());

				return 0;
			}
		}
		else if (get_operation() == MAGNITUDE)
		{
			double magnitude_error = x.Mag() - get_value().Mag();

			if ( _isnan(magnitude_error) || fabs(magnitude_error) > get_within() )
			{
				gl_verbose("Assert failed on %s: Magnitude of %s (%g) not within %f of given value %g", 
				get_parent()->get_name(), get_target(), x.Mag(), get_within(), get_value().Mag());

				return 0;
			}
		}
		else if (get_operation() == ANGLE)
		{
			double angle_error = x.Arg() - get_value().Arg();

			if ( _isnan(angle_error) || fabs(angle_error) > get_within() )
			{
				gl_verbose("Assert failed on %s: Angle of %s (%g) not within %f of given value %g", 
				get_parent()->get_name(), get_target(), x.Arg(), get_within(), get_value().Arg());

				return 0;
			}
		}
	}
	else if (get_status() == ASSERT_FALSE)
	{
		complex error = x - get_value();
		double real_error = error.Re();
		double imag_error = error.Im();
		if ((_isnan(real_error) || fabs(real_error)<get_within())||(_isnan(imag_error) || fabs(imag_error)<get_within())){
			if (_isnan(real_error) || fabs(real_error)<get_within()) {
				gl_verbose("Assert failed on %s: real part of %s %g is within %f of %g", 
				get_parent()->get_name(), get_target(), x.Re(), get_within(), get_value().Re());
			}
			if (_isnan(imag_error) || fabs(imag_error)<get_within()) {
				gl_verbose("Assert failed on %s: imaginary part of %s %+gi is within %f of %gi", 
				get_parent()->get_name(), get_target(), x.Im(), get_within(), get_value().Im());
			}
			return 0;
		}
	}
	else
	{
		gl_verbose("Assert test is not being run on %s", get_parent()->get_name());
		return TS_NEVER;
	}
	gl_verbose("Assert passed on %s",get_parent()->get_name());
	return TS_NEVER;
}

int complex_assert::postnotify(PROPERTY *prop, char *value)
{
	if ( get_once()==ONCE_DONE && strcmp(prop->name, "value")==0 )
	{
		set_once(ONCE_TRUE);
	}
	return 1;
}
