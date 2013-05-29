#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "global.h"

enum Method
{
	GET, 
	HEAD, 
	POST, 
	EXTENSION
};

struct Version
{
	int major;
	int minor;
};

enum MessageType
{
	UNKNOWN,
	REQUEST,
	RESPONSE
};

struct Request
{

	enum Method method;
	char URI[MAX_URI_LEN];
	struct Version version;
	char* entity;
};

struct Response
{
	struct Version version;
	int scode;	
	char rp[MAX_RP_LEN];
	char now_time[64];
	char server[64];
	char last_modified[64];
	char content_type[64];
	int content_length;
	int fd;
	/* if fd>=0 then send the file after the messaage head
	 * if fd<0 then don't send the file.
	 */
	int isCGI;
	/* if isCGI == 1 then donot send content_type or content_length*/
};


struct Message
{
	enum MessageType type;
	union{
		struct Request req;
		struct Response res;
	};
};

struct Message read_req_msg(char* msg);
/* Convert a string message to struct Message */

int default_rp(struct Message* m);
/* if m is a response message, assign its rp to default*/

int set_file(struct Message* m, char* path, int is_send_file, char* post_entity);

int chomp(char* string);

#endif

