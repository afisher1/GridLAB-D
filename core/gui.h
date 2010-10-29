/*
 *  gui.h
 *  gridlabd
 *
 *  Created by d3g637-local on 10/26/10.
 *  Copyright 2010 Battelle Memorial Institute. All rights reserved.
 *
 */

#ifndef _GUI_H
#define _GUI_H

typedef enum {
	GUI_UNKNOWN=0,
	GUI_ROW, // a row group
	GUI_TAB, // a tab group (includes tabs at top)
	GUI_PAGE, // a page group (includes navigation |< < > >| buttons at top)
	GUI_GROUP, // a group of entities with a labeled border around it
	GUI_SPAN, // a group of entities that are not in columns
	_GUI_GROUPING_END, // end of grouping entities
	GUI_TITLE, // the title of the page, tab, or block
	GUI_STATUS, // the status message of the page
	GUI_TEXT, // a plain text entity 
	GUI_INPUT, // an input textbox
	GUI_CHECK, // a check box (set)
	GUI_RADIO, // a radio button (enumeration)
	GUI_SELECT, // a select drop down (enumeration)
	GUI_ACTION, // an action button
} GUIENTITYTYPE;

typedef struct s_guientity {
	GUIENTITYTYPE type;	// gui entity type (see GE_*)
	char srcref[1024]; // reference to source file location
	char value[1024]; // value (if provided)
	char globalname[64]; // ref to variable
	char objectname[64]; // ref object
	char propertyname[64]; // ref property
	char action[64]; // action value
	int span; // col span
	struct s_guientity *next;
	struct s_guientity *parent;
	/* internal variables */
	GLOBALVAR *var;
	void *data;
	UNIT *unit;
} GUIENTITY;

GUIENTITY *gui_create_entity();

void gui_set_type(GUIENTITY *entity, GUIENTITYTYPE type);
void gui_set_value(GUIENTITY *entity, char *value);
void gui_set_variablename(GUIENTITY *entity, char *globalname);
void gui_set_objectname(GUIENTITY *entity, char *objectname);
void gui_set_propertyname(GUIENTITY *entity, char *propertyname);
void gui_set_span(GUIENTITY *entity, int span);
void gui_set_unit(GUIENTITY *entity, char *unit);
void gui_set_next(GUIENTITY *entity, GUIENTITY *next);
void gui_set_parent(GUIENTITY *entity, GUIENTITY *parent);

GUIENTITY *gui_get_root(void);
GUIENTITY *gui_get_last(void);
GUIENTITYTYPE gui_get_type(GUIENTITY *entity);
GUIENTITY *gui_get_parent(GUIENTITY *entity);
GUIENTITY *gui_get_next(GUIENTITY *entity);
char *gui_get_name(GUIENTITY *entity);
char *gui_get_value(GUIENTITY *entity);
void *gui_get_data(GUIENTITY *entity);
GLOBALVAR *gui_get_variable(GUIENTITY *entity);
int gui_get_span(GUIENTITY *entity);
UNIT *gui_get_unit(GUIENTITY *entity);

int gui_is_grouping(GUIENTITY *entity);
int gui_is_header(GUIENTITY *entity);

void gui_html_start(void);
void gui_X11_start(void);

STATUS gui_html_output_all(void);

#endif

