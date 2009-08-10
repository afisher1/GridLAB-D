/* $Id$ 
 * server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <sys/errno.h>
#include <pthread.h>

#define MAXSTR		1024		// maximum string length
#define PORTNUM		80		// default port (httpd port)

void server_request(int);	// Function to handle clients' request(s)

#include "server.h"
#include "output.h"
#include "globals.h"
#include "object.h"

void *server_routine(int sockfd)
{
	struct sockaddr_in cli_addr;
	int newsockfd,clilen;

	// repeat forever..
	while (1)
	{
		clilen = sizeof(cli_addr);

		/* accept client request and get client address */
		newsockfd = accept(sockfd,(struct sockaddr *)&cli_addr,&clilen);
		if (newsockfd<0 && errno!=EINTR)
			output_error("server accept error on fd=%d: %s", sockfd, strerror(errno));
		else if (newsockfd > 0)
		{
			handleRequest(newsockfd);
			close(newsockfd);
		}
	}
}

STATUS server_startup(int argc, char *argv[])
{
	int portNumber = PORTNUM;
	int sockfd,childpid;
	struct sockaddr_in serv_addr;
	int status;
	char buf[MAXSTR],ack[MAXSTR];
	pthread_t thread;

	/* create a new socket */
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		output_error("can't open stream socket");
		return FAILED;
	}

	bzero((char *)&serv_addr,sizeof(serv_addr)) ;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portNumber);

	/* bind socket to server address */
	if (bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	{
		output_error("can't bind local address");
		return FAILED;
	}
	
	/* listen for connection */
	listen(sockfd,5);
	output_verbose("server listening to port %d", portNumber);

	/* start the server thread */
	if (pthread_create(&thread,NULL,server_routine,sockfd))
	{
		output_error("server thread startup failed");
		return FAILED;
	}
	output_verbose("server thread started, switching to realtime mode");
	global_run_realtime = 1;
	return SUCCESS;
}


void handleRequest(int newsockfd) 
{
	char input[MAXSTR],output[MAXSTR];
	char method[64];
	char name[1024];
	char property[1024];
	char xml[] = "Content-Type: text/xml\n\n<xml>%s</xml>\n";

	/* read socket */
	read(newsockfd,(void *)input,MAXSTR);

	/* @todo process input and write to output */
	output_verbose("received incoming request [%s]", input);
	switch (sscanf(input,"%63s /%1023[^/]/%1023s",method,name,property)) {
	case 2:
		if (strcmp(method,"GET")==0)
		{
			char buf[1024];
			output_verbose("getting global '%s'", name);
			if (global_getvar(name,buf,sizeof(buf))==NULL)
				output_verbose("variable '%s' not found", name);
			else
				output_verbose("got %s=[%s]", name,buf);
			sprintf(output,xml,buf);
		}
		else if (strcmp(method,"POST")==0)
		{
			output_error("POST not supported yet");
			sprintf(output,xml,"POST not supported yet");			
		}
		else	
			sprintf(output,xml,"method not supported");
		break;
	case 3:
		if (strcmp(method,"GET")==0)
		{
			char buf[1024]="property not found";
			OBJECT *obj;
			output_verbose("getting object '%s'", name);
			obj = object_find_name(name);
			if (obj==NULL)
			{
				output_verbose("object '%s' not found", name);
				sprintf(output,xml,"object not found");
				break;
			}
			if (object_get_value_by_name(obj,property,buf,sizeof(buf)))
				output_verbose("got %s.%s=[%s]", name,property,buf);
			sprintf(output,xml,buf);
		}
		else if (strcmp(method,"POST")==0)
		{
			output_error("POST not supported yet");
			sprintf(output,xml,"POST not supported yet");			
		}
		else	
			sprintf(output,xml,"method not supported");		
		break;
	default:
		sprintf(output,xml,"invalid query");
		break;
	}

	/* write response */
	write(newsockfd,output,strlen(output));
	output_verbose("response [%s] sent", output);
}