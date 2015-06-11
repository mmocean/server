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
#include<netdb.h>
#include<netinet/in.h>


#define SIG_USER 14
#define SO_BUF_LEN_MAX 65536
#define HOST_IP_LEN_MAX 16
#define HTTP_PROXY

#define SERVER_DEBUG(...)\
printf("[DEBUG %d] [%s:%d] ", getpid(), __FILE__, __LINE__ );\
printf( __VA_ARGS__ );\

extern int errno;
extern int h_errno;

pid_t parent;



void 
SigUserProc(int no);

int 
process_http_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen );

int 
parse_http_data( const char* http_data, size_t http_data_len, char *host_ip, size_t host_ip_len );

size_t
read_wrapper( int fd, char *buf, size_t count );

size_t
write_wrapper( int fd, const char *buf, size_t count );

int 
server( int listen_port, const char* host_ip, int host_port );

int 
process_server( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen );

int 
process_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen, const char* host_ip, int host_port );






void 
SigUserProc(int no)
{
	int status;
	SERVER_DEBUG( "SigUserProc\n" );
	//wait(&status);	
	//waitpid( -1, &status, WNOHANG );
	waitpid( 0, &status, WNOHANG );
}


int 
process_http_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen )
{
	if( getpeername( sockfd_new, (struct sockaddr *)addr, addrlen ) == -1 )
	{
		SERVER_DEBUG( "getpeername error:%s\n", strerror(errno) );
		return -1;			
	}

	SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(addr->sin_addr), addr->sin_port );

	struct sockaddr_in client_addr;
	memset( &client_addr, 0, sizeof(client_addr) );
	
	int sockfd = 0;
	
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "client socket success\n" );	

	const int host_port = 80;
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(host_port);
		
	char buffer[SO_BUF_LEN_MAX];
	size_t length = 0;
	fd_set readfd;
	struct timeval tv;

	while(1)
	{
		memset( buffer, 0, sizeof(buffer) );
		FD_ZERO(&readfd);
		FD_SET(sockfd_new ,&readfd);

		tv.tv_sec = 60;
		tv.tv_usec = 0;
		int timeout = (tv.tv_sec*1000000+tv.tv_usec)/1000000;
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv ) ) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			SERVER_DEBUG( "receive downstream message:\n" );
			if( ( length = read_wrapper(sockfd_new, buffer, sizeof(buffer)) ) < 0 )
			{
				SERVER_DEBUG( "read error:%s\n", strerror(errno) );
				return -1;			
			}
			SERVER_DEBUG( "read size = %d \n", length );

			if( length > 0 )
			{
				//to do
				buffer[length] = '\0';
				//parse http_data
				char host_ip[HOST_IP_LEN_MAX] = {0};
				memset( host_ip, 0, sizeof(host_ip) );
				if( parse_http_data( buffer, length, host_ip, sizeof(host_ip) ) < 0 )
				{
					SERVER_DEBUG( "parse_http_data error\n" );
					close( sockfd );
					return -1;			
				} else {
					inet_pton( AF_INET, host_ip, &client_addr.sin_addr );
					if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
					{
						SERVER_DEBUG( "client connect %s:%d error:%s\n", host_ip, host_port, strerror(errno) );
						close( sockfd );
						return -1;			
					} else {
						SERVER_DEBUG( "connect success %s:%d \n", host_ip, host_port );
						SERVER_DEBUG( "send message to upsteam:\n" );
						if( ( length = write_wrapper( sockfd, buffer, length ) ) == -1 )
						{
							SERVER_DEBUG( "write error:%s\n", strerror(errno) );
							close( sockfd );		
							return -1;			
						}
						SERVER_DEBUG( "write size = %d\n", length );		   
						
						FD_ZERO(&readfd);
						FD_SET(sockfd ,&readfd);

						tv.tv_sec = 60;
						tv.tv_usec = 0;
						
						int timeout = (tv.tv_sec*1000000+tv.tv_usec)/1000000;

						if( ( nRet = select(sockfd+1, &readfd, NULL, NULL, &tv ) ) < 0 )
						{
							if( EINTR == errno )
							{
								continue;
							}
							SERVER_DEBUG( "select error:%s\n", strerror(errno) );
							return -1;			
						} else if( FD_ISSET(sockfd, &readfd) ) {
							SERVER_DEBUG( "receive upstream message:\n" );
							if( ( length = read_wrapper(sockfd, buffer, sizeof(buffer)) ) < 0 )
							{
								SERVER_DEBUG( "read error:%s\n", strerror(errno) );
								return -1;			
							}
							SERVER_DEBUG( "read size = %d \n", length );
							if( length > 0 )
							{
								SERVER_DEBUG( "send message to downsteam:\n" );
								if( ( length = write_wrapper( sockfd_new, buffer, length ) ) == -1 )
								{
									SERVER_DEBUG( "write error:%s\n", strerror(errno) );
									close( sockfd_new );		
									return -1;			
								}
								SERVER_DEBUG( "write size = %d\n", length );		   
							} else {
								SERVER_DEBUG( "receive upstream message error, close socket\n" );	
								close( sockfd );		
								break;
							}
						} else {						
							close( sockfd );
							break;
						}
					}
				}
			} else {
				SERVER_DEBUG( "receive downstream message error, close socket\n" );	
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


size_t
read_wrapper( int fd, char *buf, size_t count )
{
	size_t c = 0;
	while(1)
	{
		size_t length = read( fd, buf+c, count-c );
		if( -1 == length )
		{
			if( EINTR == errno )
			{
				SERVER_DEBUG( "ignore EINTR\n" );
				continue;
			}
			break;
		} else if( 0 < length ){
			c += length;
		} else {
			break;
		}
	}
	return c;
}


size_t
write_wrapper( int fd, const char *buf, size_t count )
{
	size_t c = 0;
	while(1)
	{
		size_t length = write( fd, buf+c, count-c );
		if( 0 > length )
		{
			if( EINTR == errno )
			{
				continue;
			}
			break;
		} else if( 0 < length ){
			c += length;
		} else {
			break;
		}
	}
	return c;
}


int 
parse_http_data( const char* http_data, size_t http_data_len, char *host_ip, size_t host_ip_len )
{
	SERVER_DEBUG( "#########dump http###########\n" );
	SERVER_DEBUG( "\n%s", http_data );
	SERVER_DEBUG( "#########dump http###########\n" );

	if( 0 == strncasecmp( "CONNECT", http_data, strlen("CONNECT") ) )
	{
		SERVER_DEBUG( "do not support https right now, refuse\n" );	
		return -1;			
	}

	//parse host value
	char host[128] = {0};
	memset( host, 0, sizeof(host) );
	int index = 0;
	do {
		char line[256] = {0};
		memset( line, 0, sizeof(line) );
		int count = sscanf( http_data+index, "%[^\n]%*c", line );	
		if( count > 0 )
		{
			SERVER_DEBUG( "line: %s\n", line );
			if( 0 == strncasecmp( "HOST:", line, strlen("HOST:") ) )
			{
				char *p = line;
				p += strlen( "HOST:" );
				while((*p < '0') || 
					  (*p > '9' && *p < 'A') ||
					  (*p > 'Z' && *p < 'a') ||
					  (*p > 'z') )
				{
					p++;
				}
				if( '-' == *p )
				{
					SERVER_DEBUG( "bad host name\n" );	
					return -1;
				}
				strncpy( host, p, sizeof(host) );
				size_t len = strlen(host);
				SERVER_DEBUG( "len = %d\n", len )
				while( len > 1 && '\r' == host[len-1] || '\n' == host[len-1] )
				{
					host[len-1] = '\0';
					len = strlen(host);
				}
				break;
			}
		} else {
			SERVER_DEBUG( "lookup host error\n" );	
			return -1;
		}
		index += strlen(line)+1;//notice there is a '\n' in every line
		SERVER_DEBUG( "index = %d\n", index );
	}while( index<http_data_len );

	SERVER_DEBUG( "host = %s\n", host );
	struct hostent *ht = gethostbyname( host );
	if( NULL == ht )
	{
		SERVER_DEBUG( "gethostbyname error:%s\n", hstrerror(h_errno) );
		return -1;			
	} else {
		SERVER_DEBUG( "ht->h_name:%s\n", ht->h_name );
		SERVER_DEBUG( "ht->h_length:%d\n", ht->h_length );
		char **h_addr_list = ht->h_addr_list;
		while( NULL != h_addr_list )
		{
			strncpy( host_ip, inet_ntoa( *(struct in_addr *)*h_addr_list ), host_ip_len );
			SERVER_DEBUG( "host_ip:%s\n", host_ip );
			return 0;
		}
		return -1;
	}

	return 0;
}


int 
server( int listen_port, const char* host_ip, int host_port )
{
	struct sockaddr_in server_addr;
	
	memset( &server_addr, 0, sizeof(server_addr) );
	
	int sockfd = 0;
	
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "server socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "server socket success\n" );	

	int reuse = 1;
	if( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, (socklen_t)sizeof(int) ) < 0 )
	{
		SERVER_DEBUG( "server setsockopt error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "server setsockopt success\n" );	

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons( listen_port );

	if( bind( sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == - 1 )
	{
		SERVER_DEBUG( "bind error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "server bind success\n" );	

	if( listen( sockfd, 5 ) == -1 )
	{
		SERVER_DEBUG( "server listen error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}		
	SERVER_DEBUG( "server listen success port = %d\n", listen_port );	
	
	parent = getpid();
	int status;

	SERVER_DEBUG( "server bind success\n" );	
	while(1) 
	{
		int sockfd_new = 0;
		
		struct sockaddr_in client_addr;		
		memset( &client_addr, 0, sizeof(client_addr) );
		socklen_t clientsize = sizeof( client_addr );
	
		sockfd_new = accept( sockfd, (struct sockaddr *)&client_addr, &clientsize );

		if( sockfd_new < 0 && errno == EINTR )
		{
			SERVER_DEBUG( "catch signal:%s\n", strerror(errno) );
			continue;				
		} else if( sockfd_new < 0  ) {
			SERVER_DEBUG( "accept error:%s\n", strerror(errno) );
			return -1;			
		}			
		SERVER_DEBUG( "sockfd_new:%d\n", sockfd_new );

		if( fcntl( sockfd_new, F_SETFL, O_NONBLOCK) < 0 )
		{
			SERVER_DEBUG( "fcntl error:%s\n", strerror(errno) );
			close( sockfd_new );	
			close( sockfd );
			return -1;				
		}

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
			//pid_t pid = fork();
			pid_t pid = 0;
			if( pid < 0 )
			{
				SERVER_DEBUG( "fork error:%s\n", strerror(errno) );
				close( sockfd_new );	
				close( sockfd );
				return -1;
			} else if( pid == 0 ){
				#if defined(PROXY)
				if( process_proxy( sockfd_new, &client_addr, &clientsize, host_ip, host_port ) < 0 )
				{
					SERVER_DEBUG( "process_proxy error:%s\n", strerror(errno) );
					close( sockfd_new );	
					return -1;
				}		
				#elif defined(HTTP_PROXY)
				if( process_http_proxy( sockfd_new, &client_addr, &clientsize ) < 0 )
				{
					SERVER_DEBUG( "process_http_proxy error:%s\n", strerror(errno) );
					close( sockfd_new );	
					return -1;
				}		
				#elif defined(SERVER)
				if( process_server( sockfd_new, &client_addr, &clientsize ) < 0 )
				{
					SERVER_DEBUG( "process_server error:%s\n", strerror(errno) );
					close( sockfd_new );	
					return -1;
				}		
				#endif
			} else {
				return 0;
			}
		} else {
			SERVER_DEBUG( "waitpid\n" );
			close( sockfd_new );	
			//waitpid( child, &status, 0 );
			waitpid( -1, &status, WNOHANG );
		}
	}
	
	close( sockfd );
	
	return 0;	
}


int 
process_server( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen )
{
	if( getpeername( sockfd_new, (struct sockaddr *)addr, addrlen ) == -1 )
	{
		SERVER_DEBUG( "getpeername error:%s\n", strerror(errno) );
		return -1;			
	}

	SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(addr->sin_addr), addr->sin_port );

	char buffer[SO_BUF_LEN_MAX];
	size_t length = 0;
	fd_set readfd;
	struct timeval tv;

	while(1)
	{
		memset( buffer, 0, sizeof(buffer) );
		FD_ZERO(&readfd);
		FD_SET(sockfd_new ,&readfd);

		tv.tv_sec = 60;
		tv.tv_usec = 0;
		int timeout = (tv.tv_sec*1000000+tv.tv_usec)/1000000;
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv ) ) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			SERVER_DEBUG( "receive message:\n" );
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
			if( 1 )
			{
				SERVER_DEBUG( "send message:\n" );
				char message[] = "world\n";
				length = 0;
				if( ( length = write( sockfd_new, message, strlen(message) ) ) == -1 )
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
process_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen, const char* host_ip, int host_port )
{
	if( getpeername( sockfd_new, (struct sockaddr *)addr, addrlen ) == -1 )
	{
		SERVER_DEBUG( "getpeername error:%s\n", strerror(errno) );
		return -1;			
	}

	SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(addr->sin_addr), addr->sin_port );

	struct sockaddr_in client_addr;
	
	memset( &client_addr, 0, sizeof(client_addr) );
	
	int sockfd = 0;
	
	if( (sockfd=socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "client socket success\n" );	

	client_addr.sin_family = AF_INET;
	//client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_addr.sin_port = htons( host_port );
	
	inet_pton( AF_INET, host_ip, &client_addr.sin_addr );

	if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
	{
		SERVER_DEBUG( "client connect error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;			
	}
	SERVER_DEBUG( "connect success\n" );	

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

		tv.tv_sec = 60;
		tv.tv_usec = 0;
		int timeout = (tv.tv_sec*1000000+tv.tv_usec)/1000000;
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv ) ) < 0 )
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
main( int argc, char **argv )
{
	signal(SIGCHLD, SigUserProc);

	int port = 0;
	char *host_ip = NULL;
	int host_port = 0;
	char c = 0;

	#if defined(PROXY)
	if( argc < 7 )
	#elif defined(HTTP_PROXY)
	if( argc < 2 )
	#endif
	{
		#if defined(PROXY)
		SERVER_DEBUG( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
		#elif defined(HTTP_PROXY)
		SERVER_DEBUG( "usage:./server -l 12306 \n" );
		#endif
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
				SERVER_DEBUG( "usage:./server -l 12306 -s 127.0.0.1 -p 12307\n" );
				return -1;
		}
	}

	SERVER_DEBUG( "listen_port:%d\nhost_ip:%s\nhost_port:%d\n", port, host_ip, host_port );

	server( port, host_ip, host_port );
	
	SERVER_DEBUG( "server close\n" );

	return 0;
}


