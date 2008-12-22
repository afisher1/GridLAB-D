/** $Id: aggregate.h 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file aggregate.h
	@addtogroup aggregate
 @{
 **/

#ifndef _AGGREGATE_H
#define _AGGREGATE_H

#include "platform.h"
#include "find.h"

typedef enum {AGGR_NOP, AGGR_MIN, AGGR_MAX, AGGR_AVG, AGGR_STD, AGGR_MBE, AGGR_MEAN, AGGR_VAR, AGGR_KUR, AGGR_GAMMA, AGGR_COUNT, AGGR_SUM, AGGR_PROD} AGGREGATOR; /**< the aggregation method to use */
typedef enum {AP_NONE, AP_REAL, AP_IMAG, AP_MAG, AP_ANG, AP_ARG} AGGRPART; /**< the part of complex values to aggregate */

#define AF_ABS 0x01 /**< absolute value aggregation flag */

typedef struct s_aggregate {
	AGGREGATOR op; /**< the aggregation operator (min, max, etc.) */
	struct s_findpgm *group; /**< the find program used to build the aggregation */
	PROPERTY *pinfo; /**< the property over which the aggregation is done */
	AGGRPART part; /**< the property part (complex only) */
	unsigned char flags; /**< aggregation flags (e.g., AF_ABS) */
	struct s_findlist *last; /**< the result of the last run */
	struct s_aggregate *next; /**< the next aggregation in the core's list of aggregators */
} AGGREGATION; /**< the aggregation type */

#ifdef __cplusplus
extern "C" {
#endif

AGGREGATION *aggregate_mkgroup(char *aggregator, char *group_expression);
double aggregate_value(AGGREGATION *aggregate);

#ifdef __cplusplus
}
#endif

#endif

/**@}**/
