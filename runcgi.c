#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

#include "global.h"

/* run path, an executable, and return the fd*/
int run_cgi(char* path,  char* argv0, int print_newline, int fdin)
{
	int fd[2];
	int status;
	pid_t pid;
	int rpipe;
	int wpipe;

	pipe2(fd, O_NONBLOCK);
	rpipe = fd[0];
	wpipe = fd[1];

	pid = fork();
	if(pid == 0)
	{
		if(fdin!=0)
		{
			dup2(fdin, STDIN_FILENO);
		}
		close(rpipe);
		dup2(wpipe, STDOUT_FILENO);
		if(print_newline == 1)
		{
			write(wpipe, "\r\n", 2);
			execlp(path, path, argv0,NULL);
		}
		else
		{
			execl(path, path, argv0,NULL);
		}
#ifdef DEBUG
		fprintf(stderr, "I SHALL NEVER SEE THIS MESSAGE.");
#endif
	}
	else
	{
		waitpid(pid, &status, 0);
		close(wpipe);
	}
	return rpipe;
}
