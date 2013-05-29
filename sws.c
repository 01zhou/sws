/* 
 * SIT CS631 Final Project
 * sws - Simple Web Server
 * Yizhou Lin / ylin8 at stevens.edu
 */


#include "global.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "protocol.h"


char* PROGRAM_NAME;
char* ROOT_PATH;

void print_err(char* msg);
int init_sock(int portno);
void handle(int sockfd,struct sockaddr_in * cli_addr);
void send_msg(int sockfd, struct Message m, FILE* fp, char* first_line);
int process_addr(char* path, char* URI);	/*convert request to path, return 1 if it is a CGI path, -1 if invalid*/

void print_usage()
{
	fprintf(stdout, "sws [ −dh] [ −c dir] [ −i address] [ −l file] [ −p port] dir\n");
	fprintf(stdout, "Example: sws -c cgi-bin -l log.txt -p 8080 -d\n");
	exit(0);
}

struct option
{
	int l;
	int d;
	int c;
	char lfile[256];
	char cdir[256];
};

struct option global_option;

int
main(int argc, char* argv[])
{
	int sockfd, newsockfd;
	int port;
	int i,j;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	char root_path[MAX_URI_LEN];

	root_path[0]=0;
	ROOT_PATH = root_path;
	PROGRAM_NAME = argv[0];

	global_option.l = 0;
	global_option.d = 0;
	global_option.c = 0;


	signal(SIGCHLD, SIG_IGN);

	port=PORT;
	for(i=1; i<argc; i++)
	{
		if(argv[i][0]=='-')
		{
			for(j=1; j>0 && argv[i][j]!=0; j++)
			{
				switch(argv[i][j])
				{
				case 'h':
					print_usage();
					break;
				case 'd':
					global_option.d = 1;
					break;
				case 'p':
					i++;
					j=-1;
					if(i<argc)
					{
						port= atoi(argv[i]);
					}
					else
					{
						print_usage();
					}
				break;
				case 'l':
					global_option.l = 1;
					i++;
					j=-1;
					if(i<argc)
					{
						strcpy(global_option.lfile, argv[i]);
					}
					else
					{
						print_usage();
					}
					break;
				case 'c':
					global_option.c = 1;
					i++;
					j=-1;
					if(i<argc)
					{
						if(argv[i][0]=='.' && argv[i][1]=='/')
						{
							strcpy(global_option.cdir, argv[i]);
						}
						else if(argv[i][0]!='/')
						{
							sprintf(global_option.cdir, "./%s", argv[i]);
						}
						else
						{
							sprintf(global_option.cdir, "%s", argv[i]);
						}
						if(global_option.cdir[strlen(global_option.cdir)-1] == '/')
						{
							global_option.cdir[strlen(global_option.cdir)-1] = 0;
						}
					}
					else
					{
						print_usage();
					}
					break;
				default:
					print_usage();
					break;
				}
			}
		}
		else
		{
			if(argv[i][0]=='.' && argv[i][1]=='/')
			{
				strcpy(ROOT_PATH, argv[i]);
			}
			else if(argv[i][0]!='/')
			{
				sprintf(ROOT_PATH, "./%s", argv[i]);
			}
			else
			{
				sprintf(ROOT_PATH, "%s", argv[i]);
			}
			if(ROOT_PATH[strlen(ROOT_PATH)-1] == '/')
			{
				ROOT_PATH[strlen(ROOT_PATH)-1] = 0;
			}
		}
	}

	if(ROOT_PATH[0] == 0)
		strcpy(ROOT_PATH, ".");


	if(global_option.d == 0)
	{
		/*daemonize*/
		if(fork()>0)
			return 0;

	}


	sockfd = init_sock(port);
	listen(sockfd, BACKLOG);
	clilen = sizeof(cli_addr);

	while(1)
	{
		newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
#ifndef DEBUG
		if(global_option.d == 1)
		{
#endif
			handle(newsockfd, &cli_addr);
#ifndef DEBUG
		}
		else
		{
			if(fork()==0)
			{
				/*child*/
				handle(newsockfd, &cli_addr);
				close(newsockfd);
				return 0;
			}
			close(newsockfd);
		}
#endif
	}
	/* shutdown(sockfd, SHUT_RDWR); */
	close(sockfd);
	return 0;

}

void handle(int sockfd, struct sockaddr_in * cli_addr)
{
	char buffer[BUFFER_SIZE];
	char addr[MAX_URI_LEN];
	char first_line[BUFFER_SIZE];
	struct Message m,res;
	int i,entlen;

	char cgi_env[BUFFER_SIZE];

	time_t now_time;
	FILE* fp;

	bzero(buffer, sizeof(buffer));
	read(sockfd, buffer, sizeof(buffer)-1);
	if(global_option.l == 1)
	{
		/*copy first line of the request*/
		for(i=0; buffer[i]!='\r' && buffer[i]!='\n' && i<BUFFER_SIZE-1; i++)
			first_line[i] = buffer[i];
		first_line[i]=0;
	}

#ifdef DEBUG
	printf("Full Message:\n%s\n\n", buffer);
#endif

	m = read_req_msg(buffer);

#ifdef DEBUG
	printf("Method: %d\n", m.req.method);
	printf("URI: %s\n", m.req.URI);
	printf("Version: %d.%d\n\n", m.req.version.major, m.req.version.minor);
#endif

	/*Prepare response*/
	/*Always use HTTP/1.0*/
	res.type = RESPONSE;
	res.res.version.major = 1;
	res.res.version.minor = 0;
	now_time = time(NULL);
	strcpy(res.res.now_time, asctime(gmtime(&now_time)));
	strcpy(res.res.server, SERVER);

	if(m.type == UNKNOWN)
	{
		/* 400 Bad Request*/
		res.res.scode = 400;
		default_rp(&res);
		set_file(&res, "400.html" , 1, NULL);
	}
	else
	{
		res.res.isCGI = process_addr(addr, m.req.URI);

		if(res.res.isCGI==1)
		{
			bzero(cgi_env, sizeof(cgi_env));
			if(m.req.method == GET)
			{
				for(i=0;addr[i]!='?' && addr[i]!=0;i++);
				if(addr[i]=='?')
				{
					sprintf(cgi_env,"QUERY_STRING=%s", &(addr[i+1]));
					addr[i]=0;
				}
#ifdef DEBUG
				printf("ENV = %s\n", cgi_env);
#endif
			}
			else if(m.req.method == POST)
			{
				if(m.req.entity != NULL)
				{
					entlen = strlen(m.req.entity);
				}
				sprintf(cgi_env,"CONTENT_LENGTH=%d", entlen);
			}
		}


		if(m.req.method == GET || m.req.method == HEAD || m.req.method == POST)
		{

			/* Test Existence */
			if(access(addr, R_OK) != 0)
			{
				/* Not Exists*/
				/* Or permission denied*/
				res.res.isCGI = 0;
				res.res.scode = 404;
				default_rp(&res);
				if(m.req.method == HEAD)
					set_file(&res, "404.html" , 0, NULL);
				else
					set_file(&res, "404.html" , 1, NULL);
			}
			else if(res.res.isCGI == 1 && access(addr, X_OK)!=0)
			{
				/* Not Executable*/
				res.res.isCGI = 0;
				res.res.scode = 403;
				default_rp(&res);
				if(m.req.method == HEAD)
					set_file(&res, "403.html" , 0, NULL);
				else
					set_file(&res, "403.html" , 1, NULL);

			}
			else
			{
				res.res.scode = 200;
					default_rp(&res);

				if(res.res.isCGI==1)
					putenv(cgi_env);
				if(m.req.method == HEAD)
					set_file(&res, addr , 0, NULL);
				else if(m.req.method == GET)
					set_file(&res, addr , 1, NULL);
				else
				{
					/*POST*/
					set_file(&res, addr , 1, m.req.entity);
				}
			}
		}
	}
	if(global_option.l == 1)
	{
		if(global_option.d == 1)
		{
			fp = (FILE*) stdout;
		}
		else
		{
			/*logging*/
			fp = fopen(global_option.lfile, "a");
		}
		fprintf(fp, "%s ", inet_ntoa(cli_addr->sin_addr));
	}
	else
	{
		fp = NULL;
	}

	send_msg(sockfd, res, fp, first_line);

	if(global_option.l == 1 && global_option.d == 0)
	{
		fclose(fp);
	}

	close(sockfd);

}
int init_sock(int portno)
{
	int sockfd;
	struct sockaddr_in serv_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		print_err("");
		return -1;
	}
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	if( bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
	{
		print_err("");
		return -1;
	}

	return sockfd;
}
void print_err(char* msg)
{
	if(msg!= 0 && strlen(msg)>0)
		strcat(msg, ": ");
	fprintf(stderr, "%s: %s%s\n",
			PROGRAM_NAME, msg, strerror(errno));
	exit(1);
}

void send_msg(int sockfd, struct Message m, FILE* fp, char* first_line)
{
	char buffer[BUFFER_SIZE];
	int n;
	if(m.type == RESPONSE)
	{
		/*Statues-Line*/
		sprintf(buffer, "HTTP/%d.%d %d %s\r\n", 
				m.res.version.major, m.res.version.minor, 
				m.res.scode, m.res.rp);
		write(sockfd, buffer, strlen(buffer));
#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif

		/*Date*/
		chomp(m.res.now_time);
		sprintf(buffer, "Date: %s\r\n", m.res.now_time);
		write(sockfd, buffer, strlen(buffer));
		if(fp!=0)
		{
			chomp(buffer);
			fprintf(fp, "%s \"%s\" ", &(buffer[6]), first_line);
			fprintf(fp, "%d %d\n", m.res.scode, m.res.content_length);
		}

#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif

		/*Server*/
		sprintf(buffer, "Server: %s\r\n", m.res.server);
		write(sockfd, buffer, strlen(buffer));
#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif

		/*Last-Modified*/
		chomp(m.res.last_modified);
		sprintf(buffer, "Last-Modified: %s\r\n", m.res.last_modified);
		write(sockfd, buffer, strlen(buffer));
#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif

		if(m.res.isCGI==0)
		{
			sprintf(buffer, "Content-Type: %s\r\n", m.res.content_type);
			write(sockfd, buffer, strlen(buffer));
#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif

		}
		if(m.res.isCGI==0)
		{
			sprintf(buffer, "Content-Length: %d\r\n", m.res.content_length);
			write(sockfd, buffer, strlen(buffer));

#ifdef DEBUG
		printf("SEND:\n %s\n",buffer);
#endif
		}
		if(m.res.isCGI==0)
		{
			write(sockfd, "\r\n", 2);
		}

		if(m.res.fd>=0)
		{
			while((n=read(m.res.fd, buffer, sizeof(buffer)))>0)
			{
				write(sockfd, buffer, n);
			}
			close(m.res.fd);
		}
	}
}

int process_addr(char* path, char* URI)
{
	int i;

	sprintf(path,"%s%s", ROOT_PATH, URI);

	/*check if it is a cgi path*/
	if(global_option.c == 0 )
		return 0;

	for(i=0;global_option.cdir[i+1]!=0 && URI[i]!=0;i++)
	{
		if(URI[i]!=global_option.cdir[i+1])
			return(0);
	}
	if(URI[i]==0 && global_option.cdir[i+1]==0)
		return(1);
	if(URI[i]==0 && global_option.cdir[i+1]=='/')
		return(1);
	if(URI[i]=='/' && global_option.cdir[i+1]==0)
		return(1);
	if(URI[i]=='/' && global_option.cdir[i+1]=='/')
		return(1);
	return(0);

}
