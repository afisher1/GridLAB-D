// $Id: link.h 1182 2008-12-22 22:08:36Z dchassin $
//	Copyright (C) 2008 Battelle Memorial Institute

#ifndef _LINK_H
#define _LINK_H

#include "lock.h"
#include "powerflow.h"

EXPORT int isa_link(OBJECT *obj, char *classname);

#define impedance(X) (B_mat[X][X])

class link : public powerflow_object
{
public: /// @todo make this private and create interfaces to control values
	complex a_mat[3][3];   // a_mat - 3x3 matrix, 'a' matrix
	complex b_mat[3][3];   // b_mat - 3x3 matrix, 'b' matrix
	complex c_mat[3][3];   // c_mat - 3x3 matrix, 'c' matrix
	complex d_mat[3][3];   // d_mat - 3x3 matrix, 'd' matrix
	complex A_mat[3][3];   // A_mat - 3x3 matrix, 'A' matrix
	complex B_mat[3][3];   // B_mat - 3x3 matrix, 'B' matrix
	complex To_Y[3][3];	   // To_Y  - 3x3 matrix, object to admittance
	complex From_Y[3][3];  // From_Y - 3x3 matrix, object from admittance
	double voltage_ratio;	   // voltage ratio (normally 1.0)
	complex phaseadjust;	//Phase adjustment term for GS transformers

public:
	typedef enum {LS_CLOSED=0, LS_OPEN=1} LINKSTATUS;
	LINKSTATUS status;	///< link status (open disconnect nodes)
	OBJECT *from;			///< from_node - source node
	OBJECT *to;				///< to_node - load node
	complex current_in[3];		///< current flow to link (w.r.t from node)
	complex current_out[3];	///< current flow out of link (w.r.t. to node)
	double power_in;		///< power flow from link (w.r.t from node)
	double power_out;		///< power flow to link (w.r.t to node)

	int create(void);
	int init(OBJECT *parent);
	TIMESTAMP presync(TIMESTAMP t0);
	TIMESTAMP sync(TIMESTAMP t0);
	TIMESTAMP postsync(TIMESTAMP t0);
	link(MODULE *mod);
	link(CLASS *cl=oclass):powerflow_object(cl){};
	static CLASS *oclass;
	static CLASS *pclass;
	int isa(char *classname);
public:
	/* status values */
	set affected_phases;				/* use this to determine which phases are affected by status change */
	#define IMPEDANCE_CHANGED		1	/* use this status to indicate an impedance change (e.g., line contact) */
	double resistance;					/* use this resistance when status=IMPEDANCE_CHANGED */
	#define LINE_CONTACT			2	/* use this to indicate line contact */
	set line_contacted;					/* use this to indicate which line was contacted (N means ground) */
	#define CONTROL_FAILED			4	/* use this status to indicate a controller failure (e.g., PLC failure) */

	class node *get_from(void) const;
	class node *get_to(void) const;
	set get_flow(class node **from, class node **to) const; /* determine flow direction (return phases on which flow is reverse) */

	inline LINKSTATUS open(void) { LINKSTATUS previous=status; status=LS_OPEN; return previous;};
	inline LINKSTATUS close(void) { LINKSTATUS previous=status; status=LS_CLOSED; return previous;};
	inline bool is_open(void) const { return status==LS_OPEN;};
	inline bool is_closed(void) const { return status==LS_CLOSED;};
	inline LINKSTATUS get_status(void) const {return status;};

	bool is_frequency_nominal();
	bool is_voltage_nominal();

	int kmldump(FILE *fp);

	void *UpdateYVs(OBJECT *fobj, complex *deltaV);
};

void inverse(complex in[3][3], complex out[3][3]);
void minverter(complex in[3][3], complex out[3][3]);
void multiply(double a, complex b[3][3], complex c[3][3]);
void multiply(complex a[3][3], complex b[3][3], complex c[3][3]);
void subtract(complex a[3][3], complex b[3][3], complex c[3][3]);
void addition(complex a[3][3], complex b[3][3], complex c[3][3]);
void equalm(complex a[3][3], complex b[3][3]);
#endif // _LINK_H

