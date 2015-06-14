/*************************************************************************
	> File Name: server.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:48:40 PM CST
 ************************************************************************/

#include"defs.h"  
#include"server.h"
#include"log.h"
#include"multiprocess.h" 
#include"multipthread.h"
#include"socket.h"

pid_t parent;

void 
SigUserProc(int no)
{
	int status;
	SERVER_DEBUG( "SigUserProc kill\n" );
	waitpid( 0, &status, WNOHANG );
}


int 
process_server( int sockfd_new )
{
	char buffer[SO_BUF_LEN_MAX];
	size_t length = 0;
	fd_set readfd;
	struct timeval tv;

	while(1)
	{
		memset( buffer, 0, sizeof(buffer) );
		FD_ZERO(&readfd);
		FD_SET(sockfd_new ,&readfd);

		tv.tv_sec = TIME_OUT_DEFAULT;
		tv.tv_usec = 0;
		if( select(sockfd_new+1, &readfd, NULL, NULL, &tv) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			SERVER_DEBUG( "receive message:\n" );
			if( ( length = read_wrapper(sockfd_new, buffer, sizeof(buffer)) ) < 0 )
			{
				SERVER_DEBUG( "read error:%s\n", strerror(errno) );
				return -1;			
			}
		
			SERVER_DEBUG( "read size = %d\n", length );
		
			if( 0 < length )
			{
				SERVER_DEBUG( "send message:\n" );
				const char* message = get_time_string();
				if( ( length = write_wrapper( sockfd_new, message, strlen(message) ) ) < 0 )
				{
					SERVER_DEBUG( "write error:%s\n", strerror(errno) );
					return -1;			
				}
				SERVER_DEBUG( "write size = %d\n", length );
			}
		} else {
			continue;
		}
	}

	kill( parent, SIGCHLD );
	return 0;
}


int 
process_proxy( int sockfd_new, const char* host_ip, int host_port )
{
	assert( NULL != host_ip );

	struct sockaddr_in client_addr;	
	memset( &client_addr, 0, sizeof(client_addr) );
	
	int sockfd = 0;
	
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "client socket successfully\n" );	

	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons( host_port );
	
	inet_pton( AF_INET, host_ip, &client_addr.sin_addr );

	if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
	{
		SERVER_DEBUG( "client connect error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "connect successfully\n" );	

	char buffer[SO_BUF_LEN_MAX];
	size_t length = 0;
	fd_set readfd;
	struct timeval tv;

	while(1)
	{
		memset( buffer, 0, sizeof(buffer) );
		FD_ZERO(&readfd);
		FD_SET(sockfd ,&readfd);
		FD_SET(sockfd_new ,&readfd);

		tv.tv_sec = TIME_OUT_DEFAULT;
		tv.tv_usec = 0;	
		if( select(sockfd_new+1, &readfd, NULL, NULL, &tv) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			SERVER_DEBUG( "receive downstream message:\n" );
			if( ( length = read(sockfd_new, buffer, sizeof(buffer)) ) < 0 )
			{
				SERVER_DEBUG( "read error:%s\n", strerror(errno) );
				return -1;			
			}
			buffer[length] = '\0';
			SERVER_DEBUG( "read size = %d\n", length );
			for( int i = 0; i<length; i++ )
			{
				SERVER_DEBUG( "buffer[%d] = %x\n", i, buffer[i] );
			}
			if( length > 0 )
			{
				SERVER_DEBUG( "send message to upsteam:\n" );
				char message[] = "world\n";
				length = 0;
				if( ( length = write( sockfd, message, strlen(message) ) ) == -1 )
				{
					SERVER_DEBUG( "write error:%s\n", strerror(errno) );
					close( sockfd );		
					return -1;			
				}
				SERVER_DEBUG( "write size = %d\n", length );
			} else {
				SERVER_DEBUG( "read error, close socket\n" );	
				close( sockfd );		
				return -1;
			}
		} else if( FD_ISSET(sockfd, &readfd) ) {
			SERVER_DEBUG( "receive upstream message:\n" );
			if( ( length = read(sockfd, buffer, sizeof(buffer)) ) < 0 )
			{
				SERVER_DEBUG( "read error:%s\n", strerror(errno) );
				close( sockfd );		
				return -1;			
			}
			buffer[length] = '\0';
			SERVER_DEBUG( "read size = %d\n", length );
			for( int i = 0; i<length; i++ )
			{
				SERVER_DEBUG( "buffer[%d] = %x\n", i, buffer[i] );
			}
			if( length > 0 )
			{
				SERVER_DEBUG( "send message to downsteam:\n" );
				char message[] = "world\n";
				length = 0;
				if( ( length = write( sockfd_new, message, strlen(message) ) ) == -1 )
				{
					SERVER_DEBUG( "write error:%s\n", strerror(errno) );
					return -1;			
				}
				SERVER_DEBUG( "write size = %d\n", length );
			} else {
				SERVER_DEBUG( "read error, close socket\n" );	
				close( sockfd );		
				return -1;
			}
		} else {
			continue;
		}
	}

	kill( parent, SIGCHLD );
	return 0;
}



int 
server( int listen_port, const char* host_ip, int host_port )
{
	#if !defined(HTTP_PROXY) && !defined(SERVER)
	assert( NULL != host_ip );
	#endif

	struct sockaddr_in server_addr;	
	memset( &server_addr, 0, sizeof(server_addr) );
	
	int sockfd = 0;
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "server socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "server socket successfully\n" );	

	if( setsockopt_wrapper( sockfd ) < 0 )
	{
		SERVER_DEBUG( "setsockopt_wrapper error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons( listen_port );

	if( bind( sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == - 1 )
	{
		SERVER_DEBUG( "server bind error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "server bind successfully\n" );	

	if( listen( sockfd, BACK_LOG_DEFAULT ) == -1 )
	{
		SERVER_DEBUG( "server listen error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}		
	SERVER_DEBUG( "server listen successfully port = %d\n", listen_port );	
	
	parent = getpid();

	while(1) 
	{
		struct sockaddr_in client_addr;		
		memset( &client_addr, 0, sizeof(client_addr) );
		socklen_t clientsize = sizeof(struct sockaddr);

		int sockfd_new = accept( sockfd, (struct sockaddr *)&client_addr, &clientsize );
		if( sockfd_new < 0 && EINTR == errno )
		{
			SERVER_DEBUG( "catch signal:%s\n", strerror(errno) );
			continue;				
		} else if( sockfd_new < 0  ) {
			SERVER_DEBUG( "accept error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;			
		}

		if( setsockopt_nonblock( sockfd_new ) < 0 )
		{
			SERVER_DEBUG( "setsockopt_nonblock error\n" );
			close( sockfd_new );		
			return -1;			
		}

		if( getpeername( sockfd_new, (struct sockaddr *)&client_addr, &clientsize ) == -1 )
		{
			SERVER_DEBUG( "getpeername error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;			
		}
		SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port );

		#if defined(MULTI_PROCESS)
		pid_t child = fork();
		if( child < 0 )
		{
			SERVER_DEBUG( "fork error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;
		}else if( child == 0 ){
			SERVER_DEBUG( "child process\n" );
			close( sockfd );
			#if defined(PROXY)
			if( process_proxy( sockfd_new, host_ip, host_port ) < 0 )
			{
				SERVER_DEBUG( "process_proxy error:%s\n", strerror(errno) );
				close( sockfd_new );	
				return -1;
			}		
			#elif defined(HTTP_PROXY)
			if( process_http_proxy( sockfd_new ) < 0 )
			{
				SERVER_DEBUG( "process_http_proxy error\n" );
				close( sockfd_new );	
				return -1;
			}		
			#elif defined(SERVER)
			if( process_server( sockfd_new ) < 0 )
			{
				SERVER_DEBUG( "process_server error:%s\n", strerror(errno) );
				close( sockfd_new );	
				return -1;
			}		
			#endif
		} else {
			int status;
			SERVER_DEBUG( "waitpid\n" );
			close( sockfd_new );	
			waitpid( -1, &status, WNOHANG );
		}
		#elif defined(MULTI_PTHREAD)
		pthread_t pid;
		struct pthread_vargs pv;
		memset( &pv, 0, sizeof(struct pthread_vargs) );
		pv.sockfd = sockfd_new;
		if( pthread_create( &pid, NULL, process_http_proxy, &pv ) < 0 )
		{
			SERVER_DEBUG( "pthread_create error\n" );
			close( sockfd_new );	
			close( sockfd );
			return -1;
		}
		void *error = NULL;
		if( pthread_join( pid, &error ) < 0 )
		{
			SERVER_DEBUG( "pthread_join error\n" );
			close( sockfd_new );	
			close( sockfd );
			return -1;
		}
		SERVER_DEBUG( "pthread_join ret:%d\n", (int)error );
		display_pthread_vargs( &pv );
		#endif
	}
	
	close( sockfd );
	SERVER_DEBUG( "server close\n" );
	
	return 0;	
}


int 
main( int argc, char **argv )
{
	signal(SIGCHLD, SigUserProc);

	#if defined(PROXY)
	if( argc < 7 )
	#elif defined(HTTP_PROXY)||defined(SERVER)
	if( argc < 2 )
	#endif
	{
		#if defined(PROXY)
		SERVER_DEBUG( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
		#elif defined(HTTP_PROXY)||defined(SERVER)
		SERVER_DEBUG( "usage:./server -l 12306 \n" );
		#endif
		return -1;
	}

	int port = 0;
	char *host_ip = NULL;
	int host_port = 0;
	char c = 0;

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
				#if defined(PROXY)
				SERVER_DEBUG( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
				#elif defined(HTTP_PROXY)||defined(SERVER)
				SERVER_DEBUG( "usage:./server -l 12306 \n" );
				#endif
				return -1;
		}
	}

	#if defined(PROXY)
	SERVER_DEBUG( "listen_port:%d host_ip:%s host_port:%d\n", port, host_ip, host_port );
	#elif defined(HTTP_PROXY)||defined(SERVER)
	SERVER_DEBUG( "listen_port:%d\n", port );
	#endif

	(void)server( port, host_ip, host_port );

	return 0;
}


