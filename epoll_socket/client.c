#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define PORT 7777
#define MAX_LINE 2048

int Curid = 0;

int max(int a , int b)
{
	return a > b ? a : b;
}

/*readline函数实现*/
ssize_t readline(int fd, char *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = read(fd, &c,1)) == 1) {
			if (c == 3)
			{
				*ptr = 0;
				return n - 1;
			}
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

void handle_touch(char sendline[MAX_LINE], char recvline[MAX_LINE], int sockfd1, int sockfd2)
{
	int sockfd;
	if (!(Curid++ % 2))
	{
		sockfd = sockfd1;
	}
	else
	{
		sockfd = sockfd2;
	}

	write(sockfd, sendline, strlen(sendline));		
	bzero(recvline , MAX_LINE);
	if(readline(sockfd , recvline , MAX_LINE) <= 0)
	{
		perror("server terminated prematurely when touch file");
		exit(1);
	}//if

	if (!strcmp(recvline, "success"))
	{
		if(fputs("succeed to create file\n" , stdout) == EOF)
		{
			perror("fputs error");
			exit(1);
		}//if
	}
	else
	{
		if(fputs("fail to create file\n" , stdout) == EOF)
		{
			perror("fputs error");
			exit(1);
		}//if
	}
}

void handle_ls(char sendline[MAX_LINE], char recvline[MAX_LINE], int sockfd1, int sockfd2)
{
	const char *cmd = "ls";
	int rc;

	fputs("Files in server1:\n", stdout);
	write(sockfd1, cmd, strlen(cmd));
	bzero(recvline , MAX_LINE);
	while ((rc = readline(sockfd1 , recvline , MAX_LINE)) > 0)
	{
		if(fputs(recvline , stdout) == EOF)
		{
			perror("fputs error");
			exit(1);
		}
	}
	if (rc < 0)
	{
		perror("server1 terminated prematurely when ls file");
		exit(1);
	}

	fputs("Files in server2:\n", stdout);
	write(sockfd2, cmd, strlen(cmd));
	bzero(recvline , MAX_LINE);
	while ((rc = readline(sockfd2 , recvline , MAX_LINE)) > 0)
	{
		if(fputs(recvline , stdout) == EOF)
		{
			perror("fputs error");
			exit(1);
		}
	}
	if (rc < 0)
	{
		perror("server2 terminated prematurely when ls file");
		exit(1);
	}
}

void handle_rm(char sendline[MAX_LINE], char recvline[MAX_LINE], int sockfd1, int sockfd2)
{
	const char *cmd = sendline;
	int rc;
	write(sockfd1, cmd, strlen(cmd));
	bzero(recvline , MAX_LINE);
	if ((rc = readline(sockfd1 , recvline , MAX_LINE)) > 0)
	{
		if (!strncmp(recvline, "success", strlen("success")))
		{
			fputs("succeed to delete file from server1\n", stdout);
		}		
		else
		{
			fputs("fail to delete file\n", stdout);
		}
	}
	
	write(sockfd2, cmd, strlen(cmd));
	bzero(recvline , MAX_LINE);
	if (readline(sockfd2 , recvline , MAX_LINE) > 0)
	{
		if (!strncmp(recvline, "success", strlen("success")))
		{
			fputs("succeed to delete file from server2\n", stdout);
		}		
	}
}

/* get user's input */
void str_cli(int sockfd1, int sockfd2)
{
	/*发送和接收缓冲区*/
	char sendline[MAX_LINE] , recvline[MAX_LINE];
	while(fgets(sendline , MAX_LINE , stdin) != NULL)	
	{
		if (!strncmp(sendline, "touch", strlen("touch")))
		{
			handle_touch(sendline, recvline, sockfd1, sockfd2);
		}
		else if (!strncmp(sendline, "ls", strlen("ls")))
		{
			handle_ls(sendline, recvline, sockfd1, sockfd2);
		}
		else if (!strncmp(sendline, "rm", strlen("rm")))
		{
			handle_rm(sendline, recvline, sockfd1, sockfd2);
		}
		else 
		{
			printf("wrong command!\n");
		}
	}//while
}

int main(int argc , char **argv)
{
	/*声明套接字和链接服务器地址*/
    int sockfd1, sockfd2;
    struct sockaddr_in servaddr1, servaddr2;

    /*判断是否为合法输入*/
    if(argc != 3)
    {
        perror("usage:./client <IPaddress> <IPaddress>");
        exit(1);
    }//if

    /*(1) 创建套接字*/
    if((sockfd1 = socket(AF_INET , SOCK_STREAM , 0)) == -1)
    {
        perror("socket1 error");
        exit(1);
    }
	if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket2 error");
		exit(1);
	}
	
    /*(2) 设置链接服务器地址结构*/
    bzero(&servaddr1 , sizeof(servaddr1));
    servaddr1.sin_family = AF_INET;
    servaddr1.sin_port = htons(PORT);
    if(inet_pton(AF_INET , argv[1] , &servaddr1.sin_addr) < 0)
    {
        printf("inet_pton error for %s\n",argv[1]);
        exit(1);
    }
    bzero(&servaddr2 , sizeof(servaddr2));
    servaddr2.sin_family = AF_INET;
    servaddr2.sin_port = htons(PORT);
    if(inet_pton(AF_INET , argv[2] , &servaddr2.sin_addr) < 0)
    {
        printf("inet_pton error for %s\n",argv[1]);
        exit(1);
    }


    /*(3) 发送链接服务器请求*/
    if(connect(sockfd1 , (struct sockaddr *)&servaddr1 , sizeof(servaddr1)) < 0)
    {
        perror("connect 1th server error");
        exit(1);
    }
    if(connect(sockfd2 , (struct sockaddr *)&servaddr2 , sizeof(servaddr2)) < 0)
    {
        perror("connect 2th server error");
        exit(1);
    }

	/*调用消息处理函数*/
	str_cli(sockfd1, sockfd2);	
	exit(0);
}
