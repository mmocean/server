/*************************************************************************
	> File Name: client.c
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:49:23 PM CST
 ************************************************************************/

#include"defs.h"  
#include"log.h"
#include"client.h"

int main( int argc, char **argv )
{

	int process_number = 0;
	int connect_number = 0;
	int port = 0;
	if( argc > 3 )
	{
		process_number = atoi( argv[1] );
		connect_number = atoi( argv[2] );
		port = atoi( argv[3] );
		SERVER_DEBUG( "process_number:%d connect_number:%d port:%d\n", process_number, connect_number, port );
	} else {
		SERVER_DEBUG( "[SERVER]usage:./client 5 10 12306\n" );
		return -1;
	}

	int i = 0;
	for( i = 0; i < process_number; ++i )
	{
		SERVER_DEBUG( "fork\n" );
		pid_t pid = fork();
		if( pid == 0 )
		{
			//child process
			SERVER_DEBUG( "child process\n" );
			int sockfd = 0;
			int fd[connect_number];
			
			int j = 0;
			for( j = 0; j < connect_number; ++j )
			{
				if( ( sockfd = initial_socket() ) < 0 )
				{
					SERVER_DEBUG( "initial_socket error\n" );
					return -1;			
				}

				struct sockaddr_in client_addr;	
				memset( &client_addr, 0, sizeof(client_addr) );
				client_addr.sin_family = AF_INET;
				client_addr.sin_port = htons( port );	
				inet_pton( AF_INET, "127.0.0.1", &client_addr.sin_addr );

				if( connect( sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
				{
					SERVER_DEBUG( "client connect error:%s\n", strerror(errno) );
					close( sockfd );
					return -1;			
				}
				SERVER_DEBUG( "connect successfully\n" );	
				fd[j] = sockfd;
			}

			for( j = 0; j < connect_number; ++j )
			{
				const char* message = "benchmark test";
				static int len = 0;
				if( 0 == len )
				{
					len = strlen(message);
				}

				sockfd = fd[j];
				int length = 0;
				if( ( length = write( sockfd, message, len ) ) == -1 )
				{
					SERVER_DEBUG( "write error:%s\n", strerror(errno) );
					close( sockfd );		
					return -1;			
				}
				SERVER_DEBUG( "write size = %d\n", length );

				close( sockfd );
			}

			for( j = 0; j < connect_number; ++j )
			{
				sockfd = fd[j];
				close( sockfd );
			}

			exit(0);	
		} else if( pid < 0 ) {
			SERVER_DEBUG( "fork error:%s\n", strerror(errno) );
			return -1;
		}
	}

	pid_t pid = 0;
	while( ( pid = wait(NULL) ) > 0 )
	{
		SERVER_DEBUG( "pid:%d\n", pid );
	}

	SERVER_DEBUG( "client\n" );

	return 0;
}
