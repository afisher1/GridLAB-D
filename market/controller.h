/** $Id: controller.h 1182 2009-09-09 22:08:36Z mhauer $
	Copyright (C) 2009 Battelle Memorial Institute
	@file controller.h
	@addtogroup controller
	@ingroup market

 **/

#ifndef _controller_H
#define _controller_H

#include <stdarg.h>
#include "auction.h"
#include "gridlabd.h"

class controller {
public:
	controller(MODULE *);
	int create(void);
	int init(OBJECT *parent);
	int isa(char *classname);
	TIMESTAMP presync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP sync(TIMESTAMP t0, TIMESTAMP t1);
	TIMESTAMP postsync(TIMESTAMP t0, TIMESTAMP t1);
	static CLASS *oclass;
public:
	typedef enum {
		SM_NONE,
		SM_HOUSE_HEAT,
		SM_HOUSE_COOL,
		SM_HOUSE_PREHEAT,
		SM_HOUSE_PRECOOL,
		SM_WATERHEATER,
	} SIMPLE_MODE;
	SIMPLE_MODE simplemode;
	
	typedef enum {
		BM_OFF,
		BM_ON,
	} BIDMODE;
	BIDMODE bidmode;

	typedef enum {
		CN_RAMP,
		CN_DOUBLE_RAMP,
	} CONTROLMODE;
	CONTROLMODE control_mode;
	
	typedef enum {
		RM_DEADBAND,
		RM_SLIDING,
	} RESOLVEMODE;
	RESOLVEMODE resolve_mode;

	double kT_L, kT_H, Tmin, Tmax;
	char target[33];
	char setpoint[33];
	char demand[33];
	char total[33];
	char load[33];
	char state[33];
	char32 avg_target;
	char32 std_target;
	OBJECT *pMarket;
	auction *market;
	int64 lastbid_id;
	int64 lastmkt_id;
	double last_p;
	double last_q;
	double set_temp;
	int may_run;

	// new stuff
	double ramp_low, ramp_high;
	int64 period;
	double slider_setting_heat;
	double slider_setting_cool;
	double range_low;
	double range_high;
private:
	TIMESTAMP next_run;
	double *pMonitor;
	double *pSetpoint;
	double *pDemand;
	double *pTotal;
	double *pLoad;
	double *pAvg;
	double *pStd;
	enumeration *pState;
	double setpoint0;
	void cheat();
	void fetch(double **prop, char *name, OBJECT *parent);
	int dir;
	double min, max;
	double T_lim, k_T;
};

#endif // _controller_H

// EOF
