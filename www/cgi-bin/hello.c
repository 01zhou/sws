#include <stdio.h>
#include <time.h>

int main()
{
	time_t t;
	printf("Content-Type: text/html\r\n\r\n");
	printf("<h1>Now time</h1>");
	t = time(NULL);
	printf("%s<br/><br/>", ctime(&t));

	printf("<a href=\"hello.c\">Link to a file that is not executable </a><br/>");
}

