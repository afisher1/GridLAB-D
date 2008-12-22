/** $Id: convert.c 1182 2008-12-22 22:08:36Z dchassin $
	Copyright (C) 2008 Battelle Memorial Institute
	@file convert.c
	@author David P. Chassin
	@date 2007
	@addtogroup convert Conversion of properties
	@ingroup core
	
	The convert module handles conversion object properties and strings
	
@{
 **/

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "globals.h"
#include "convert.h"
#include "object.h"

/** Convert from a \e void
	This conversion does not change the data
	@return 6, the number of characters written to the buffer, 0 if not enough space
 **/
int convert_from_void(char *buffer, /**< a pointer to the string buffer */
					  int size, /**< the size of the string buffer */
					  void *data, /**< a pointer to the data that is not changed */
					  PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	if(size < 7)
		return 0;
	return sprintf(buffer,"%s","(void)");
}

/** Convert to a \e void
	This conversion ignores the data
	@return always 1, indicated data was successfully ignored
 **/
int convert_to_void(char *buffer, /**< a pointer to the string buffer that is ignored */
					  void *data, /**< a pointer to the data that is not changed */
					  PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	return 1;
}

/** Convert from a \e double
	Converts from a \e double property to the string.  This function uses
	the global variable \p global_double_format to perform the conversion.
	@return the number of characters written to the string
 **/
int convert_from_double(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	int count = sprintf(temp, global_double_format, *(double *)data);
	if(count < size+1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to a \e double
	Converts a string to a \e double property.  This function uses the global
	variable \p global_double_format to perform the conversion.
	@return 1 on success, 0 on failure, -1 is conversion was incomplete
 **/
int convert_to_double(char *buffer, /**< a pointer to the string buffer */
					  void *data, /**< a pointer to the data */
					  PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	return sscanf(buffer,"%lg",data);
}

/** Convert from a complex
	Converts a complex property to a string.  This function uses
	the global variable \p global_complex_format to perform the conversion.
	@return the number of character written to the string
 **/
int convert_from_complex(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	int count = 0;
	char temp[1025];
	complex *v = data;
	if (v->f==A)
	{
		double m = sqrt(v->r*v->r+v->i*v->i);
		double a = (v->r==0) ? (v->i>0 ? PI/2 : (v->i==0 ? 0 : -PI/2)) : (v->r>0 ? atan(v->i/v->r) : PI+atan(v->i/v->r));
		if (a>PI) a-=(2*PI);
		count = sprintf(temp,global_complex_format,m,a*180/PI,A);
	} else {
		count = sprintf(temp,global_complex_format,v->r,v->i,v->f?v->f:'i');
	}
	if(count < size - 1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to a complex
	Converts a string to a complex property.  This function uses the global
	variable \p global_complex_format to perform the conversion.
	@return 1 when only real is read, 2 imaginary part is also read, 3 when notation is also read, 0 on failure, -1 is conversion was incomplete
 **/
int convert_to_complex(char *buffer, /**< a pointer to the string buffer */
					   void *data, /**< a pointer to the data */
					   PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	complex *v = (complex*)data;
	char notation[2]={CNOTATION_DEFAULT,'\0'};
	int n;
	double a=0, b=0; 
	n = sscanf(buffer,"%lg%lg%1[ijd]",&a,&b,notation);
	if (n < 2)
	{
		char signage[2] = {0, 0};
		/* printf("alt complex form?"); */
		n = sscanf(buffer, "%lg %1[+-] %lg%1[ijd]", &a, signage, &b, notation);
		if (n > 0 && signage[0] == '-')
			b *= -1.0;
	}
	if (n>0)
	{
		if (n>1 && notation[0]==A)
		{
			v->r = a*cos(b*PI/180);
			v->i = a*sin(b*PI/180);
		}
		else
		{
			v->r = a;
			v->i = (n>1?b:0);
		}
		v->f = notation[0];
		return 1;
	}
	else
		return 0;
}

/** Convert from an \e enumeration
	Converts an \e enumeration property to a string.  
	@return the number of character written to the string
 **/
int convert_from_enumeration(char *buffer, /**< pointer to the string buffer */
						     int size, /**< size of the string buffer */
					         void *data, /**< a pointer to the data */
					         PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	KEYWORD *keys=prop->keywords;
	int count = 0;
	char temp[1025];
	/* get the true value */
	int value = *(unsigned long*)data;

	/* process the keyword list, if any */
	for ( ; keys!=NULL ; keys=keys->next)
	{
		/* if the key value matched */
		if (keys->value==value){
			/* use the keyword */
			count = strncpy(temp,keys->name,1024)?(int)strlen(temp):0;
			break;
		}
	}

	/* no keyword found, return the numeric value instead */
	if (count == 0){
		 count = sprintf(temp,"%d",value);
	}
	if(count < size - 1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to an \e enumeration
	Converts a string to an \e enumeration property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_enumeration(char *buffer, /**< a pointer to the string buffer */
					       void *data, /**< a pointer to the data */
					       PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	KEYWORD *keys=prop->keywords;

	/* process the keyword list */
	for ( ; keys!=NULL ; keys=keys->next)
	{
		if (strcmp(keys->name,buffer)==0)
		{
			*(unsigned long*)data=keys->value;
			return 1;
		}
	}
	return sscanf(buffer,"%d",(unsigned long*)data);
}

/** Convert from an \e set
	Converts a \e set property to a string.  
	@return the number of character written to the string
 **/
#define SETDELIM "|"
int convert_from_set(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	KEYWORD *keys=prop->keywords;

	/* get the actual value */
	unsigned long value = *(unsigned long*)data;

	/* keep track of how characters written */
	int count=0;

	int NONZERO = (value > 0);
	/* clear the buffer */
	buffer[0] = '\0';

	/* process each keyword */
	for ( ; keys!=NULL ; keys=keys->next)
	{
		/* if the keyword matches */
		if ((keys->value&value)==keys->value || (keys->value==0 && value==0 && !NONZERO))
		{
			/* get the length of the keyword */
			int len = (int)strlen(keys->name);

			/* remove the key from the copied values */
			value -= keys->value;

			/* if there's room for it in the buffer */
			if (size>count+len+1)
			{
				/* if the buffer already has keywords in it */
				if (buffer[0]!='\0')
				{
					/* add a separator to the buffer */
					if (!(prop->flags&PF_CHARSET))
					{
						count++;
						strcat(buffer,SETDELIM);
					}
				}

				/* add the keyword to the buffer */
				count += len;
				strcat(buffer,keys->name);
			}

			/* no room in the buffer */
			else

				/* fail */
				return 0;
		}
	}

	/* succeed */
	return count;
}

/** Convert to a \e set
	Converts a string to a \e set property.  
	@return number of values read on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_set(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	KEYWORD *keys=prop->keywords;
	char temp[4096], *ptr;
	unsigned long value=0;
	int count=0;

	/* directly convert numeric strings */
	if (strnicmp(buffer,"0x",2)==0)
		return sscanf(buffer,"0x%x",(unsigned long *)data);
	else if (isdigit(buffer[0]))
		return sscanf(buffer,"%d",(unsigned long *)data);

	/* prevent long buffer from being scanned */
	if (strlen(buffer)>sizeof(temp)-1)
		return 0;

	/* make a temporary copy of the buffer */
	strcpy(temp,buffer);

	/* check for CHARSET keys (single character keys) and usage without | */
	if ((prop->flags&PF_CHARSET) && strchr(buffer,'|')==NULL)
	{
		for (ptr=buffer; *ptr!='\0'; ptr++)
		{
			KEYWORD *key;
			for (key=keys; key!=NULL; key=key->next)
			{
				if (*ptr==key->name[0])
				{
					value |= key->value;
					count ++;
					break; /* we found our key */
				}
			}
		}
	}
	else
	{
		/* process each keyword in the temporary buffer*/
		for (ptr=strtok(temp,SETDELIM); ptr!=NULL; ptr=strtok(NULL,SETDELIM))
		{
			KEYWORD *key;

			/* scan each of the keywords in the set */
			for (key=keys; key!=NULL; key=key->next)
			{
				if (strcmp(ptr,key->name)==0)
				{
					value |= key->value;
					count ++;
					break; /* we found our key */
				}
			}
		}
	}
	*(unsigned long *)data = value;
	return count;
}

/** Convert from an \e int16
	Converts an \e int16 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_int16(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	int count = sprintf(temp,"%hd",*(short*)data);
	if(count < size - 1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to an \e int16
	Converts a string to an \e int16 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_int16(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	return sscanf(buffer,"%hd",data);
}

/** Convert from an \e int32
	Converts an \e int32 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_int32(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	int count = sprintf(temp,"%ld",*(int*)data);
	if(count < size - 1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to an \e int32
	Converts a string to an \e int32 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_int32(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	return sscanf(buffer,"%ld",data);
}

/** Convert from an \e int64
	Converts an \e int64 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_int64(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	int count = sprintf(temp,"%" FMT_INT64 "d",*(int64*)data);
	if(count < size - 1){
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	} else {
		return 0;
	}
}

/** Convert to an \e int64
	Converts a string to an \e int64 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_int64(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	return sscanf(buffer,"%" FMT_INT64 "d",data);
}

/** Convert from a \e char8
	Converts a \e char8 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_char8(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	char *format = "%s";
	int count = 0;
	if (strchr((char*)data,' ')!=NULL || strchr((char*)data,';')!=NULL || ((char*)data)[0]=='\0')
		format = "\"%s\"";
	count = sprintf(temp,format,(char*)data);
	if(count > size - 1){
		return 0;
	} else {
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	}
}

/** Convert to a \e char8
	Converts a string to a \e char8 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_char8(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char c=((char*)data)[0];
	switch (c) {
	case '"':
		return sscanf(buffer+1,"%8[^\"]",data);
	default:
		return sscanf(buffer,"%8s",data);
	}
}

/** Convert from a \e char32
	Converts a \e char32 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_char32(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	char *format = "%s";
	int count = 0;
	if (strchr((char*)data,' ')!=NULL || strchr((char*)data,';')!=NULL || ((char*)data)[0]=='\0')
		format = "\"%s\"";
	count = sprintf(temp,format,(char*)data);
	if(count > size - 1){
		return 0;
	} else {
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	}
}

/** Convert to a \e char32
	Converts a string to a \e char32 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_char32(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char c=((char*)data)[0];
	switch (c) {
	case '"':
		return sscanf(buffer+1,"%32[^\"]",data);
	default:
		return sscanf(buffer,"%32s",data);
	}
}

/** Convert from a \e char256
	Converts a \e char256 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_char256(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[1025];
	char *format = "%s";
	int count = 0;
	if (strchr((char*)data,' ')!=NULL || strchr((char*)data,';')!=NULL || ((char*)data)[0]=='\0')
		format = "\"%s\"";
	count = sprintf(temp,format,(char*)data);
	if(count > size - 1){
		return 0;
	} else {
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	}
}

/** Convert to a \e char256
	Converts a string to a \e char256 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_char256(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char c=((char*)data)[0];
	switch (c) {
	case '"':
		return sscanf(buffer+1,"%256[^\"]",data);
	default:
		return sscanf(buffer,"%256s",data);
	}
}

/** Convert from a \e char1024
	Converts a \e char1024 property to a string.  
	@return the number of character written to the string
 **/
int convert_from_char1024(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char temp[4097];
	char *format = "%s";
	int count = 0;
	if (strchr((char*)data,' ')!=NULL || strchr((char*)data,';')!=NULL || ((char*)data)[0]=='\0')
		format = "\"%s\"";
	count = sprintf(temp,format,(char*)data);
	if(count > size - 1){
		return 0;
	} else {
		memcpy(buffer, temp, count);
		buffer[count] = 0;
		return count;
	}
}

/** Convert to a \e char1024
	Converts a string to a \e char1024 property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_char1024(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	char c=((char*)buffer)[0];
	switch (c) {
	case '"':
		return sscanf(buffer+1,"%1024[^\"]",data);
	default:
		return sscanf(buffer,"%1024[^\n]",data);
	}
}

/** Convert from an \e object
	Converts an \e object reference to a string.  
	@return the number of character written to the string
 **/
int convert_from_object(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	OBJECT *obj = (data ? *(OBJECT**)data : NULL);
	char temp[256];
	if (obj==NULL)
		return 0;
	if (obj->name != NULL){
		if ((strlen(obj->name) != 0) && (strlen(obj->name) < (size_t)(size - 1))){
			strcpy(buffer, obj->name);
			return 1;
		}
	}
	if (sprintf(temp,global_object_format,obj->oclass->name,obj->id)<size)
		strcpy(buffer,temp);
	else
		return 0;
	return 1;
}

/** Convert to an \e object
	Converts a string to an \e object property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_object(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	CLASSNAME cname;
	OBJECTNUM id;
	OBJECT **target = (OBJECT**)data;
	char oname[256];
	if (sscanf(buffer,"\"%[^\"]\"",oname)==1 || (strchr(buffer,':')==NULL && strncpy(oname,buffer,sizeof(oname))))
	{
		oname[sizeof(oname)-1]='\0'; /* terminate unterminated string */
		*target = object_find_name(oname);
		return (*target)!=NULL;
	}
	else if (sscanf(buffer,global_object_scan,cname,&id)==2)
	{
		OBJECT *obj = object_find_by_id(id);
		if(obj == NULL){ /* failure case, make noisy if desired. */
			*target = NULL;
			return 0;
		}
		if (obj!=NULL && strcmp(obj->oclass->name,cname)==0)
		{
			*target=obj;
			return 1;
		}
	}
	else
		*target = NULL;
	return 0;
}

/** Convert from a \e delegated data type
	Converts a \e delegated data type reference to a string.  
	@return the number of character written to the string
 **/
int convert_from_delegated(char *buffer, /**< pointer to the string buffer */
						int size, /**< size of the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	DELEGATEDVALUE *value = (DELEGATEDVALUE*)data;
	if (value==NULL || value->type==NULL || value->type->to_string==NULL) 
		return 0;
	else
		return (*(value->type->to_string))(value->data,buffer,size);
}

/** Convert to a \e delegated data type
	Converts a string to a \e delegated data type property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_delegated(char *buffer, /**< a pointer to the string buffer */
					    void *data, /**< a pointer to the data */
					    PROPERTY *prop) /**< a pointer to keywords that are supported */
{
	DELEGATEDVALUE *value = (DELEGATEDVALUE*)data;
	if (value==NULL || value->type==NULL || value->type->from_string==NULL) 
		return 0;
	else
		return (*(value->type->from_string))(value->data,buffer);
}

/** Convert from a \e boolean data type
	Converts a \e boolean data type reference to a string.  
	@return the number of characters written to the string
 **/
int convert_from_boolean(char *buffer, int size, void *data, PROPERTY *prop){
	unsigned int b = 0;
	if(buffer == NULL || data == NULL || prop == NULL)
		return 0;
//	b = *(unsigned int *)data;
	b = *(unsigned char *)data;
	if(b == 1 && (size > 4)){
		return sprintf(buffer, "TRUE");
	}
	if(b == 0 && (size > 5)){
		return sprintf(buffer, "FALSE");
	}
	return 0;
}

/** Convert to a \e boolean data type
	Converts a string to a \e boolean data type property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_boolean(char *buffer, void *data, PROPERTY *prop){
	char str[32];
	int i = 0;
	if(buffer == NULL || data == NULL || prop == NULL)
		return 0;
	memcpy(str, buffer, 31);
	for(i = 0; i < 31; ++i){
		if(str[i] == 0)
			break;
		str[i] = toupper(str[i]);
	}
	if(0 == strcmp(str, "TRUE")){
		*(unsigned int *)data = 1;
		return 1;
	}
	if(0 == strcmp(str, "FALSE")){
		*(unsigned int *)data = 0;
		return 1;
	}
	return 0;
}

int convert_from_timestamp_stub(char *buffer, int size, void *data, PROPERTY *prop){
	TIMESTAMP ts = *(int64 *)data;
	return convert_from_timestamp(ts, buffer, size);
	//return 0;
}

int convert_to_timestamp_stub(char *buffer, void *data, PROPERTY *prop){
	TIMESTAMP ts = convert_to_timestamp(buffer);
	*(int64 *)data = ts;
	return 1;
}

/** Convert from a \e double_array data type
	Converts a \e double_array data type reference to a string.  
	@return the number of character written to the string
 **/
int convert_from_double_array(char *buffer, int size, void *data, PROPERTY *prop){
	int i = 0;
	return 0;
}

/** Convert to a \e double_array data type
	Converts a string to a \e double_array data type property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_double_array(char *buffer, void *data, PROPERTY *prop){
	return 0;
}

/** Convert from a \e complex_array data type
	Converts a \e complex_array data type reference to a string.  
	@return the number of character written to the string
 **/
int convert_from_complex_array(char *buffer, int size, void *data, PROPERTY *prop){
	return 0;
}

/** Convert to a \e complex_array data type
	Converts a string to a \e complex_array data type property.  
	@return 1 on success, 0 on failure, -1 if conversion was incomplete
 **/
int convert_to_complex_array(char *buffer, void *data, PROPERTY *prop){
	return 0;
}

/**@}**/
