/** $Id: auction.cpp 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file auction.cpp
	@defgroup auction Template for a new object class
	@ingroup market

	The auction object implements the basic auction. 

 **/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "gridlabd.h"
#include "auction.h"
#include "stubauction.h"

CLASS *auction::oclass = NULL;
auction *auction::defaults = NULL;
STATISTIC *auction::stats = NULL;
TIMESTAMP auction::longest_statistic = 0;
int auction::statistic_check = -1;
size_t auction::statistic_count = 0;

static PASSCONFIG passconfig = PC_PRETOPDOWN|PC_POSTTOPDOWN;
static PASSCONFIG clockpass = PC_POSTTOPDOWN;

EXPORT int64 get_market_for_time(OBJECT *obj, TIMESTAMP ts){
	auction *pAuc = 0;
	TIMESTAMP market_time = 0;
	int64 market_id;
	if(obj == 0){
		gl_error("get_market_for_time() was called with a null object pointer");
		/* TROUBLESHOOT
			The call was made without specifying the object that called it.  Double-check the code where this
			method was called and verify that a non-null object pointer is passed in.
			*/
		return -1;
	}
	pAuc = OBJECTDATA(obj, auction);
	// find when the current market started
	market_time = gl_globalclock + pAuc->period + pAuc->latency - ((gl_globalclock + pAuc->period) % pAuc->period);
	if(ts < market_time){
		KEY key;
		int64 remainder = ts - market_time;
		// the market ID wanted is N markets ahead of the current market...
		if(remainder < pAuc->period){
			market_id = pAuc->market_id;
		} else {
			market_id = pAuc->market_id + remainder/pAuc->period;
		}
		write_bid(key, market_id, -1, BID_UNKNOWN);
		return (int64)key;
	} else { // late, the market for that time has already closed
		return -1;
	}
	return -1;
}

/* Class registration is only called once to register the class with the core */
auction::auction(MODULE *module)
{
	if (oclass==NULL)
	{
		oclass = gl_register_class(module,"auction",sizeof(auction),passconfig);
		if (oclass==NULL){
			char msg[256];
			sprintf(msg, "unable to register object class implemented by %s", __FILE__);
			throw msg;
		}

		if (gl_publish_variable(oclass,
/**/		PT_enumeration, "type", PADDR(type), PT_DEPRECATED, PT_DESCRIPTION, "type of market",
				PT_KEYWORD, "NONE", AT_NONE,
				PT_KEYWORD, "SINGLE", AT_SINGLE,
				PT_KEYWORD, "DOUBLE", AT_DOUBLE,
/**/		PT_char32, "unit", PADDR(unit), PT_DESCRIPTION, "unit of quantity",
			PT_double, "period[s]", PADDR(dPeriod), PT_DESCRIPTION, "interval of time between market clearings",
			PT_double, "latency[s]", PADDR(dLatency), PT_DESCRIPTION, "latency between market clearing and delivery", 
			PT_int64, "market_id", PADDR(market_id), PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "unique identifier of market clearing",
			PT_double, "last.Q", PADDR(last.quantity), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "last cleared quantity", 
			PT_double, "last.P", PADDR(last.price), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "last cleared price", 
			PT_double, "next.Q", PADDR(next.quantity), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "next cleared quantity", 
			PT_double, "next.P", PADDR(next.price),  PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "next cleared price",
			PT_double, "avg24", PADDR(avg24), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "daily average of price",
			PT_double, "std24", PADDR(std24), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "daily stdev of price",
			PT_double, "avg72", PADDR(avg72), PT_DEPRECATED, PT_DESCRIPTION, "three day price average",
			PT_double, "std72", PADDR(std72), PT_DEPRECATED, PT_DESCRIPTION, "three day price stdev",
			PT_double, "avg168", PADDR(avg168), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "weekly average of price",
			PT_double, "std168", PADDR(std168), PT_DEPRECATED, PT_ACCESS, PA_REFERENCE, PT_DESCRIPTION, "weekly stdev of price",
/**/		PT_object, "network", PADDR(network), PT_DESCRIPTION, "the comm network used by object to talk to the market (if any)",
			PT_bool, "verbose", PADDR(verbose), PT_DESCRIPTION, "enable verbose auction operations",
			PT_object, "linkref", PADDR(linkref), PT_DEPRECATED, PT_DESCRIPTION, "reference to link object that has demand as power_out (only used when not all loads are bidding)",
			PT_double, "pricecap", PADDR(pricecap), PT_DEPRECATED, PT_DESCRIPTION, "the maximum price (magnitude) allowed",
			PT_double, "price_cap", PADDR(pricecap), PT_DESCRIPTION, "the maximum price (magnitude) allowed",

			PT_double, "demand.total", PADDR(asks.total),PT_DEPRECATED,
			PT_double, "demand.total_on", PADDR(asks.total_on),PT_DEPRECATED,
			PT_double, "demand.total_off", PADDR(asks.total_off),PT_DEPRECATED,
			PT_double, "supply.total", PADDR(offers.total),PT_DEPRECATED,
			PT_double, "supply.total_on", PADDR(offers.total_on),PT_DEPRECATED,-
			PT_double, "supply.total_off", PADDR(offers.total_off),PT_DEPRECATED,
			//PT_int32, "immediate", PADDR(immediate),
			PT_enumeration, "special_mode", PADDR(special_mode),
				PT_KEYWORD, "NONE", MD_NONE,
				PT_KEYWORD, "SELLERS_ONLY", MD_SELLERS,
				PT_KEYWORD, "BUYERS_ONLY", MD_BUYERS,
			PT_double, "fixed_price", PADDR(fixed_price),
			PT_double, "fixed_quantity", PADDR(fixed_quantity),
			PT_object, "capacity_reference_object", PADDR(capacity_reference_object),
			PT_char32, "capacity_reference_property", PADDR(capacity_reference_propname),
			PT_double, "capacity_reference_bid_price", PADDR(capacity_reference_bid_price),
			PT_double, "max_capacity_reference_bid_quantity", PADDR(max_capacity_reference_bid_quantity),
			PT_double, "capacity_reference_bid_quantity", PADDR(capacity_reference_bid_quantity),
			PT_double, "init_price", PADDR(init_price),
			PT_double, "init_stdev", PADDR(init_stdev),

			PT_timestamp, "current_market.start_time", PADDR(current_frame.start_time),
			PT_timestamp, "current_market.end_time", PADDR(current_frame.end_time),
			PT_double, "current_market.clearing_price[$]", PADDR(current_frame.clearing_price),
			PT_double, "current_market.clearing_quantity", PADDR(current_frame.clearing_quantity),
			PT_enumeration, "current_market.clearing_type", PADDR(current_frame.clearing_type),
				PT_KEYWORD, "MARGINAL_SELLER", CT_SELLER,
				PT_KEYWORD, "MARGINAL_BUYER", CT_BUYER,
				PT_KEYWORD, "MARGINAL_PRICE", CT_PRICE,
				PT_KEYWORD, "EXACT", CT_EXACT,
				PT_KEYWORD, "FAILURE", CT_FAILURE,
				PT_KEYWORD, "NULL", CT_NULL,
			PT_double, "current_market.marginal_quantity", PADDR(current_frame.marginal_quantity),
			PT_double, "current_market.total_marginal_quantity", PADDR(current_frame.total_marginal_quantity),
			PT_double, "current_market.seller_total_quantity", PADDR(current_frame.seller_total_quantity),
			PT_double, "current_market.buyer_total_quantity", PADDR(current_frame.buyer_total_quantity),
			PT_double, "current_market.seller_min_price", PADDR(current_frame.seller_min_price),

			PT_timestamp, "next_market.start_time", PADDR(next_frame.start_time),
			PT_timestamp, "next_market.end_time", PADDR(next_frame.end_time),
			PT_double, "next_market.clearing_price[$]", PADDR(next_frame.clearing_price),
			PT_double, "next_market.clearing_quantity", PADDR(next_frame.clearing_quantity),
			PT_enumeration, "next_market.clearing_type", PADDR(next_frame.clearing_type),
				PT_KEYWORD, "MARGINAL_SELLER", CT_SELLER,
				PT_KEYWORD, "MARGINAL_BUYER", CT_BUYER,
				PT_KEYWORD, "MARGINAL_PRICE", CT_PRICE,
				PT_KEYWORD, "EXACT", CT_EXACT,
				PT_KEYWORD, "FAILURE", CT_FAILURE,
				PT_KEYWORD, "NULL", CT_NULL,
			PT_double, "next_market.marginal_quantity", PADDR(next_frame.marginal_quantity),
			PT_double, "next_market.total_marginal_quantity", PADDR(next_frame.total_marginal_quantity),
			PT_double, "next_market.seller_total_quantity", PADDR(next_frame.seller_total_quantity),
			PT_double, "next_market.buyer_total_quantity", PADDR(next_frame.buyer_total_quantity),
			PT_double, "next_market.seller_min_price", PADDR(next_frame.seller_min_price),

			PT_timestamp, "past_market.start_time", PADDR(past_frame.start_time),
			PT_timestamp, "past_market.end_time", PADDR(past_frame.end_time),
			PT_double, "past_market.clearing_price[$]", PADDR(past_frame.clearing_price),
			PT_double, "past_market.clearing_quantity", PADDR(past_frame.clearing_quantity),
			PT_enumeration, "past_market.clearing_type", PADDR(past_frame.clearing_type),
				PT_KEYWORD, "MARGINAL_SELLER", CT_SELLER,
				PT_KEYWORD, "MARGINAL_BUYER", CT_BUYER,
				PT_KEYWORD, "MARGINAL_PRICE", CT_PRICE,
				PT_KEYWORD, "EXACT", CT_EXACT,
				PT_KEYWORD, "FAILURE", CT_FAILURE,
				PT_KEYWORD, "NULL", CT_NULL,
			PT_double, "past_market.marginal_quantity", PADDR(past_frame.marginal_quantity),
			PT_double, "past_market.total_marginal_quantity", PADDR(past_frame.total_marginal_quantity),
			PT_double, "past_market.seller_total_quantity", PADDR(past_frame.seller_total_quantity),
			PT_double, "past_market.buyer_total_quantity", PADDR(past_frame.buyer_total_quantity),
			PT_double, "past_market.seller_min_price", PADDR(past_frame.seller_min_price),
			//PT_double, "total_supply", PADDR(total_sell),
			//PT_double, "total_demand", PADDR(total_buy),
			//PT_double, "total_responsive_supply", PADDR(responsive_sell),
			//PT_double, "total_responsive_demand", PADDR(responsive_buy),
			//PT_double, "total_unresponsive_supply", PADDR(unresponsive_sell),
			//PT_double, "total_unresponsive_demand", PADDR(unresponsive_buy),

			PT_int32, "warmup", PADDR(warmup),

			NULL)<1){
				char msg[256];
				sprintf(msg, "unable to publish properties in %s",__FILE__);
				throw msg;
			}
		gl_publish_function(oclass,	"submit_bid", (FUNCTIONADDR)submit_bid);
		gl_publish_function(oclass,	"submit_bid_state", (FUNCTIONADDR)submit_bid_state);
		gl_publish_function(oclass, "get_market_for_time", (FUNCTIONADDR)get_market_for_time);
		defaults = this;
//		immediate = 1;
		memset(this,0,sizeof(auction));
	}
}


int auction::isa(char *classname)
{
	return strcmp(classname,"auction")==0;
}

/* Object creation is called once for each object that is created by the core */
int auction::create(void)
{
	STATISTIC *stat;
	double val = -1.0;
	memcpy(this,defaults,sizeof(auction));
	lasthr = thishr = -1;
	verbose = 0;
	pricecap = 0;
	warmup = 1;
	market_id = 1;
	clearing_scalar = 0.5;
	/* process dynamic statistics */
	if(statistic_check == -1){
		int rv;
		this->statistic_check = 0;
		rv = init_statistics();
		if(rv < 0){
			return 0;
		} else if(rv == 0){
			;
		} // else some number of statistics came back
	}
	for(stat = stats; stat != NULL; stat = stat->next){
		gl_set_value(OBJECTHDR(this), stat->prop, val);
	}
	return 1; /* return 1 on success, 0 on failure */
}

/* Object initialization is called once after all object have been created */
int auction::init(OBJECT *parent)
{
	OBJECT *obj=OBJECTHDR(this);
	unsigned int i = 0;

	if (linkref!=NULL)
	{
		if (!gl_object_isa(linkref,"link","powerflow"))
		{
			gl_error("%s (auction:%d) linkref '%s' does not reference a powerflow link object", obj->name?obj->name:"anonymous", obj->id, linkref->name);
			/* TROUBLESHOOT
				linkref must be an object reference to a powerflow::link object.  Verify that the property references that desired object in the model.
				*/
			return 0;
		}
		Qload = (double*)gl_get_addr(linkref,"power_out");
		if (Qload==NULL)
		{
			gl_error("%s (auction:%d) linkref '%s' does not publish power_out", obj->name?obj->name:"anonymous", obj->id, linkref->name);
			/* TROUBLESHOOT
				linkref must be an object reference to a powerflow::link object that publishes power_out.  Verify that the powerflow module is from
				version 2.0 or later.
				*/
			return 0;
		}
	}
	else
	{
		Qload = NULL;
	}
	
	if (pricecap==0){
		pricecap = 9999.0;
	}

	if(dPeriod == 0.0){
		dPeriod = 300.0;
		period = 300; // five minutes
	} else {
		period = (TIMESTAMP)floor(dPeriod + 0.5);
	}

	if(dLatency <= 0.0){
		dLatency = 0.0;
		latency = 0;
	} else {
		latency = (TIMESTAMP)floor(dLatency + 0.5);
	}
	// @new

	// init statistics, vs create statistics
	STATISTIC *statprop;
	for(statprop = stats; statprop != NULL; statprop = statprop->next){
		if(statprop->interval < this->period){
			static int was_warned = 0;
			if(was_warned == 0){
				gl_warning("market statistic '%s' samples faster than the market updates and will be filled with immediate data", statprop->prop->name);
				/* TROUBLESHOOT
					Market statistics are only calculated when the market clears.  Statistics shorter than the market clearing rate wind up being
					equal to either the immediate price or to zero, due to the constant price during the specified interval.
					*/
				was_warned = 0;
			}
			//statprop.interval = (TIMESTAMP)(this->period);
		} else if(statprop->interval % (int64)(this->period) != 0){
			static int was_also_warned = 0;
			gl_warning("market statistic '%s' interval not a multiple of market period, rounding towards one interval", statprop->prop->name);
			/* TROUBLESHOOT
				Market statistics are only calculated when the market clears, and will be calculated assuming intervals that are multiples of the market's period.
				*/
			statprop->interval -= statprop->interval % (int64)(period);
		}
	}
	/* reference object & property */	
	if(capacity_reference_object != NULL){
		if(capacity_reference_propname[0] != 0){
			capacity_reference_property = gl_get_property(capacity_reference_object, capacity_reference_propname);
			if(capacity_reference_property == NULL){
				gl_error("%s (auction:%d) capacity_reference_object of type '%s' does not contain specified reference property '%s'", obj->name?obj->name:"anonymous", obj->id, capacity_reference_object->oclass->name, capacity_reference_propname);
				/* TROUBLESHOOT
					capacity_reference_object must contain a property named by capacity_reference_property.  Review the published properties of the object specified
					in capacity_reference_object.
					*/
				return 0;
			}
			if(capacity_reference_property->ptype != PT_double){
				gl_warning("%s (auction:%d) capacity_reference_property '%s' is not a double type property", obj->name?obj->name:"anonymous", obj->id, capacity_reference_propname);
				/* TROUBLESHOOT
					capacity_reference_property must specify a double property to work properly, and may behave unpredictably should it be pointed at
					non-double properties.
					*/
			}
		} else {
			gl_error("%s (auction:%d) capacity_reference_object specified without a reference property", obj->name?obj->name:"anonymous", obj->id);
			/* TROUBLESHOOT
				capacity_reference_object can only be used when capacity_reference_property is specified.
				*/
			return 0;
		}
	}

	if(special_mode != MD_NONE){
		if(fixed_quantity < 0.0){
			gl_error("%s (auction:%d) is using a one-sided market with a negative fixed quantity", obj->name?obj->name:"anonymous", obj->id);
			/* TROUBLESHOOT
				capacity_reference_object can only be used when capacity_reference_property is specified.
				*/
			return 0;
		}
	}

	// initialize latency queue
	latency_count = (size_t)(latency / period + 2);
	latency_stride = sizeof(MARKETFRAME) + statistic_count * sizeof(double);
	framedata = (MARKETFRAME *)malloc(latency_stride * latency_count);
	memset(framedata, 0, latency_stride * latency_count);
	for(i = 0; i < latency_count; ++i){
		MARKETFRAME *frameptr;
		int64 addr = latency_stride * i + (int64)framedata;
		int64 stataddr = addr + sizeof(MARKETFRAME);
		int64 nextaddr = addr + latency_stride;
		frameptr = (MARKETFRAME *)(addr);
		if(statistic_count > 0){
			frameptr->statistics = (double *)(stataddr);
		} else {
			frameptr->statistics = 0;
		}
		if(i+1 < latency_count){
			frameptr->next = (MARKETFRAME *)(nextaddr);
		} else if(i+1 == latency_count){
			frameptr->next = framedata; // last->next = first
		}
	}
	latency_front = latency_back = 0;
	// initialize arrays
	if(statistic_count > 0){
		statdata = (double *)malloc(sizeof(double) * statistic_count);
	}
	if(longest_statistic > 0){
		history_count = (size_t)longest_statistic / (size_t)(this->period) + 2;
		new_prices = (double *)malloc(sizeof(double) * history_count);
	} else {
		history_count = 1;
		new_prices = (double *)malloc(sizeof(double));
	}
	price_index = 0;
	price_count = 0;
	for(i = 0; i < history_count; ++i){
		new_prices[i] = init_price;
	}

	if(init_stdev < 0.0){
		gl_error("auction init_stdev is negative!");
		/* TROUBLESHOOT
			By mathematical definition, real standard deviations cannot be negative.
			*/
		return 0;
	}
	STATISTIC *stat;
	for(stat = stats; stat != NULL; stat = stat->next){
		double check = 0.0;
		if(stat->stat_type == SY_STDEV){
			check = *gl_get_double(obj, stat->prop);
			if(check == -1.0){
				gl_set_value(obj, stat->prop, init_stdev);
			}
		} else if(stat->stat_type == SY_MEAN){
			check = *gl_get_double(obj, stat->prop);
			if(check == -1.0){
				gl_set_value(obj, stat->prop, init_price);
			}
		}
	}
	if(clearing_scalar <= 0.0){
		clearing_scalar = 0.5;
	}
	if(clearing_scalar >= 1.0){
		clearing_scalar = 0.5;
	}
	current_frame.clearing_price = init_price;
	past_frame.clearing_price = init_price;
	if(latency > 0)
		next_frame.clearing_price = init_price;
	return 1; /* return 1 on success, 0 on failure */
}

int auction::init_statistics(){
	STATISTIC *sp = 0;
	STATISTIC *tail = 0;
	STATISTIC statprop;
	PROPERTY *prop = oclass->pmap;
	OBJECT *obj = OBJECTHDR(this);
	for(prop = oclass->pmap; prop != NULL; prop = prop->next){
		char frame[32], price[32], stat[32], period[32], period_unit[32];
		memset(&statprop, 0, sizeof(STATISTIC));
		period_unit[0] = 0;
		if(sscanf(prop->name, "%[^\n_]_%[^\n_]_%[^\n_]_%[0-9]%[A-Za-z]", frame, price, stat, period, period_unit) >= 4){
			if(strcmp(price, "price") != 0){
				continue;
			}
			if(strcmp(stat, "mean") == 0){
				statprop.stat_type = SY_MEAN;
			} else if(strcmp(stat, "stdev") == 0){
				statprop.stat_type = SY_STDEV;
			} else {
				continue; 
			}
			if(strcmp(frame, "past") == 0){
				statprop.stat_mode = ST_PAST;
			} else if(strcmp(frame, "current") == 0){
				statprop.stat_mode = ST_CURR;
			} else {
				continue;
			}
			// parse period
			statprop.interval = strtol(period, 0, 10);
			if(statprop.interval <= 0){
				gl_warning("market statistic interval for '%s' is not positive, skipping", prop->name);
				/* TROUBLESHOOT
					Negative market statistic intervals are nonsensical.
					*/
				continue;
			}
			// scale by period_unit, if any
			if(period_unit[0] == 0){
				; // none? continue!
			} else if(period_unit[0] == 'm'){
				statprop.interval *= 60; // minutes
			} else if(period_unit[0] == 'h'){
				statprop.interval *= 3600;
			} else if(period_unit[0] == 'd'){
				statprop.interval *= 86400;
			} else if(period_unit[0] == 'w'){
				statprop.interval *= 604800;
			} else {
				gl_warning("market statistic period scalar '%c' not recognized, statistic ignored", period_unit[0]);
			} // months and years are of varying length
			// enqueue a new STATPROP instance
			sp = (STATISTIC *)malloc(sizeof(STATISTIC));
			memcpy(sp, &statprop, sizeof(STATISTIC));
			strcpy(sp->statname, prop->name);
			sp->prop = prop;
			sp->value = 0;
			if(stats == 0){
				stats = sp;
				tail = stats;
			} else {
				tail->next = sp;
				tail = sp;
			}
			// init statistic 
			// handled in create()
			/*
			if(statprop.stat_type == SY_MEAN){
				gl_set_value(obj, sp->prop, init_price);
			} else if(statprop.stat_type == SY_STDEV){
				gl_set_value(obj, sp->prop, init_stdev);
			}*/

			++statistic_count;
			if(statprop.interval > longest_statistic){
				longest_statistic = statprop.interval;
			}
		}
	}
	memset(&cleared_frame, 0, latency_stride);
	memset(&current_frame, 0, latency_stride);
	return 1;
}

int auction::update_statistics(){
	OBJECT *obj = OBJECTHDR(this);
	STATISTIC *current = 0;
	size_t sample_need = 0;
	unsigned int start = 0, stop = 0;
	unsigned int i = 0;
	unsigned int idx = 0;
	double mean = 0.0;
	double stdev = 0.0;
	if(statistic_count < 1){
		return 1; // no statistics
	}
	if(new_prices == 0){
		return 0;
	}
	if(statdata == 0){
		return 0;
	}
	if(stats == 0){
		return 1; // should've been caught with statistic_count < 1
	}
	for(current = stats; current != 0; current = current->next){
		mean = 0.0;
		sample_need = (size_t)(current->interval / this->period);
		if(current->stat_mode == ST_CURR){
			stop = price_index;
		} else if(current->stat_mode == ST_PAST){
			stop = price_index - 1;
		}
		//start = (unsigned int)((history_count + stop - sample_need + 1) % history_count);
		start = (unsigned int)((history_count + stop - sample_need) % history_count); // one off for initial period delay
		for(i = 0; i < sample_need; ++i){
			idx = (start + i + history_count) % history_count;
			mean += new_prices[idx];
		}
		mean /= sample_need;
		if(current->stat_type == SY_MEAN){
			current->value = mean;
		} else if(current->stat_type == SY_STDEV){
			double x = 0.0;
			if(sample_need + (current->stat_mode == ST_PAST ? 1 : 0) > total_samples){ // extra sample for 'past' values
				//	still in initial period, use init_stdev
				current->value = init_stdev;
			} else {
				stdev = 0.0;
				for(i = 0; i < sample_need; ++i){
					idx = (start + i + history_count) % history_count;
					x = new_prices[idx] - mean;
					stdev += x * x;
				}
				stdev /= sample_need;
				current->value = sqrt(stdev);
			}
		}
		if(latency == 0){
			gl_set_value(obj, current->prop, current->value);
		}
	}
	return 1;
}

/*	Take the current market values and enqueue them on the end of the latency frame queue. */
int auction::push_market_frame(TIMESTAMP t1){
	MARKETFRAME *frame = 0;
	OBJECT *obj = OBJECTHDR(this);
	STATISTIC *stat = stats;
	double *stats = 0;
	int64 frame_addr = latency_stride * latency_back + (int64)framedata;
	size_t i = 0;
	if((latency_back + 1) % latency_count == latency_front){
		gl_error("market latency queue is overwriting as-yet unused data, so is not long enough or is not consuming data");
		/* TROUBLESHOOT
			This is indicative of problems with how the internal circular queue is being handled.  Please report this error on the GridLAB-D
			TRAC site.
			*/
		return 0;
	}
	frame = (MARKETFRAME *)frame_addr;
	stats = frame->statistics;
	// set market details
	frame->market_id = cleared_frame.market_id;
	frame->start_time = cleared_frame.start_time;
	frame->end_time = cleared_frame.end_time;
	frame->clearing_price = cleared_frame.clearing_price;
	frame->clearing_quantity = cleared_frame.clearing_quantity;
	frame->clearing_type = cleared_frame.clearing_type;
	frame->marginal_quantity = cleared_frame.marginal_quantity;
	frame->total_marginal_quantity = cleared_frame.total_marginal_quantity;
	frame->seller_total_quantity = cleared_frame.seller_total_quantity;
	frame->buyer_total_quantity = cleared_frame.buyer_total_quantity;
	frame->seller_min_price = cleared_frame.seller_min_price;
	// set stats
	for(i = 0, stat = this->stats; i < statistic_count && stat != 0; ++i, stat = stat->next){
		stats[i] = stat->value;
	}
	if(back != 0){
		back->next = frame;
	}
	back = frame;

	latency_back = (latency_back + 1) % latency_count;
	if(latency > 0){
		++total_samples;
	}
	return 1;
}

int auction::check_next_market(TIMESTAMP t1){
	MARKETFRAME *frame = 0;
	frame = (MARKETFRAME *)(latency_front * this->latency_stride + (int64)framedata);
	if(frame->start_time > t1 && frame->start_time <= t1 + period){
		OBJECT *obj = OBJECTHDR(this);
		MARKETFRAME *nframe = frame;
		unsigned int i = 0;
		STATISTIC *stat;
		next_frame.market_id = nframe->market_id;
		next_frame.start_time = nframe->start_time;
		next_frame.end_time = nframe->end_time;
		next_frame.clearing_price = nframe->clearing_price;
		next_frame.clearing_quantity = nframe->clearing_quantity;
		next_frame.clearing_type = nframe->clearing_type;
		next_frame.marginal_quantity = nframe->marginal_quantity;
		next_frame.total_marginal_quantity = nframe->total_marginal_quantity;
		next_frame.seller_total_quantity = nframe->seller_total_quantity;
		next_frame.buyer_total_quantity = nframe->buyer_total_quantity;
		next_frame.seller_min_price = nframe->seller_min_price;
		for(i = 0, stat = this->stats; i < statistic_count, stat != 0; ++i, stat = stat->next){
			gl_set_value(obj, stat->prop, frame->statistics[i]);
		}
		return 1;
	}
	return 0;
}

/*	Fill in the exposed current market values with those within the */
TIMESTAMP auction::pop_market_frame(TIMESTAMP t1){
	MARKETFRAME *frame = 0;
	OBJECT *obj = OBJECTHDR(this);
	STATISTIC *stat = stats;
	double *stats = 0;
	size_t i = 0;
	if(latency_front == latency_back){
		gl_verbose("market latency queue has no data");
		return TS_NEVER;
	}
	//frame = &(framedata[latency_front]);

	frame = (MARKETFRAME *)(latency_front * this->latency_stride + (int64)framedata);

	// grab first sample as next but not current if it's *almost* time to push data
/*	if(latency > 0 && t1 >= (frame->start_time - period) && t1 < frame->start_time){
		MARKETFRAME *nframe = frame;
		next_frame.market_id = nframe->market_id;
		next_frame.start_time = nframe->start_time;
		next_frame.end_time = nframe->end_time;
		next_frame.clearing_price = nframe->clearing_price;
		next_frame.clearing_quantity = nframe->clearing_quantity;
		next_frame.clearing_type = nframe->clearing_type;
		next_frame.marginal_quantity = nframe->marginal_quantity;
		next_frame.seller_total_quantity = nframe->seller_total_quantity;
		next_frame.buyer_total_quantity = nframe->buyer_total_quantity;
		next_frame.seller_min_price = nframe->seller_min_price;
		return frame->start_time;
	}*/

	if(t1 < frame->start_time){
		gl_verbose("market latency queue data is not yet applicable");
		return frame->start_time - (latency > 0 ? period : 0);
	}
	// valid, time-applicable data
	// ~ copy current data to past_frame
	memcpy(&past_frame, &current_frame, sizeof(MARKETFRAME)); // not copying lagging stats
	// ~ copy new data in
	current_frame.market_id = frame->market_id;
	current_frame.start_time = frame->start_time;
	current_frame.end_time = frame->end_time;
	current_frame.clearing_price = frame->clearing_price;
	current_frame.clearing_quantity = frame->clearing_quantity;
	current_frame.clearing_type = frame->clearing_type;
	current_frame.marginal_quantity = frame->marginal_quantity;
	current_frame.total_marginal_quantity = frame->total_marginal_quantity;
	current_frame.seller_total_quantity = frame->seller_total_quantity;
	current_frame.buyer_total_quantity = frame->buyer_total_quantity;
	current_frame.seller_min_price = frame->seller_min_price;
	// copy statistics
	for(i = 0, stat = this->stats; i < statistic_count, stat != 0; ++i, stat = stat->next){
		gl_set_value(obj, stat->prop, frame->statistics[i]);
	}
	// ~ if latency > 0, cache next frame
	/*if(latency > 0){
		MARKETFRAME *nframe = frame->next;
		next_frame.market_id = nframe->market_id;
		next_frame.start_time = nframe->start_time;
		next_frame.end_time = nframe->end_time;
		next_frame.clearing_price = nframe->clearing_price;
		next_frame.clearing_quantity = nframe->clearing_quantity;
		next_frame.clearing_type = nframe->clearing_type;
		next_frame.marginal_quantity = nframe->marginal_quantity;
		next_frame.seller_total_quantity = nframe->seller_total_quantity;
		next_frame.buyer_total_quantity = nframe->buyer_total_quantity;
		next_frame.seller_min_price = nframe->seller_min_price;
		// copy statistics
		for(i = 0, stat = this->stats; i < statistic_count, stat != 0; ++i, stat = stat->next){
			gl_set_value(obj, stat->prop, frame->statistics[i]);
		}
	}*/
	// having used this index, push the index forward
	latency_front = (latency_front + 1) % latency_count;
	return TS_NEVER;
}

/* Presync is called when the clock needs to advance on the first top-down pass */
TIMESTAMP auction::presync(TIMESTAMP t0, TIMESTAMP t1)
{
	if (clearat==TS_ZERO)
	{
		clearat = nextclear();
		DATETIME dt;
		gl_localtime(clearat,&dt);
		update_statistics();
		char buffer[256];
		char myname[64];
		if (verbose) gl_output("   ...%s first clearing at %s", gl_name(OBJECTHDR(this),myname,sizeof(myname)), gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
	}
	else
	{
		/* if clock has advanced to a market clearing time */
//		if (t1>t0 && fmod((double)(t1/TS_SECOND),period)==0)
		if (t1>t0 && ((t1/TS_SECOND) % period)==0)
		{
			/* save the last clearing and reset the next clearing */
			last = next;
			next.from = NULL; /* in the context of a clearing, from is the marginal resource */
			next.quantity = next.price = 0;
		}
	}

	if (t1>=clearat)
	{
		DATETIME dt;
		gl_localtime(clearat,&dt);
		char buffer[256];
		char myname[64];
		if (verbose) gl_output("   ...%s clearing process started at %s", gl_name(OBJECTHDR(this),myname,sizeof(myname)), gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");

		/* clear market */
		thishr = dt.hour;
		clear_market();

		// advance market_id
		++market_id;

		char name[64];
		clearat = nextclear();
		// kick this over every hour to prevent odd behavior
		checkat = gl_globalclock + (TIMESTAMP)(3600.0 - fmod(gl_globalclock+3600.0,3600.0));
		gl_localtime(clearat,&dt);
		if (verbose) gl_output("   ...%s opens for clearing of market_id %d at %s", gl_name(OBJECTHDR(this),name,sizeof(name)), (int32)market_id, gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
	}

	return -clearat; /* return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

/* Postsync is called when the clock needs to advance on the second top-down pass */
TIMESTAMP auction::postsync(TIMESTAMP t0, TIMESTAMP t1)
{
	retry = 0;
	
	return (retry ? t1 : -clearat); /* soft return t2>t1 on success, t2=t1 for retry, t2<t1 on failure */
}

void auction::clear_market(void)
{
	unsigned int sph24 = (unsigned int)(3600/period*24);
	BID unresponsive;
	extern double bid_offset;

	memset(&unresponsive, 0, sizeof(unresponsive));

	/* handle linkref */
	if (Qload!=NULL && special_mode != MD_FIXED_BUYER) // buyers-only means no unresponsive bid
	{	
		char name[256];
		double total_unknown = asks.get_total() - asks.get_total_on() - asks.get_total_off();
		double refload = (*Qload);

		if (strcmp(unit,"")!=0) 
		{
			if (gl_convert("W",unit,&refload)==0){
				char msg[256];
				sprintf(msg, "linkref %s uses units of (W) and is incompatible with auction units (%s)", gl_name(linkref,name,sizeof(name)), unit);
				throw msg;
				/* TROUBLESHOOT
					Power markets must be trading in quantities measured with power units.  Change the market's unit property to some
					quantity of watts -- W, MW, kW, mW, dW, etc.
					*/
			}
			else if (verbose){
				gl_output("linkref converted %.3f W to %.3f %s", *Qload, refload, unit);
			}
		}
		if (total_unknown > 0.001) // greater than one mW ~ allows rounding errors
			gl_warning("total_unknown is %.0f -> some controllers are not providing their states with their bids", total_unknown);
		BID unresponsive;
		unresponsive.from = linkref;
		unresponsive.price = pricecap;
		unresponsive.state = BS_UNKNOWN;
		unresponsive.quantity = (refload - asks.get_total_on() - total_unknown/2); /* estimate load on as 1/2 unknown load */
		if (unresponsive.quantity < -0.001)
		{
			gl_warning("linkref %s has negative unresponsive load--this is probably due to improper bidding", gl_name(linkref,name,sizeof(name)), unresponsive.quantity);
		}
		else if (unresponsive.quantity > 0.001)
		{
			asks.submit(&unresponsive);
			gl_verbose("linkref %s has %.3f unresponsive load", gl_name(linkref,name,sizeof(name)), -unresponsive.quantity);
		}
	}

	/* handle unbidding capacity */
	if(capacity_reference_property != NULL && special_mode != MD_FIXED_BUYER){
		char name[256];
		double total_unknown = asks.get_total() - asks.get_total_on() - asks.get_total_off();
		double *pRefload = gl_get_double(capacity_reference_object, capacity_reference_property);
		double refload;
		
		if(pRefload == NULL){
			char msg[256];
			sprintf(msg, "unable to retreive property '%s' from capacity reference object '%s'", capacity_reference_property->name, capacity_reference_object->name);
			throw msg;
			/* TROUBLESHOOT
				The specified capacity reference property cannot be retrieved as a double.  Verify that it is a double type property.
				*/
		} else {
			refload = *pRefload;
		}

		if(strcmp(unit, "") != 0){
			if(capacity_reference_property->unit != 0){
				if(gl_convert(capacity_reference_property->unit->name,unit,&refload) == 0){
					char msg[256];
					sprintf(msg, "capacity_reference_property %s uses units of %s and is incompatible with auction units (%s)", gl_name(linkref,name,sizeof(name)), capacity_reference_property->unit->name, unit);
					throw msg;
					/* TROUBLESHOOT
						If capacity_reference_property has units specified, the units must be convertable to the units used by its auction object.
						*/
				} else if (verbose){
					gl_output("capacity_reference_property converted %.3f %s to %.3f %s", *pRefload, capacity_reference_property->unit->name, refload, unit);
				}
			} // else assume same units
		}
		if (total_unknown > 0.001){ // greater than one mW ~ allows rounding errors.  Threshold may be one kW given a MW-unit'ed market.
			gl_warning("total_unknown is %.0f -> some controllers are not providing their states with their bids", total_unknown);
		}
		unresponsive.from = linkref;
		unresponsive.price = pricecap;
		unresponsive.state = BS_UNKNOWN;
		unresponsive.quantity = (refload - asks.get_total_on() - total_unknown/2); /* estimate load on as 1/2 unknown load */
		if (unresponsive.quantity < -0.001)
		{
			gl_warning("capacity_reference_property %s has negative unresponsive load--this is probably due to improper bidding", gl_name(linkref,name,sizeof(name)), unresponsive.quantity);
		}
		else if (unresponsive.quantity > 0.001)
		{
			asks.submit(&unresponsive);
			gl_verbose("capacity_reference_property %s has %.3f unresponsive load", gl_name(linkref,name,sizeof(name)), -unresponsive.quantity);
		}
	}

	/* handle bidding capacity reference */
	if(capacity_reference_object != NULL && special_mode == MD_NONE) {
		double *pCaprefq = gl_get_double(capacity_reference_object, capacity_reference_property);
		double caprefq;
		if(pCaprefq == NULL) {
			char msg[256];
			sprintf(msg, "unable to retreive property '%s' from capacity reference object '%s'", capacity_reference_property->name, capacity_reference_object->name);
			throw msg;
		}
		caprefq = *pCaprefq;
		if(strcmp(unit, "") != 0) {
			if (capacity_reference_property->unit != 0) {
				if(gl_convert(capacity_reference_property->unit->name,unit,&caprefq) == 0) {
					char msg[256];
					sprintf(msg, "capacity_reference_property %s uses units of %s and is incompatible with auction units (%s)", capacity_reference_property->name, capacity_reference_property->unit->name, unit);
					throw msg;
				} else {
					capacity_reference_bid_quantity = caprefq;
					if(capacity_reference_bid_quantity < max_capacity_reference_bid_quantity) {
						submit(OBJECTHDR(this), capacity_reference_bid_quantity, capacity_reference_bid_price, -1, BS_ON);
					} else {
						submit(OBJECTHDR(this), max_capacity_reference_bid_quantity, pricecap, -1, BS_ON);
					}
				}
			}
		}
	}

	double single_quantity = 0.0;
	double single_price = 0.0;
	/* sort the bids */
	switch(special_mode){
		char name[64];
		case MD_SELLERS:
			offers.sort(false);
			if (verbose){
				gl_output("   ...  supply curve");
			}
			for (unsigned int i=0; i<offers.getcount(); i++){
				if (verbose){
					gl_output("   ...  %4d: %s offers %.3f %s at %.2f $/%s",i,gl_name(offers.getbid(i)->from,name,sizeof(name)), offers.getbid(i)->quantity,unit,offers.getbid(i)->price,unit);
				}
			}
			if(fixed_price * fixed_quantity != 0.0){
				gl_warning("fixed_price and fixed_quantity are set in the same single auction market ~ only fixed_price will be used");
			}
			if(fixed_quantity > 0.0){
				for(unsigned int i = 0; i < offers.getcount() && single_quantity < fixed_quantity; ++i){
					single_price = offers.getbid(i)->price;
					single_quantity += offers.getbid(i)->quantity;
				}
				if(single_quantity > fixed_quantity){
					single_quantity = fixed_quantity;
					clearing_type = CT_SELLER;
				} else if(single_quantity == fixed_quantity){
					clearing_type = CT_EXACT;
				} else {
					clearing_type = CT_FAILURE;
					single_quantity = 0.0;
					single_price = offers.getbid(0)->price - bid_offset;
				}
			} else if(fixed_quantity < 0.0){
				GL_THROW("fixed_quantity is negative");
			} else {
				single_price = fixed_price;
				for(unsigned int i = 0; i < offers.getcount(); ++i){
					if(offers.getbid(i)->price <= fixed_price){
						single_quantity += offers.getbid(i)->quantity;
					} else {
						break;
					}
				}
				if(single_quantity > 0.0){
					clearing_type = CT_EXACT;
				} else {
					clearing_type = CT_NULL;
				}
			} 
			next.quantity = single_quantity;
			next.price = single_price;
			break;
		case MD_BUYERS:
			asks.sort(true);
			if (verbose){
				gl_output("   ...  demand curve");
			}
			for (unsigned int i=0; i<asks.getcount(); i++){
				if (verbose){
					gl_output("   ...  %4d: %s asks %.3f %s at %.2f $/%s",i,gl_name(asks.getbid(i)->from,name,sizeof(name)), asks.getbid(i)->quantity,unit,asks.getbid(i)->price,unit);
				}
			}
			if(fixed_price * fixed_quantity != 0.0){
				gl_warning("fixed_price and fixed_quantity are set in the same single auction market ~ only fixed_price will be used");
			}
			if(fixed_quantity > 0.0){
				for(unsigned int i = 0; i < asks.getcount() && single_quantity < fixed_quantity; ++i){
					single_price = asks.getbid(i)->price;
					single_quantity += asks.getbid(i)->quantity;
				}
				if(single_quantity > fixed_quantity){
					single_quantity = fixed_quantity;
					clearing_type = CT_BUYER;
				} else if(single_quantity == fixed_quantity){
					clearing_type = CT_EXACT;
				} else {
					clearing_type = CT_FAILURE;
					single_quantity = 0.0;
					single_price = asks.getbid(0)->price + bid_offset;
				}
			} else if(fixed_quantity < 0.0){
				GL_THROW("fixed_quantity is negative");
			} else {
				single_price = fixed_price;
				for(unsigned int i = 0;  i < asks.getcount(); ++i){
					if(asks.getbid(i)->price >= fixed_price){
						single_quantity += asks.getbid(i)->quantity;
					} else {
						break;
					}
				}
				if(single_quantity > 0.0){
					clearing_type = CT_EXACT;
				} else {
					clearing_type = CT_NULL;
				}
			}
			next.quantity = single_quantity;
			next.price = single_price;
			break;
		case MD_FIXED_SELLER:
			offers.sort(false);
			if(asks.getcount() > 0){
				gl_warning("Seller-only auction was given purchasing bids");
			}
			asks.clear();
			submit(OBJECTHDR(this), -fixed_quantity, fixed_price, -1, BS_ON);
			break;
		case MD_FIXED_BUYER:
			asks.sort(true);
			if(offers.getcount() > 0){
				gl_warning("Buyer-only auction was given offering bids");
			}
			offers.clear();
			submit(OBJECTHDR(this), fixed_quantity, fixed_price, -1, BS_ON);
			break;
		case MD_NONE:
			offers.sort(false);
			asks.sort(true);
			break;
	}

	if(special_mode == MD_SELLERS || special_mode == MD_BUYERS){
		char name[64];
		char buffer[256];
		DATETIME dt;
		TIMESTAMP submit_time = gl_globalclock;
		gl_localtime(submit_time,&dt);
		if (verbose){
			gl_output("   ...  %s clears %.2f %s at $%.2f/%s at %s", gl_name(OBJECTHDR(this),name,sizeof(name)),
				next.quantity, unit, next.price, unit, gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
		}
	} else if ((asks.getcount()>0) && offers.getcount()>0)
	{
		TIMESTAMP submit_time = gl_globalclock;
		DATETIME dt;
		gl_localtime(submit_time,&dt);
		char buffer[256];
		/* clear market */
		unsigned int i=0, j=0;
		BID *buy = asks.getbid(i), *sell = offers.getbid(j);
		BID clear = {NULL,0,0};
		double demand_quantity = 0, supply_quantity = 0;
		double a=this->pricecap, b=-pricecap;
		bool check=false;
		
		// dump curves
		if (1)
		{
			char name[64];
			unresponsive_sell = 0.0;
			unresponsive_buy = 0.0;
			if (verbose){
				gl_output("   ...  supply curve");
			}
			for (i=0; i<offers.getcount(); i++){
				if (verbose){
					gl_output("   ...  %4d: %s offers %.3f %s at %.2f $/%s",i,gl_name(offers.getbid(i)->from,name,sizeof(name)), offers.getbid(i)->quantity,unit,offers.getbid(i)->price,unit);
				}
				if(offers.getbid(i)->price == -pricecap){
					unresponsive_sell += offers.getbid(i)->quantity;
				} else {
					responsive_sell += offers.getbid(i)->quantity;
				}
			}
			total_sell = responsive_sell + unresponsive_sell;
			if (verbose){
				gl_output("   ...  demand curve");
			}
			for (i=0; i<asks.getcount(); i++){
				if (verbose){
					gl_output("   ...  %4d: %s asks %.3f %s at %.2f $/%s",i,gl_name(asks.getbid(i)->from,name,sizeof(name)), asks.getbid(i)->quantity,unit,asks.getbid(i)->price,unit);
				}
				if(asks.getbid(i)->price == pricecap){
					unresponsive_buy += asks.getbid(i)->quantity;
				} else {
					responsive_buy += asks.getbid(i)->quantity;
				}
			}
			total_buy = responsive_buy + unresponsive_buy;
		}

		i = j = 0;
		this->clearing_type = CT_NULL;
		while (i<asks.getcount() && j<offers.getcount() && buy->price>=sell->price)
		{
			double buy_quantity = demand_quantity + buy->quantity;
			double sell_quantity = supply_quantity + sell->quantity;
			if (buy_quantity > sell_quantity)
			{
				clear.quantity = supply_quantity = sell_quantity;
				a = b = buy->price;
				sell = offers.getbid(++j);
				check = false;
				clearing_type = CT_BUYER;
			}
			else if (buy_quantity < sell_quantity)
			{
				clear.quantity = demand_quantity = buy_quantity;
				a = b = sell->price;
				buy = asks.getbid(++i);
				check = false;
				clearing_type = CT_SELLER;
			}
			else /* buy quantity equal sell quantity but price split */
			{
				clear.quantity = demand_quantity = supply_quantity = buy_quantity;
				a = buy->price;
				b = sell->price;
				buy = asks.getbid(++i);
				sell = offers.getbid(++j);
				check = true;
			}
		}
	
		if(a == b){
			clear.price = a;
		}
		if(check){ /* there was price agreement or quantity disagreement */
			clear.price = a;
			if(supply_quantity == demand_quantity){
				if(i == asks.getcount() || j == offers.getcount()){
					if(i == asks.getcount() && j == offers.getcount()){ // both sides exhausted at same quantity
						if(a == b){
							clearing_type = CT_EXACT;
						} else {
							clearing_type = CT_PRICE;
						}
					} else if (i == asks.getcount() && b == sell->price){ // exhausted buyers, sellers unsatisfied at same price
						clearing_type = CT_SELLER;
					} else if (j == offers.getcount() && a == buy->price){ // exhausted sellers, buyers unsatisfied at same price
						clearing_type = CT_BUYER;
					} else { // both sides satisfied at price, but one side exhausted
						if(a == b){
							clearing_type = CT_EXACT;
						} else {
							clearing_type = CT_PRICE;
						}
					}
				} else {
					if(a != buy->price && b != sell->price && a == b){
						clearing_type = CT_EXACT; // price changed in both directions
					} else if (a == buy->price && b != sell->price){
						// sell price increased ~ marginal buyer since all sellers satisfied
						clearing_type = CT_BUYER;
					} else if (a != buy->price && b == sell->price){
						// buy price increased ~ marginal seller since all buyers satisfied
						clearing_type = CT_SELLER;
						clear.price = b; // use seller's price, not buyer's price
					} else if(a == buy->price && b == sell->price){
						// possible when a == b, q_buy == q_sell, and either the buyers or sellers are exhausted
						if(i == asks.getcount() && j == offers.getcount()){
							clearing_type = CT_EXACT;
						} else if (i == asks.getcount()){ // exhausted buyers
							clearing_type = CT_SELLER;
						} else if (j == offers.getcount()){ // exhausted sellers
							clearing_type = CT_BUYER;
						}
					} else {
						clearing_type = CT_PRICE; // marginal price
					}
				}
			}
			if(clearing_type == CT_PRICE){
				double avg, dHigh, dLow;
				avg = (a+b) / 2;
				dHigh = (i == asks.getcount() ? a : buy->price);
				dLow = (j == offers.getcount() ? b : sell->price);
				// needs to be just off such that it does not trigger any other bids
				if(a == pricecap && b != -pricecap){
					clear.price = (buy->price > b ? buy->price + bid_offset : b);
				} else if(a != pricecap && b == -pricecap){
					clear.price = (sell->price < a ? sell->price - bid_offset : a);
				} else if(a == pricecap && b == -pricecap){
					if(i == asks.getcount() && j == offers.getcount()){
						clear.price = 0; // no additional bids on either side
					} else if(i == asks.getcount()){ // buyers left
						clear.price = buy->price + bid_offset;
					} else if(j == asks.getcount()){ // sellers left
						clear.price = sell->price - bid_offset;
					} else { // additional bids on both sides, just no clearing
						clear.price = (dHigh + dLow) / 2;
					}
				} else {
					if(i != asks.getcount() && buy->price == a){
						clear.price = a;
					} else if (j != offers.getcount() && sell->price == b){
						clear.price = b;
					} else if(i != asks.getcount() && avg < buy->price){
						clear.price = dHigh + bid_offset;
					} else if(j != offers.getcount() && avg > sell->price){
						clear.price = dLow - bid_offset;
					} else {
						clear.price = avg;
					}
				}
			}
		}
	
		/* check for zero demand but non-zero first unit sell price */
		if (clear.quantity==0)// && offers.getcount()>0)
		{
			clearing_type = CT_NULL;
			if(offers.getcount() > 0 && asks.getcount() == 0){
				clear.price = offers.getbid(0)->price - bid_offset;
			} else if(offers.getcount() == 0 && asks.getcount() > 0){
				clear.price = asks.getbid(0)->price + bid_offset;
			} else {
				clear.price = offers.getbid(0)->price + (asks.getbid(0)->price - offers.getbid(0)->price) * clearing_scalar;;
			}
			
		} else if (clear.quantity < unresponsive_buy){
			clearing_type = CT_FAILURE;
			clear.price = pricecap;
		} else if (clear.quantity < unresponsive_sell){
			clearing_type = CT_FAILURE;
			clear.price = -pricecap;
		} else if (clear.quantity == unresponsive_buy && clear.quantity == unresponsive_sell){
			// only cleared unresponsive loads
			clearing_type = CT_PRICE;
			clear.price = 0.0;
		}
	
		/* post the price */
		char name[64];
		if (verbose) gl_output("   ...  %s clears %.2f %s at $%.2f/%s at %s", gl_name(OBJECTHDR(this),name,sizeof(name)), clear.quantity, unit, clear.price, unit, gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
		next.price = clear.price;
		next.quantity = clear.quantity;
	}
	else
	{
		char name[64];
		if(offers.getcount() > 0 && asks.getcount() == 0){
			next.price = offers.getbid(0)->price - bid_offset;
		} else if(offers.getcount() == 0 && asks.getcount() > 0){
			next.price = asks.getbid(0)->price + bid_offset;
		} else if(asks.getcount() > 0 && offers.getcount() > 0){
			next.price = offers.getbid(0)->price + (asks.getbid(0)->price - offers.getbid(0)->price) * clearing_scalar;
		} else if(asks.getcount() == 0 && offers.getcount() == 0){
			next.price = 0.0;
		}
		next.quantity = 0;
		clearing_type = CT_NULL;
		gl_warning("market '%s' fails to clear due to missing %s", gl_name(OBJECTHDR(this),name,sizeof(name)), asks.getcount()==0?(offers.getcount()==0?"buyers and sellers":"buyers"):"sellers");
	}
	
	double marginal_total = 0.0;
	double marginal_quantity = 0.0;
	if(clearing_type == CT_BUYER){
		unsigned int i = 0;
		double marginal_subtotal = 0.0;
		for(i = 0; i < asks.getcount(); ++i){
			if(asks.getbid(i)->price > next.price){
				marginal_subtotal += asks.getbid(i)->quantity;
			} else {
				break;
			}
		}
		marginal_quantity = next.quantity - marginal_subtotal;
		for(; i < asks.getcount(); ++i){
			if(asks.getbid(i)->price == next.price)
				marginal_total += asks.getbid(i)->quantity;
			else
				break;
		}
	} else if (clearing_type == CT_SELLER){
		unsigned int i = 0;
		double marginal_subtotal = 0.0;
		for(i = 0; i < offers.getcount(); ++i){
			if(offers.getbid(i)->price < next.price){
				marginal_subtotal += offers.getbid(i)->quantity;
			} else {
				break;
			}
		}
		marginal_quantity = next.quantity - marginal_subtotal;
		for(; i < offers.getcount(); ++i){
			if(offers.getbid(i)->price == next.price)
				marginal_total += offers.getbid(i)->quantity;
			else
				break;
		}	
	} else {
		marginal_quantity = 0.0;
	}

	if(history_count > 0){
		if(price_index == history_count){
			price_index = 0;
		}
		new_prices[price_index] = next.price;
		//int update_rv = update_statistics();
		++price_index;
	}
	if(period <= 3600){
		if(lasthr != thishr){
			unsigned int i = 0;
			unsigned int sph = (unsigned int)(3600/period);
			unsigned int sph24 = 24*sph;
			unsigned int sph72 = 72*sph;
			unsigned int sph168 = 168*sph;

			/* add price/quantity to the history */
			prices[count%sph168] = next.price;
			++count;
			
			/* update the daily and weekly averages */
			avg168 = 0.0;
			for(i = 0; i < count && i < sph168; ++i){
				avg168 += prices[i];
			}
			avg168 /= (count > sph168 ? sph168 : count);

			avg72 = 0.0;
			for(i = 1; i <= sph72 && i <= count; ++i){
				unsigned int j = (sph168 - i + (unsigned int)count) % sph168;
				avg72 += prices[j];
			}
			avg72 /= (count > sph72 ? sph72 : count);

			avg24 = 0.0;
			for(i = 1; i <= sph24 && i <= count; ++i){
				unsigned int j = (sph168 - i + (unsigned int)count) % sph168;
				avg24 += prices[j];
			}
			avg24 /= (count > sph24 ? sph24 : (unsigned int)count);

			/* update the daily & weekly standard deviations */
			std168 = 0.0;
			for(i = 0; i < count && i < sph168; ++i){
				std168 += prices[i] * prices[i];
			}
			std168 /= (count > sph168 ? sph168 : (unsigned int)count);
			std168 -= avg168*avg168;
			std168 = sqrt(fabs(std168));
			if (std168<0.01) std168=0.01;

			std72 = 0.0;
			for(i = 1; i <= sph72 && i <= (unsigned int)count; ++i){
				unsigned int j = (sph168 - i + (unsigned int)count) % sph168;
				std72 += prices[j] * prices[j];
			}
			std72 /= (count > sph72 ? sph72 : (unsigned int)count);
			std72 -= avg72*avg72;
			std72 = sqrt(fabs(std72));
			if (std72 < 0.01){
				std72 = 0.01;
			}

			std24 = 0.0;
			for(i = 1; i <= sph24 && i <= (unsigned int)count; ++i){
				unsigned int j = (sph168 - i + (unsigned int)count) % sph168;
				std24 += prices[j] * prices[j];
			}
			std24 /= (count > sph24 ? sph24 : (unsigned int)count);
			std24 -= avg24*avg24;
			std24 = sqrt(fabs(std24));
			if (std24<0.01){
				std24=0.01;
			}

			retry = 1; // reiterate to pass the updated values to the controllers

			/* update reference hour */
			lasthr = thishr;
		}
	}

	// update cleared_frame data
	cleared_frame.market_id = this->market_id;
	cleared_frame.start_time = gl_globalclock + latency;
	cleared_frame.end_time = gl_globalclock + latency + period;
	cleared_frame.clearing_price = next.price;
	cleared_frame.clearing_quantity = next.quantity;
	cleared_frame.clearing_type = clearing_type;
	cleared_frame.marginal_quantity = marginal_quantity;
	cleared_frame.total_marginal_quantity = marginal_total;
	cleared_frame.buyer_total_quantity = asks.get_total();
	cleared_frame.seller_total_quantity = offers.get_total();
	cleared_frame.seller_min_price = offers.get_min();

	if(latency > 0){
		TIMESTAMP rt;
		rt = pop_market_frame(gl_globalclock);
		update_statistics();
		push_market_frame(gl_globalclock);
		check_next_market(gl_globalclock);
	} else {
		STATISTIC *stat = 0;
		OBJECT *obj = OBJECTHDR(this);
		memcpy(&past_frame, &current_frame, sizeof(MARKETFRAME)); // just the frame
		// ~ copy new data in
		current_frame.market_id = cleared_frame.market_id;
		current_frame.start_time = cleared_frame.start_time;
		current_frame.end_time = cleared_frame.end_time;
		current_frame.clearing_price = cleared_frame.clearing_price;
		current_frame.clearing_quantity = cleared_frame.clearing_quantity;
		current_frame.clearing_type = cleared_frame.clearing_type;
		current_frame.marginal_quantity = cleared_frame.marginal_quantity;
		current_frame.total_marginal_quantity = cleared_frame.total_marginal_quantity;
		current_frame.seller_total_quantity = cleared_frame.seller_total_quantity;
		current_frame.buyer_total_quantity = cleared_frame.buyer_total_quantity;
		current_frame.seller_min_price = cleared_frame.seller_min_price;
		++total_samples;
		update_statistics();
		
	}

	/* clear the bid lists */
	asks.clear();
	offers.clear();

	/* limit price */
	if (next.price<-pricecap) next.price = -pricecap;
	else if (next.price>pricecap) next.price = pricecap;
}

KEY auction::submit(OBJECT *from, double quantity, double real_price, KEY key, BIDDERSTATE state)
{
	char myname[64];
	TIMESTAMP submit_time = gl_globalclock;
	DATETIME dt;
	double price;
	gl_localtime(submit_time,&dt);
	char buffer[256];
	BIDDEF biddef;
	/* suppress demand bidding until market stabilizes */
	unsigned int sph24 = (unsigned int)(3600/period*24);
	if(real_price > pricecap){
		gl_warning("%s received a bid above the price cap, truncating");
		price = pricecap;
	} else {
		price = real_price;
	}
	if (total_samples<sph24 && quantity<0 && warmup)
	{
		if (verbose) gl_output("   ...  %s ignoring demand bid during first 24 hours", gl_name(OBJECTHDR(this),myname,sizeof(myname)));
		return -1;
	}

	/* translate key */
	if(key == -1 || key == 0xccccccccffffffffULL){ // new bid ~ rebuild key
		//write_bid(key, market_id, -1, BID_UNKNOWN);
		biddef.bid = -1;
		biddef.bid_type = BID_UNKNOWN;
		biddef.market = -1;
		biddef.raw = -1;
	} else {
		if((key & 0xFFFFFFFF00000000ULL) == 0xCCCCCCCC00000000ULL){
			key &= 0x00000000FFFFFFFFULL;
		}
		translate_bid(biddef, key);
	}

	if (biddef.market > market_id)
	{	// future market
		gl_error("bidding into future markets is not yet supported");
		/* TROUBLESHOOT
			Tracking bids input markets other than the immediately open one will be supported in the future.
			*/
	}
	else if (biddef.market == market_id) // resubmit
	{
		char biddername[64];
		KEY out;
		if (verbose){
			gl_output("   ...  %s resubmits %s from object %s for %.2f %s at $%.2f/%s at %s", 
				gl_name(OBJECTHDR(this),myname,sizeof(myname)), quantity<0?"ask":"offer", gl_name(from,biddername,sizeof(biddername)), 
				fabs(quantity), unit, price, unit, gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
		}
		BID bid = {from,fabs(quantity),price,state};
		if(quantity == 0.0){
			return 0;
		}
		if (biddef.bid_type == BID_BUY){
			out = asks.resubmit(&bid,biddef.bid);
		} else if (biddef.bid_type == BID_SELL){
			out = offers.resubmit(&bid,biddef.bid);
		} else {
			;
		}
		return biddef.raw;
	}
	else if (biddef.market < 0 || biddef.bid_type == BID_UNKNOWN){
		char myname[64];
		char biddername[64];
		KEY out;
		if (verbose){
			gl_output("   ...  %s receives %s from object %s for %.2f %s at $%.2f/%s at %s", 
				gl_name(OBJECTHDR(this),myname,sizeof(myname)), quantity<0?"ask":"offer", gl_name(from,biddername,sizeof(biddername)), 
				fabs(quantity), unit, price, unit, gl_strtime(&dt,buffer,sizeof(buffer))?buffer:"unknown time");
		}
		BID bid = {from,fabs(quantity),price,state};
		if (quantity<0){
			out = asks.submit(&bid);
		} else if (quantity>0){
			out = offers.submit(&bid);
		} else {
			char name[64];
			gl_debug("zero quantity bid from %s is ignored", gl_name(from,name,sizeof(name)));
			return -1;
		}
		biddef.bid = (int16)out;
		biddef.market = market_id;
		biddef.bid_type = (quantity > 0 ? BID_SELL : BID_BUY);
		write_bid(out, biddef.market, biddef.bid, biddef.bid_type);
		biddef.raw = out;
		return biddef.raw;
	} else { // key between cleared market and 'market_id' ~ points to an old market
		if(verbose){
			char myname[64];
			char biddername[64];
			gl_output(" ... %s receives %s from object %s for a previously cleared market",
				gl_name(OBJECTHDR(this),myname,sizeof(myname)),quantity<0?"ask":"offer",
				gl_name(from,biddername,sizeof(biddername)));
		}
		return 0;
	}
	return 0;
}

TIMESTAMP auction::nextclear(void) const
{
	return gl_globalclock + (TIMESTAMP)(period - (gl_globalclock+period) % period);
}

//////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF CORE LINKAGE
//////////////////////////////////////////////////////////////////////////

EXPORT int create_auction(OBJECT **obj, OBJECT *parent)
{
	try
	{
		*obj = gl_create_object(auction::oclass);
		if (*obj!=NULL)
		{
			auction *my = OBJECTDATA(*obj,auction);
			gl_set_parent(*obj,parent);
			return my->create();
		}
	}
	catch (char *msg)
	{
		gl_error("create_auction: %s", msg);
	}
	return 1;
}

EXPORT int init_auction(OBJECT *obj, OBJECT *parent)
{
	try
	{
		if (obj!=NULL)
			return OBJECTDATA(obj,auction)->init(parent);
	}
	catch (char *msg)
	{
		char name[64];
		gl_error("init_auction(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
	}
	catch (const char *msg)
	{
		char name[64];
		gl_error("init_auction(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
	}
	return 1;
}

EXPORT int isa_auction(OBJECT *obj, char *classname)
{
	if(obj != 0 && classname != 0){
		return OBJECTDATA(obj,auction)->isa(classname);
	} else {
		return 0;
	}
}

EXPORT TIMESTAMP sync_auction(OBJECT *obj, TIMESTAMP t1, PASSCONFIG pass)
{
	TIMESTAMP t2 = TS_NEVER;
	auction *my = OBJECTDATA(obj,auction);
	try
	{
		switch (pass) {
		case PC_PRETOPDOWN:
			t2 = my->presync(obj->clock,t1);
			break;
		case PC_POSTTOPDOWN:
			t2 = my->postsync(obj->clock,t1);
			break;
		default:
			GL_THROW("invalid pass request (%d)", pass);
			break;
		}
		if (pass==clockpass)
			obj->clock = t1;
	}
	catch (char *msg)
	{
		char name[64];
		gl_error("sync_auction(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
	}
	catch (const char *msg)
	{
		char name[64];
		gl_error("sync_auction(obj=%s): %s", gl_name(obj,name,sizeof(name)), msg);
	}
	return t2;
}

