#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <regex.h>

#include "protocol.h"
#include "runcgi.h"
#include "global.h"


int 
chomp(char* string)
	/* remove "\r" "\n" and " " from last of string. 
	 * return number of char removed
	 */
{
	int i, count;
	for(i = strlen(string)-1; i>=0; i--)
	{
		if(string[i] == '\r' || string[i] == '\n' || string[i] == ' ')
		{
			string[i] = 0;
			count ++;
		}
		else
		{
			return count;
		}
	}

	return count;
}

int
split_string(char* string, regmatch_t pmatch, 
		char* before, char* matched, char* after)
	/*
	 * Split string into 3 parts by pmatch.
	 */
{
	char temp[MAX_MSG_LEN];

	if(pmatch.rm_so ==-1)
		return 0;

	strcpy(temp, string);

	if(before)
	{
		strncpy(before, temp, pmatch.rm_so);
		before[pmatch.rm_so] = 0;
	}
	if(matched)
	{
		strncpy(matched, temp + pmatch.rm_so, 
				(pmatch.rm_eo - pmatch.rm_so));
		matched[pmatch.rm_eo - pmatch.rm_so] = 0;
	}
	if(after)
	{
		strncpy(after, temp + pmatch.rm_eo, 
				strlen(temp) - pmatch.rm_eo);
		after[strlen(temp) - pmatch.rm_eo] = 0;
	}
	return 1;

}


struct Message read_req_msg(char* msg)
{
	struct Message m;
	int ret;
	regmatch_t pmatch[1];
	int nmatch;
	int i;

	/*
	char temp[MAX_MSG_LEN];
	char before[MAX_MSG_LEN];
	*/

	char matched[MAX_MSG_LEN];
	char after[MAX_MSG_LEN];

	regex_t r;

	nmatch = 1;
	
	ret = regcomp(&r, "^\\S* ", 0);
	ret = regexec(&r, msg, nmatch, pmatch, 0);
	regfree(&r);
	if (ret == REG_NOMATCH)
	{
		m.type = UNKNOWN;
		return m;
	}

	split_string(msg, pmatch[0], NULL, matched, after);
#ifdef DEBUG
	printf("DEBUG:\n METHOD: %s\n\n", matched);
#endif

	if(strcmp(matched,"GET ")==0)
		m.req.method = GET;
	else if(strcmp(matched,"HEAD ")==0)
		m.req.method = HEAD;
	else if(strcmp(matched,"POST ")==0)
		m.req.method = POST;
	else
	{
		m.type = UNKNOWN;
		return m;
	}

	if(m.req.method == POST)
	{
		m.req.entity = NULL;
		for(i=0; strlen(msg)>4 && msg[i+3]!=0; i++)
		{
			if(msg[i]=='\r' && msg[i+1] == '\n' && msg[i+2] == '\r' && msg[i+3]=='\n')
			{
				m.req.entity = &(msg[i]);
				break;
			}
		}
	}

	ret = regcomp(&r, "^\\S* ", 0);
	ret = regexec(&r, after, nmatch, pmatch, 0);
	regfree(&r);
	if (ret == REG_NOMATCH)
	{
		m.type = UNKNOWN;
		return m;
	}

	split_string(after, pmatch[0], NULL, matched, after);
	chomp(matched);
#ifdef DEBUG
	printf("DEBUG:\n URI: %s\n\n", matched); /*should length-1*/
#endif
	strcpy(m.req.URI, matched);
	

	ret = regcomp(&r, "^HTTP/[0-9]*\\.[0-9]*\r\n", REG_NEWLINE);
	ret = regexec(&r, after, nmatch, pmatch, 0);
	regfree(&r);
	if (ret == REG_NOMATCH)
	{
		m.type = UNKNOWN;
		return m;
	}

	split_string(after, pmatch[0], NULL, matched, after);
	chomp(matched);
#ifdef DEBUG
	printf("DEBUG:\n PROTOCOL: %s\n\n", matched); /*should remove \r\n*/
#endif

	m.type = REQUEST;
	return m;

}

int default_rp(struct Message* m)
{
	if(!(m->type == RESPONSE))
		return(-2);

	switch(m->res.scode)
	{
		case 200:
			strcpy(m->res.rp, "OK");
			break;
		case 400:
			strcpy(m->res.rp, "Bad Request");
			break;
		case 403:
			strcpy(m->res.rp, "Forbidden");
			break;
		case 404:
			strcpy(m->res.rp, "Not Found");
			break;
		default:
			return(-1);
	}
	return(0);
}

int set_file(struct Message* m, char* path, int is_send_file, char* post_entity)
{
	struct stat f_stat;
	char index[MAX_URI_LEN];
	int fd[2];	/*0=r 1=w*/
	DIR* dir;

	if(m->type != RESPONSE)
		return(-1);
	if(lstat(path, &f_stat)==-1)
		return(-2);

	strcpy(m->res.last_modified, asctime(gmtime(&(f_stat.st_mtime))));
	m->res.content_length = f_stat.st_size;

	if (strlen(path)>4 && strcmp(".htm", &(path[strlen(path)-4]))==0)
		strcpy(m->res.content_type, "text/html");
	else if (strlen(path)>5 && strcmp(".html", &(path[strlen(path)-5]))==0)
		strcpy(m->res.content_type, "text/html");
	else
		strcpy(m->res.content_type, "text/plain");

	if(is_send_file == 0)
		m->res.fd = -1;
	else
	{
		/* check if path is a dir */
		dir = opendir(path);
		if(dir != NULL)
		{
			/* path is a dir*/
			closedir(dir);
			strcpy(index, path);
			strcat(index, "/index.html");

			if(access(index, R_OK)==0)
			{
				set_file(m, index, is_send_file, 0);
			}
			else
			{
				m->res.fd = run_cgi("ls", path, 1, 0);
				m->res.isCGI = 1;
			}
		}
		else
		{
			if(m->res.isCGI==1)
			{
				if(post_entity == NULL)
				{
					m->res.fd = run_cgi(path, NULL, 0, 0);
				}
				else
				{
					/*pipe2(fd, O_NONBLOCK);*/
					pipe(fd);
					write(fd[1], post_entity, strlen(post_entity));
					m->res.fd = run_cgi(path, NULL, 0, fd[0]);
				}
			}
			else
			{
				m->res.fd = open(path, O_RDONLY);
			}
		}
	}
	return(0);
}

