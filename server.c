/*************************************************************************
	> File Name: server.c
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:48:40 PM CST
 ************************************************************************/

#include"defs.h"  
#include"server.h"
#include"log.h"
#include"multiprocess.h" 
#include"socket.h"
#include <sys/epoll.h>

#if defined(MULTI_PTHREAD)
#include"multipthread.h"
#endif


pid_t parent;
int exit_flag = 0;


sighandler_t 
signal_intr( int no, sighandler_t func )  
{  
	struct sigaction act,oact;  
	act.sa_handler = func;  
	sigemptyset(&act.sa_mask);  
	act.sa_flags = 0;  
	act.sa_flags |= SA_INTERRUPT;  
	if( sigaction( no,&act,&oact ) < 0 )  
	{
		return (SIG_ERR);  
	}
	return (oact.sa_handler);  
}  


void SigInt( int no )
{
	SERVER_DEBUG( "catch signal SIGINT\n" );
	exit_flag = 1;
}


void 
SigUserProc( int no )
{
	int status;
	SERVER_DEBUG( "SigUserProc kill\n" );
	waitpid( 0, &status, WNOHANG );
}


int 
process_server( int sockfd_new )
{
	while(1)
	{
		fd_set readfd;
		FD_ZERO(&readfd);
		FD_SET(sockfd_new ,&readfd);

		struct timeval tv;
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
			char buffer[SO_BUF_LEN_MAX];
			memset( buffer, 0, sizeof(buffer) );

			SERVER_DEBUG( "receive message:\n" );
			
			ssize_t length = 0;
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
			} else {
				SERVER_DEBUG( "read error, close socket\n" );	
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
process_proxy( int sockfd_new, const char* host_ip, int host_port )
{
	assert( NULL != host_ip );

	int sockfd = 0;
	if( ( sockfd = initial_socket() ) < 0 )
	{
		SERVER_DEBUG( "initial_socket error\n" );
		return -1;			
	}
	
	struct sockaddr_in client_addr;	
	memset( &client_addr, 0, sizeof(client_addr) );
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons( host_port );	
	inet_pton( AF_INET, host_ip, &client_addr.sin_addr );

	if( connect( sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
	{
		SERVER_DEBUG( "client connect error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "connect successfully\n" );	

	char buffer[SO_BUF_LEN_MAX];
	memset( buffer, 0, sizeof(buffer) );
	ssize_t length = 0;

	while(1)
	{
		fd_set readfd;
		FD_ZERO(&readfd);
		FD_SET(sockfd, &readfd);
		FD_SET(sockfd_new, &readfd);

		struct timeval tv;
		tv.tv_sec = TIME_OUT_DEFAULT;
		tv.tv_usec = 0;	

		if( select(sockfd_new+1, &readfd, NULL, NULL, &tv) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			close( sockfd );		
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			SERVER_DEBUG( "receive downstream message:\n" );
			if( ( length = read(sockfd_new, buffer, sizeof(buffer)) ) < 0 )
			{
				SERVER_DEBUG( "read error:%s\n", strerror(errno) );
				return -1;			
			}
			SERVER_DEBUG( "read size = %d\n", length );

			if( length > 0 )
			{
				buffer[length] = '\0';
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
			SERVER_DEBUG( "read size = %d\n", length );

			if( length > 0 )
			{
				buffer[length] = '\0';
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


int handle_new_connection( int sockfd )
{
	SERVER_DEBUG( "handle_new_connection\n" );
	while( 1 )
	{
		struct sockaddr_in client_addr;		
		memset( &client_addr, 0, sizeof(client_addr) );
		socklen_t clientsize = sizeof(struct sockaddr);

		int sockfd_new = accept( sockfd, (struct sockaddr *)&client_addr, &clientsize );
		if( sockfd_new < 0 && EINTR == errno )
		{
			SERVER_DEBUG( "catch signal from blocking system call [accept]:%s\n", strerror(errno) );
			continue;
		} else if( sockfd_new < 0  ) {
			SERVER_DEBUG( "accept error:%s\n", strerror(errno) );
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
			return -1;
		}

		SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port );

		return sockfd_new;
	}

	return -1;
}


int epoll_server( int sockfd )
{
	#define EVENTS_MAX 100
	int epfd = epoll_create( EVENTS_MAX );
	if( epfd < 0 )
	{
		SERVER_DEBUG( "epoll_create error:%s\n", strerror( errno ) );	
		return -1;
	}

	struct epoll_event epollev;
	epollev.events = EPOLLIN;
	epollev.data.fd = sockfd;
	if( epoll_ctl( epfd, EPOLL_CTL_ADD, sockfd, &epollev) < 0 )
	{
		SERVER_DEBUG( "epoll_ctl error:%s\n", strerror( errno ) );	
		close( epfd );
		return -1;
	}
	
	struct epoll_event events[EVENTS_MAX];
	while( 1 )
	{
		#define TIME_OUT 5000
		int ret = epoll_wait( epfd, events, EVENTS_MAX, TIME_OUT );
		SERVER_DEBUG( "epoll_wait ret:%d\n", ret );
		if( ret < 0 )
		{
			SERVER_DEBUG( "epoll_wait error:%s\n", strerror( errno ) );	
			close( epfd );
			//handle other fdes
			return -1;
		} else if( 0 == ret ) {
			SERVER_DEBUG( "TIME_OUT,listenning...\n" );
			continue;
		} else {
			int i = 0;
			for( i = 0; i < ret; ++i )
			{
				if( events[i].data.fd == sockfd )
				{
					//accept new connection	
					int sockfd_new = handle_new_connection( sockfd );
					if( sockfd_new < 0 )
					{
						SERVER_DEBUG( "handle_new_connection error\n" );
						return -1;
					}
					
					//epollev.events = EPOLLIN|EPOLLOUT|EPOLLET;
					epollev.events = EPOLLIN|EPOLLET;
					epollev.data.fd = sockfd_new;
					if( epoll_ctl( epfd, EPOLL_CTL_ADD, sockfd_new, &epollev) < 0 )
					{
						SERVER_DEBUG( "epoll_ctl error:%s\n", strerror( errno ) );	
						close( epfd );
						return -1;
					}
				} else {
					//read-write
					int sockfd = events[i].data.fd;

					char buffer[SO_BUF_LEN_MAX];
					memset( buffer, 0, sizeof(buffer) );

					SERVER_DEBUG( "receive message:\n" );

					ssize_t length = 0;
					if( ( length = read_wrapper(sockfd, buffer, sizeof(buffer)) ) < 0 )
					{
						SERVER_DEBUG( "read error:%s\n", strerror(errno) );
						return -1;			
					}

					SERVER_DEBUG( "read size = %d\n", length );
					if( 0 == length )
					{
						SERVER_DEBUG( "closing socket\n" );
						epollev.data.fd = sockfd;
						if( epoll_ctl( epfd, EPOLL_CTL_DEL, sockfd, &epollev) < 0 )
						{
							SERVER_DEBUG( "epoll_ctl error:%s\n", strerror( errno ) );	
							close( epfd );
							return -1;
						}
					} else {
						SERVER_DEBUG( "buffer:%s\n", buffer );		
						const char* message = get_time_string();
						SERVER_DEBUG( "send message:%s\n", message );
						if( ( length = write_wrapper( sockfd, message, strlen(message) ) ) < 0 )
						{
							SERVER_DEBUG( "write error:%s\n", strerror(errno) );
							return -1;			
						}
						SERVER_DEBUG( "write size = %d\n", length );
					}
				}	
			}	
		}
	}

	return 0;
}


int 
server( int listen_port, const char* host_ip, int host_port )
{
	#if !defined(HTTP_PROXY) && !defined(SERVER)
	assert( NULL != host_ip );
	#endif
	
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

	struct sockaddr_in server_addr;	
	memset( &server_addr, 0, sizeof(server_addr) );
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

	#if defined(EPOLL)
	return epoll_server( sockfd );
	#endif

	parent = getpid();
	
	struct pthread_vargs *list = NULL;

	while(1) 
	{
		if( exit_flag )
		{
			break;
		}
	
		int sockfd_new = handle_new_connection( sockfd );
		if( sockfd_new < 0 )
		{
			SERVER_DEBUG( "handle_new_connection\n" );
			break;
		}

		#if !defined(MULTI_PTHREAD)
		pid_t child = fork();
		if( child < 0 )
		{
			SERVER_DEBUG( "fork error:%s\n", strerror(errno) );
			close( sockfd_new );	
			break;
		}else if( child == 0 ){
			SERVER_DEBUG( "child process\n" );
			close( sockfd );
			#if defined(PROXY)
			if( process_proxy( sockfd_new, host_ip, host_port ) < 0 )
			{
				SERVER_DEBUG( "process_proxy error:%s\n", strerror(errno) );
				close( sockfd_new );	
				break;
			}		
			#elif defined(HTTP_PROXY)
			if( process_http_proxy( sockfd_new ) < 0 )
			{
				SERVER_DEBUG( "process_http_proxy error\n" );
				close( sockfd_new );	
				break;
			}		
			#elif defined(SERVER)
			if( process_server( sockfd_new ) < 0 )
			{
				SERVER_DEBUG( "process_server error:%s\n", strerror(errno) );
				close( sockfd_new );	
				break;
			}		
			#endif
		} else {
			int status;
			SERVER_DEBUG( "waitpid\n" );
			close( sockfd_new );	
			waitpid( -1, &status, WNOHANG );
		}
		#elif defined(MULTI_PTHREAD)
		struct pthread_vargs *pv = (struct pthread_vargs *)malloc(sizeof(struct pthread_vargs));
		if( NULL == pv )
		{
			SERVER_DEBUG( "malloc error:%s\n", strerror(errno) );
			close( sockfd_new );	
			break;
		}
		memset( pv, 0, sizeof(struct pthread_vargs) );
		if( NULL == list )
		{
			list = pv;
			list->next = NULL;
		} else {
			struct pthread_vargs *iter = list;
			while( NULL != iter->next )
			{
				iter = iter->next;
			}
			iter->next = pv;
			pv->next = NULL;
			iter = NULL;
		}
		pv->sockfd = sockfd_new;
		if( pthread_create( &pv->pid, NULL, pthread_http_proxy, pv ) < 0 )
		{
			SERVER_DEBUG( "pthread_create error\n" );
			close( sockfd_new );	
			break;
		}	
		#endif
	}

	#if defined(MULTI_PTHREAD)
	(void)pthread_join_wrapper( list );
	pthread_vargs_cleanup( list );
	#endif

	close( sockfd );
	SERVER_DEBUG( "server close\n" );
	
	return 0;	
}


int 
main( int argc, char **argv )
{
	signal( SIGCHLD, SigUserProc );
	signal( SIGPIPE, SIG_IGN );
	signal_intr( SIGINT, SigInt );
	signal_intr( SIGTERM, SigInt );

	#if defined(PROXY)
	if( argc < 7 )
	#elif defined(HTTP_PROXY) || defined(SERVER) || defined(MULTI_PTHREAD)
	if( argc < 3 )
	#endif
	{
		#if defined(PROXY)
		SERVER_DEBUG( "[PROXY]usage:./server -l 8087 -s 127.0.0.1 -p 12307\n" );
		#elif defined(HTTP_PROXY)||defined(SERVER) || defined(MULTI_PTHREAD)
		SERVER_DEBUG( "[SERVER]usage:./server -l 8087 \n" );
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
				SERVER_DEBUG( "[PROXY]usage:./server -l 8087 -s 127.0.0.1 -p 12307\n" );
				#elif defined(HTTP_PROXY) || defined(SERVER) || defined(MULTI_PTHREAD)
				SERVER_DEBUG( "[SERVER]usage:./server -l 8087 \n" );
				#endif
				return -1;
		}
	}

	#if defined(PROXY)
	SERVER_DEBUG( "[PROXY] listen_port:%d host_ip:%s host_port:%d\n", port, host_ip, host_port );
	#elif defined(HTTP_PROXY) || defined(SERVER) || defined(MULTI_PTHREAD)
	SERVER_DEBUG( "[SERVER] listen_port:%d\n", port );
	#endif

	(void)server( port, host_ip, host_port );

	return 0;
}


