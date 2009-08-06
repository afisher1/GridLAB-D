/** $Id$
 	Copyright (C) 2008 Battelle Memorial Institute
	@file loadshape.h
	@addtogroup loadshape Built-in loadshapes
**/

#ifndef _SFSM_H
#define _SFSM_H

#include "schedule.h"
#include "timestamp.h"
#include "class.h"

typedef struct s_loadshape loadshape;

typedef enum {
	MT_UNKNOWN=0,
	MT_ANALOG,		/**< machine output an analog signal */
	MT_PULSED,		/**< machine outputs pulses of fixed area with varying frequency to match value */
	MT_MODULATED,	/**< machine outputs pulses of fixed frequency with varying area to match value */
	MT_QUEUED,		/**< machine accrues values and output pulses of fixed area with varying frequency */
} MACHINETYPE; /** type of machine */
typedef enum {
	MPT_UNKNOWN=0,
	MPT_TIME,		/**< pulses are of fixed time (either total period or on-time duration); power is energy/duration */
	MPT_POWER,		/**< pulses are of fixed power; duration is energy/power */
} MACHINEPULSETYPE; /**< the type of pulses generated by the machine */
typedef enum {
	MS_OFF=0,
	MS_ON=1,
} MACHINESTATE;
struct s_loadshape {
	double load;		/**< the actual load magnitude */
	
	/* machine specification */
	SCHEDULE *schedule;	/**< the schedule driving this machine */
	MACHINETYPE type;	/**< the type of this machine */
	union {
		struct {
			double energy;		/**< the total energy used over the shape (0 if scalar is used) */
			double power;		/**< the power scaling factor of the shape (0 if energy is used) */
		} analog;
		struct {
			double energy;		/**< the total energy used over the shape */
			double scalar;		/**< the number of pulses over the shape */
			MACHINEPULSETYPE pulsetype;	/**< the fixed part of the pulse (time or power) */
			double pulsevalue;	/**< the value of the fixed part of the pulse */
		} pulsed;
		struct {
			double energy;		/**< the total energy used over the shape */
			double scalar;		/**< the number of pulses over the shape */
			MACHINEPULSETYPE pulsetype;	/**< the fixed part of the pulse (time or power) */
			double pulsevalue;	/**< the value of the fixed part of the pulse */
		} modulated;
		struct {
			double energy;		/**< the total energy used over the shape */
			double scalar;		/**< the number of pulses over the shape */
			MACHINEPULSETYPE pulsetype;	/**< the fixed part of the pulse (time or power) */
			double pulsevalue;	/**< the value of the fixed part of the pulse */
			double q_on, q_off;	/**< the queue thresholds (in units of 1 pulse) */
		} queued;
	} params;	/**< the machine parameters (depends on #type) */

	/* internal machine parameters */
	double r;			/**< the state rate */
	double re[2];		/**< the state rate stdevs (not used yet) */ 
	double d[2];		/**< the state transition thresholds */
	double de[2];		/**< the state transition threshold stdevs (not used yet) */
	double dPdV;		/**< the voltage sensitivity of the load */

	/* state variables */
	double q;			/**< the internal state of the machine */
	MACHINESTATE s;		/**< the current state of the machine (0 or 1) */
	TIMESTAMP t0;	/**< time of last update (in seconds since epoch) */
	TIMESTAMP t2;	/**< time of next update (in seconds since epoch) */

	struct s_loadshape *next;	/* next loadshape in list */
};

int loadshape_create(void *shape);
int loadshape_init(loadshape *shape);
int loadshape_initall(void);
TIMESTAMP loadshape_sync(loadshape *m, TIMESTAMP t1);
TIMESTAMP loadshape_syncall(TIMESTAMP t1);

int loadshape_test(void);

int convert_from_loadshape(char *string,int size,void *data, PROPERTY *prop); /**< convert from a loadshape to a string */
int convert_to_loadshape(char *string, void *data, PROPERTY *prop); /**< convert from a string to a loadshape */


#endif
