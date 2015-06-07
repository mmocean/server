/*************************************************************************
	> File Name: server.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:48:40 PM CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<getopt.h>

#define SIG_USER 14
#define MAX_LEN 65535

extern int errno;

pid_t parent;

void 
SigUserProc(int no)
{
	int status;
	printf( "kill\n" );
	//wait(&status);	
	waitpid( -1, &status, WNOHANG );
}

int 
process( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen, const char* host_ip, int host_port )
{
	if( getpeername( sockfd_new, (struct sockaddr *)addr, addrlen ) == -1 )
	{
		printf( "getpeername error:%s\n", strerror(errno) );
		return -1;			
	}

	printf( "ESTABLISHED\n" );
	printf( "listen addr = %s:%u\n", inet_ntoa(addr->sin_addr), addr->sin_port );

	struct sockaddr_in client_addr;
	
	memset( &client_addr, 0, sizeof(client_addr) );
	
	int sockfd = 0;
	
	errno = 0;
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		printf( "socket error:%s\n", strerror(errno) );
		return -1;
	}
	printf( "client socket success\n" );	

	client_addr.sin_family = AF_INET;
	//client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_addr.sin_port = htons( host_port );
	
	inet_pton( AF_INET, host_ip, &client_addr.sin_addr );

	if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
	{
		printf( "client connect error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	printf( "connect success\n" );	

	char buffer[MAX_LEN];
	size_t length = 0;
	fd_set readfd;
	struct timeval tv;

	while(1)
	{
		memset( buffer, 0, sizeof(buffer) );
		FD_ZERO(&readfd);
		FD_SET(sockfd ,&readfd);
		FD_SET(sockfd_new ,&readfd);

		tv.tv_sec = 2;
		tv.tv_usec = 0;
		int timeout = (tv.tv_sec*1000000+tv.tv_usec)/1000000;
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv ) ) < 0 )
		{
			printf( "select error:%s\n", strerror(errno) );
			close( sockfd_new );		
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			printf( "receive downstream message:\n" );
			if( ( length = read(sockfd_new, buffer, sizeof(buffer)) ) < 0 )
			{
				printf( "read error:%s\n", strerror(errno) );
				close( sockfd_new );		
				return -1;			
			}
			buffer[length] = '\0';
			printf( "read size = %d\n", length );
			for( int i = 0; i<length; i++ )
			{
				printf( "buffer[%d] = %x\n", i, buffer[i] );
			}
			//if( strcmp( buffer, "hello" ) == 0 )
			if( length > 0 )
			{
				printf( "send message to upsteam:\n" );
				char message[] = "world\n";
				length = 0;
				if( ( length = write( sockfd, message, strlen(message) ) ) == -1 )
				{
					printf( "write error:%s\n", strerror(errno) );
					close( sockfd );		
					return -1;			
				}
				printf( "write size = %d\n", length );
			} else {
				printf( "read error, close socket" );	
				close( sockfd );		
				close( sockfd_new );		
				return -1;
			}
		} else if( FD_ISSET(sockfd, &readfd) ) {
			printf( "receive upstream message:\n" );
			if( ( length = read(sockfd, buffer, sizeof(buffer)) ) < 0 )
			{
				printf( "read error:%s\n", strerror(errno) );
				close( sockfd );		
				return -1;			
			}
			buffer[length] = '\0';
			printf( "read size = %d\n", length );
			for( int i = 0; i<length; i++ )
			{
				printf( "buffer[%d] = %x\n", i, buffer[i] );
			}
			//if( strcmp( buffer, "hello" ) == 0 )
			if( length > 0 )
			{
				printf( "send message to downsteam:\n" );
				char message[] = "world\n";
				length = 0;
				if( ( length = write( sockfd_new, message, strlen(message) ) ) == -1 )
				{
					printf( "write error:%s\n", strerror(errno) );
					close( sockfd_new );		
					return -1;			
				}
				printf( "write size = %d\n", length );
			} else {
				printf( "read error, close socket" );	
				close( sockfd );		
				close( sockfd_new );		
				return -1;
			}
		} else {
			printf( "listen interval:%d s\n", timeout );
			continue;
		}
	}

	close( sockfd_new );		
	kill( parent, SIGCHLD );
	return 0;
}

int 
server( int listen_port, const char* host_ip, int host_port )
{
	struct sockaddr_in server_addr;
	
	memset( &server_addr, 0, sizeof(server_addr) );
	
	int sockfd = 0;
	
	errno = 0;
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		printf( "server socket error:%s\n", strerror(errno) );
		return -1;
	}
	printf( "server socket success\n" );	

	int reuse = 1;
	if( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, (socklen_t)sizeof(int) ) < 0 )
	{
		printf( "server setsockopt error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	printf( "server setsockopt success\n" );	

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons( listen_port );

	if( bind( sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == - 1 )
	{
		printf( "bind error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	printf( "server bind success\n" );	

	if( listen( sockfd, 5) == -1 )
	{
		printf( "server listen error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}		
	printf( "server listen success port = %d\n", listen_port );	
	
	parent = getpid();
	int status;

	printf( "server bind success\n" );	
	printf( "server bind success\n" );	
	while(1) 
	{
		int sockfd_new = 0;
		
		struct sockaddr_in client_addr;		
		memset( &client_addr, 0, sizeof(client_addr) );
		socklen_t clientsize = sizeof( client_addr );
	
		sockfd_new = accept( sockfd, (struct sockaddr *)&client_addr, &clientsize );

		if( sockfd_new < 0 && errno == EINTR )
		{
			printf( "catch signal:%s\n", strerror(errno) );
			continue;				
		} else if( sockfd_new < 0  ) {
			printf( "accept error:%s\n", strerror(errno) );
			return -1;			
		}			
		printf( "sockfd_new:%d\n", sockfd_new );

		if( fcntl( sockfd_new, F_SETFL, O_NONBLOCK) < 0 )
		{
			printf( "fcntl error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;				
		}

		pid_t child = fork();

		if( child < 0 )
		{
			printf( "fork error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;
		}else if( child == 0 ){
			printf( "child process\n" );
			close( sockfd );
			//pid_t pid = fork();
			pid_t pid = 0;
			if( pid < 0 )
			{
				printf( "fork error:%s\n", strerror(errno) );
				close( sockfd_new );	
				close( sockfd );
				return -1;
			} else if( pid == 0 ){
				if( process( sockfd_new, &client_addr, &clientsize, host_ip, host_port ) < 0 )
				{
					printf( "process error:%s\n", strerror(errno) );
					return -1;
				}	
			} else {
				return 0;
			}
		} else {
			printf( "waitpid\n" );
			close( sockfd_new );	
			//waitpid( child, &status, 0 );
			waitpid( -1, &status, WNOHANG );
		}
	}
	
	close( sockfd );
	
	return 0;	
}


int 
main( int argc, char **argv )
{
	signal(SIGCHLD, SigUserProc);

	int port = 0;
	char *host_ip = NULL;
	int host_port = 0;
	char c = 0;

	if( argc < 7 )
	{
		printf( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
		return -1;
	}
	while( (c = getopt( argc, argv, "l:s:p:" )) > 0 )
	{
		switch( c )
		{
			case ('l'):
				port = atoi( optarg );
				break;
			case ('s'):
				host_ip = optarg;
				break;
			case ('p'):
				host_port = atoi( optarg );
				break;
			default:
				printf( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
				return -1;
		}
	}

	printf( "listen_port:%d\nhost_ip:%s\nhost_port:%d\n", port, host_ip, host_port );

	server( port, host_ip, host_port );
	
	printf( "server close\n" );

	return 0;
 }
