
#ifndef _ENDUSE_H
#define _ENDUSE_H

#include "class.h"
#include "timestamp.h"
#include "loadshape.h"

#define EUF_IS220 0x0001 ///< enduse flag to indicate that the voltage is line-to-line, not line-to-neutral

typedef struct s_enduse {
	char *name;
	loadshape *shape;
	long flags;
	complex power;				/* power in kW */
	complex energy;				/* total energy in kWh */
	complex demand;				/* maximum power in kW (can be reset) */
	double impedance_fraction;	/* constant impedance fraction (pu load) */
	double current_fraction;	/* constant current fraction (pu load) */
	double power_fraction;		/* constant power fraction (pu load)*/
	double power_factor;		/* power factor */
	double voltage_factor;		/* voltage factor (pu nominal) */
	double heatgain;			/* internal heat from load (Btu/h) */
	double heatgain_fraction;	/* fraction of power that goes to internal heat (pu Btu/h) */

	struct s_enduse *next;
} enduse;

int enduse_create(void *addr);
int enduse_init(enduse *e);
int enduse_initall(void);
TIMESTAMP enduse_sync(enduse *e, PASSCONFIG pass, TIMESTAMP t0, TIMESTAMP t1);
TIMESTAMP enduse_syncall(TIMESTAMP t1);
int convert_to_enduse(char *string, void *data, PROPERTY *prop);
int convert_from_enduse(char *string,int size,void *data, PROPERTY *prop);
int enduse_publish(CLASS *oclass, int struct_address, char *prefix);
int enduse_test(void);

#endif
