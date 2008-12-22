/** $Id: aggregate.c 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file aggregate.c
	@addtogroup aggregate Aggregation of object properties
	@ingroup core
	
	Aggregation functions support calculations over properties of multiple objects.
	This is used primarily by the collector object in the tape module to define groups
	(see the \p group property in the collector object).  The \p group can be defined
	by specifying a boolean series are relationship of object properties, e.g.,
	\verbatim class=node and parent=root \endverbatim.
	
	Aggregations also must specify	the property that is to be aggregated.  Most common
	aggregations and some uncommon ones are supported.  In addition, if the aggregation is
	over a complex quantity, the aggregation must specific how a double is to be obtained
	from it, e.g., magnitude, angle, real, imaginary.  Some examples aggregate \p property 
	definitions are
	\verbatim
	sum(cost)
	max(power.angle)
	mean(energy.mag)
	std(price)
	\endverbatim

	@bug Right now, not all allowed aggregations are invariant (meaning that the members of the group
	do not change over time).  However, the collector object requires invariant aggregations.  Using 
	an aggregation that isn't invariant will cause the simulation to fail. (ticket #112)
 @{
 **/

#include <ctype.h>
#include <math.h>
#include "platform.h"
#include "aggregate.h"
#include "output.h"
#include "find.h"

/** This function builds an collection of objects into an aggregation.  
	The aggregation can be run using aggregate_value(AGGREGATION*)
 **/
AGGREGATION *aggregate_mkgroup(char *aggregator, /**< aggregator (min,max,avg,std,sum,prod,mbe,mean,var,kur,count,gamma) */
							   char *group_expression) /**< grouping rule; see find_mkpgm(char *)*/
{
	AGGREGATOR op = AGGR_NOP;
	AGGREGATION *result=NULL;
	char aggrop[9], aggrval[33], aggrpart[9]="";
	unsigned char flags=0x00;
	if (sscanf(aggregator,"%8[A-Za-z0-9_](%32[A-Za-z0-9_].%8[A-Za-z0-9_])",aggrop,aggrval,aggrpart)!=3 &&
		sscanf(aggregator,"%8[A-Za-z0-9_](%32[A-Za-z0-9_])",aggrop,aggrval)!=2 &&
		(flags|=AF_ABS,
		sscanf(aggregator,"%8[A-Za-z0-9_]|%32[A-Za-z0-9_].%8[A-Za-z0-9_]|",aggrop,aggrval,aggrpart)!=3) &&
		sscanf(aggregator,"%8[A-Za-z0-9_]|%32[A-Za-z0-9_]|",aggrop,aggrval)!=2 
		)
	{
		output_error("aggregate group '%s' is not valid", aggregator);
		errno = EINVAL;
		return NULL;
	}

	if (stricmp(aggrop,"min")==0) op=AGGR_MIN;
	else if (stricmp(aggrop,"max")==0) op=AGGR_MAX;
	else if (stricmp(aggrop,"avg")==0) op=AGGR_AVG;
	else if (stricmp(aggrop,"std")==0) op=AGGR_STD;
	else if (stricmp(aggrop,"sum")==0) op=AGGR_SUM;
	else if (stricmp(aggrop,"prod")==0) op=AGGR_SUM;
	else if (stricmp(aggrop,"mbe")==0) op=AGGR_MBE;
	else if (stricmp(aggrop,"mean")==0) op=AGGR_MEAN;
	else if (stricmp(aggrop,"var")==0) op=AGGR_VAR;
	else if (stricmp(aggrop,"kur")==0) op=AGGR_KUR;
	else if (stricmp(aggrop,"count")==0) op=AGGR_COUNT;
	else if (stricmp(aggrop,"gamma")==0) op=AGGR_GAMMA;
	else
	{
		output_error("aggregate group '%s' does not use a known aggregator", aggregator);
		errno = EINVAL;
		return NULL;
	}
	if (op!=AGGR_NOP)
	{
		PROPERTY *pinfo=NULL;
		AGGRPART part=AP_NONE;
		FINDLIST *list=NULL;

		/* compile and check search program */
		FINDPGM *pgm = find_mkpgm(group_expression);
		if (pgm==NULL)
		{
			output_error("aggregate group expression '%s' is not valid", group_expression);
			errno = EINVAL;
			return NULL;
		}
		else
		{
			PGMCONSTFLAGS flags = find_pgmconstants(pgm); 
			
			/* the search must be over the same class so that the property offset is known in advance */
			if ((flags&CF_CLASS)!=CF_CLASS)
			{
				output_error("aggregate group expression '%s' does not result in a set with a fixed class", group_expression);
				errno = EINVAL;
				free(pgm);
				return NULL;
			}
			else
			{
				OBJECT *obj;
				list = find_runpgm(NULL,pgm);
				if (list==NULL)
				{
					output_error("aggregate group expression '%s' does not result is a usable object list", group_expression);
					free(pgm);
					errno=EINVAL;
					return NULL;
				}
				obj = find_first(list);
				if (obj==NULL)
				{
					output_error("aggregate group expression '%s' results is an empty object list", group_expression);
					free(pgm);
					free(list);
					errno=EINVAL;
					return NULL;
				}
				pinfo = class_find_property(obj->oclass,aggrval);
				if (pinfo==NULL)
				{
					output_error("aggregate group property '%s' is not found in the objects satisfying search criteria '%s'", aggrval, group_expression);
					errno = EINVAL;
					free(pgm);
					free(list);
					return NULL;
				}
				else if (pinfo->ptype==PT_double)
				{
					if (strcmp(aggrpart,"")!=0)
					{	/* doubles cannot have parts */
						output_error("aggregate group property '%s' cannot have part '%s'", aggrval, aggrpart);
						errno = EINVAL;
						free(pgm);
						free(list);
						return NULL;
					}
					part = AP_NONE;
				}
				else if (pinfo->ptype==PT_complex)
				{	/* complex must have parts */
					if (strcmp(aggrpart,"real")==0)
						part = AP_REAL;
					else if (strcmp(aggrpart,"imag")==0)
						part = AP_IMAG;
					else if (strcmp(aggrpart,"mag")==0)
						part = AP_MAG;
					else if (strcmp(aggrpart,"ang")==0)
						part = AP_ANG;
					else if (strcmp(aggrpart,"arg")==0)
						part = AP_ARG;
					else
					{
						output_error("aggregate group property '%s' cannot have part '%s'", aggrval, aggrpart);
						errno = EINVAL;
						free(pgm);
						free(list);
						return NULL;
					}
				}
				else
				{
					output_error("aggregate group property '%s' cannot be aggregated", aggrval);
					errno = EINVAL;
					free(pgm);
					free(list);
					return NULL;
				}
			}
		}

		/* build aggregation unit */
		result = (AGGREGATION*)malloc(sizeof(AGGREGATION));
		if (result!=NULL)
		{
			result->op = op;
			result->group = pgm;
			result->pinfo = pinfo;
			result->part = part;
			result->last = list;
			result->next = NULL;
			result->flags = flags;
		}
		else
		{
			errno=ENOMEM;
			free(pgm);
			free(list);
			return NULL;
		}
	}

	return result;
}

double mag(complex *x)
{
	return sqrt(x->r*x->r + x->i*x->i);
}

double arg(complex *x)
{
	return (x->r==0) ? (x->i>0 ? PI/2 : (x->i==0 ? 0 : -PI/2)) : ((x->i>0) ? (x->r>0 ? atan(x->i/x->r) : PI-atan(x->i/x->r)) : (x->r>0 ? -atan(x->i/x->r) : PI+atan(x->i/x->r)));
}

/** This function performs an aggregate calculation given by the aggregation 
 **/
double aggregate_value(AGGREGATION *aggr) /**< the aggregation to perform */
{
	OBJECT *obj;
	double numerator=0, denominator=0, secondary=0;

	/* non-constant groups need search program rerun */
	if ((aggr->group->constflags&CF_CONSTANT)!=CF_CONSTANT)
		aggr->last = find_runpgm(NULL,aggr->group); /** @todo use constant part instead of NULL (ticket #14) */
	for (obj=find_first(aggr->last); obj!=NULL; obj=find_next(aggr->last,obj))
	{
		double value=0;
		double *pdouble = NULL;
		complex *pcomplex = NULL;
		switch (aggr->pinfo->ptype) {
		case PT_complex:
			pcomplex = object_get_complex(obj,aggr->pinfo);
			if (pcomplex!=NULL)
			{
				switch (aggr->part) {
				case AP_REAL: value=pcomplex->r; break;
				case AP_IMAG: value=pcomplex->i; break;
				case AP_MAG: value=mag(pcomplex); break;
				case AP_ARG: value=arg(pcomplex); break;
				case AP_ANG: value=arg(pcomplex)*180/PI;  break;
				default: pcomplex = NULL; break; /* invalidate the result */
				}
			}
			break;
		case PT_double:
			pdouble = object_get_double(obj,aggr->pinfo);
			if (pdouble!=NULL)
				value = *pdouble;
			break;
		default:
			break;
		}
		if (pdouble!=NULL || pcomplex!=NULL) /* valid value */
		{
			if ((aggr->flags&AF_ABS)==AF_ABS) value=fabs(value);
			switch (aggr->op) {
			case AGGR_MIN:
				if (value<numerator || denominator==0) numerator=value;
				denominator = 1;
				break;
			case AGGR_MAX:
				if (value>numerator || denominator==0) numerator=value;
				denominator = 1;
				break;
			case AGGR_COUNT:
				numerator++;
				denominator=1;
				break;
			case AGGR_MBE:
			case AGGR_AVG:
			case AGGR_MEAN:
				numerator+=value;
				denominator++;
				break;
			case AGGR_SUM:
				numerator+=value;
				denominator = 1;
				break;
			case AGGR_PROD:
				numerator*=value;
				denominator = 1;
				break;
			case AGGR_GAMMA:
				denominator+=log(value);
				if (numerator==0 || secondary>value)
					secondary = value;
				numerator++;
				break;
			case AGGR_STD:
			case AGGR_VAR:
				denominator++;
				numerator += value;
				secondary += value*value;
				break;
			case AGGR_KUR:
				/* not yet supported (see below for todos) */
			default:
				break;
			}
		}
	}
	switch (aggr->op) {
		double v = 0.0, t = 0.0, m = 0.0;
	case AGGR_GAMMA:
		return 1 + numerator/(denominator-numerator*log(secondary));
	case AGGR_STD:
		return sqrt((secondary + numerator*numerator/denominator)/(denominator-1));
	case AGGR_VAR:
		return (secondary + numerator*numerator/denominator) / (denominator-1);
	case AGGR_MBE:
		v = 0.0;
		m = numerator/denominator;
		for (obj=find_first(aggr->last); obj!=NULL; obj=find_next(aggr->last,obj)){
			t = *(object_get_double(obj,aggr->pinfo)) - m;
			v += ((t > 0) ? t : -t);
		}
		return v/denominator;
	case AGGR_KUR:
		/** @todo implement kurtosis aggregate (no ticket) */
		throw_exception("kurtosis aggregation is not implemented");
	default:
		return numerator/denominator;
	}
}
/**@}**/
