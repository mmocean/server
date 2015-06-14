/*************************************************************************
	> File Name: server.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:48:40 PM CST
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<assert.h>
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

#define HTTP_PROXY
//#define SERVER

#define SIG_USER 14

#define HOST_IP_LEN_MAX 16
#define HOST_PORT_DEFAULT 80
#define HOST_NAME_LEN_MAX 128
#define HTTP_LINE_LEN_MAX 256

#define TIME_OUT_DEFAULT 60
#define TIME_STRING_LEN_MAX 32

#define BACK_LOG_DEFAULT 5

#define SO_BUF_LEN_MAX 32768
#define SO_SEND_BUF_MAX 8192
#define SO_RECV_BUF_MAX 8192


#define SERVER_DEBUG(...)\
printf("[DEBUG] [%s] [%d] [%s:%d] ", get_time_string(), getpid(), __FILE__, __LINE__ );\
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

ssize_t
read_wrapper( int fd, char *buf, size_t count );

ssize_t
write_wrapper( int fd, const char *buf, size_t count );

int 
dada_exchange( int sockfd_dest, int sockfd_src, char* buf, size_t count );

int 
setsockopt_wrapper( int sockfd );

int 
server( int listen_port, const char* host_ip, int host_port );

int 
process_server( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen );

int 
process_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen, const char* host_ip, int host_port );

const char* get_time_string( void );




void 
SigUserProc(int no)
{
	int status;
	SERVER_DEBUG( "SigUserProc kill\n" );
	waitpid( 0, &status, WNOHANG );
}


int 
process_http_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen )
{
	assert( NULL != addr );
	assert( NULL != addrlen );

	if( getpeername( sockfd_new, (struct sockaddr *)addr, addrlen ) == -1 )
	{
		SERVER_DEBUG( "getpeername error:%s\n", strerror(errno) );
		return -1;			
	}

	SERVER_DEBUG( "socket established, peer addr = %s:%u\n", inet_ntoa(addr->sin_addr), addr->sin_port );

	while(1)
	{
		fd_set readfd;
		FD_ZERO(&readfd);
		FD_SET(sockfd_new, &readfd);

		struct timeval tv;
		tv.tv_sec = TIME_OUT_DEFAULT;
		tv.tv_usec = 0;
		int nRet = 0;
		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv) ) < 0 )
		{
			if( EINTR == errno )
			{
				continue;
			}
			SERVER_DEBUG( "select error:%s\n", strerror(errno) );
			return -1;			
		} else if( FD_ISSET(sockfd_new, &readfd) ) {
			char buffer[SO_BUF_LEN_MAX];
			SERVER_DEBUG( "receive downstream message:\n" );

			ssize_t length = 0;
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
				if( parse_http_data( buffer, length, host_ip, sizeof(host_ip) ) < 0 )
				{
					SERVER_DEBUG( "parse_http_data error\n" );
					return -1;			
				} else {
					struct sockaddr_in client_addr;
					memset( &client_addr, 0, sizeof(client_addr) );
					int sockfd = 0;
					if( (sockfd = socket(AF_INET,SOCK_STREAM,0) ) == -1 )
					{
						SERVER_DEBUG( "socket error:%s\n", strerror(errno) );
						return -1;
					}
					SERVER_DEBUG( "client socket successfully\n" );	

					const int host_port = HOST_PORT_DEFAULT;
					client_addr.sin_family = AF_INET;
					client_addr.sin_port = htons(host_port);

					if( setsockopt_wrapper( sockfd ) < 0 )
					{
						SERVER_DEBUG( "setsockopt_wrapper error:%s\n", strerror(errno) );
						close( sockfd );
						return -1;
					}

					inet_pton( AF_INET, host_ip, &client_addr.sin_addr );
					if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
					{
						SERVER_DEBUG( "client connect %s:%d error:%s\n", host_ip, host_port, strerror(errno) );
						close( sockfd );
						return -1;			
					} else {
						SERVER_DEBUG( "connect successfully %s:%d \n", host_ip, host_port );
						
						int flag = 0;
						if( flag = fcntl( sockfd_new, F_GETFL,0 ) != -1 )
						{
							if( fcntl( sockfd_new, F_SETFL, flag|O_NONBLOCK ) < 0 )
							{
								SERVER_DEBUG( "fcntl error:%s\n", strerror(errno) );
								return -1;				
							}
							SERVER_DEBUG( "fcntl O_NONBLOCK successfully\n" );	
						}
						
						SERVER_DEBUG( "send message to upsteam:\n" );
						if( ( length = write_wrapper( sockfd, buffer, length ) ) == -1 )
						{
							SERVER_DEBUG( "write error:%s\n", strerror(errno) );
							close( sockfd );		
							return -1;			
						}
						SERVER_DEBUG( "write size = %d\n", length );		   

						while(1)
						{
							int count = 0;

							FD_ZERO(&readfd);
							FD_SET(sockfd_new, &readfd);
							FD_SET(sockfd ,&readfd);
							
							tv.tv_sec = TIME_OUT_DEFAULT;
							tv.tv_usec = 0;

							if( ( nRet = select(sockfd+1, &readfd, NULL, NULL, &tv) ) < 0 )
							{
								if( EINTR == errno )
								{
									continue;
								}
								SERVER_DEBUG( "select error:%s\n", strerror(errno) );
								return -1;			
							} else if( FD_ISSET(sockfd, &readfd) ) {
								SERVER_DEBUG( "receive upstream message:\n" );							
								if( dada_exchange( sockfd_new, sockfd, buffer, sizeof(buffer) ) < 0)
								{
									SERVER_DEBUG( "dada_exchange error\n" );
									close( sockfd );
									return -1;			
								}
							} else if( FD_ISSET(sockfd_new, &readfd) ) {
								SERVER_DEBUG( "receive downstream message:\n" );
								if( dada_exchange( sockfd, sockfd_new, buffer, sizeof(buffer) ) < 0)
								{
									SERVER_DEBUG( "dada_exchange error\n" );
									close( sockfd );
									return -1;			
								}
							} else {						
								SERVER_DEBUG( "receive upstream message timeout\n" );	
								continue;
							} //select timeout
						} //while(1)
					} //connect
				} //parse_http_data
			} else { //read_wrapper <= 0 
				SERVER_DEBUG( "receive downstream message error, client socket closed\n" );	
				return -1;
			}
		} else { //select timeout
			SERVER_DEBUG( "receive downstream message timeout\n" );	
		}
	}

	kill( parent, SIGCHLD );
	return 0;
}


ssize_t
read_wrapper( int fd, char *buf, size_t count )
{
	assert( NULL != buf );
	memset( buf, 0, count );

	ssize_t c = 0;
	while(1)
	{
		ssize_t length = read( fd, buf+c, count-c );
		if( 0 > length )
		{
			SERVER_DEBUG( "read: errno:%d error:%s \n", errno, strerror(errno) );
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


ssize_t
write_wrapper( int fd, const char *buf, size_t count )
{
	assert( NULL != buf );

	ssize_t c = 0;
	while(1)
	{
		ssize_t length = write( fd, buf+c, count-c );
		if( 0 > length )
		{
			SERVER_DEBUG( "write: errno:%d error:%s\n", errno, strerror(errno) );
			if( EINTR == errno || EAGAIN == errno )
			{
				continue;
			}
			return length;
		} else {
			c += length;
			if( c == count )
			{
				break;
			}
		}
	}
	return c;
}


int 
dada_exchange( int sockfd_dest, int sockfd_src, char* buf, size_t count )
{
	int total = 0;
	while(1)
	{
		ssize_t length = 0;
		SERVER_DEBUG( "read message from src:\n" );
		if( ( length = read_wrapper( sockfd_src, buf, count ) ) < 0 )
		{
			SERVER_DEBUG( "read error:%s\n", strerror(errno) );
			return -1;			
		}
		SERVER_DEBUG( "read size = %d \n", length );
		total += length;
		if( 0 == total && 0 == length )
		{
			SERVER_DEBUG( "receive src message error, close socket\n" );	
			return -1;			
		} 
		if( length > 0 )
		{
			SERVER_DEBUG( "send message to dest:\n" );
			if( ( length = write_wrapper( sockfd_dest, buf, length ) ) == -1 )
			{
				SERVER_DEBUG( "write error:%s\n", strerror(errno) );
				return -1;			
			}
			SERVER_DEBUG( "write size = %d\n", length );		   
		} else {
			break;
		}
	}
	return 0;
}

int 
setsockopt_wrapper( int sockfd )
{
	int reuse = 1;
	if( setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, (socklen_t)sizeof(int) ) < 0 )
	{
		SERVER_DEBUG( "setsockopt SO_REUSEADDR error:%s\n", strerror(errno) );
		return -1;			
	}
	SERVER_DEBUG( "setsockopt SO_REUSEADDR successfully\n" );	

	int sndbuf = SO_SEND_BUF_MAX;
	if( setsockopt( sockfd, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbuf, (socklen_t)sizeof(int) ) < 0 )
	{
		SERVER_DEBUG( "setsockopt SO_SNDBUF error:%s\n", strerror(errno) );
		return -1;			
	}
	SERVER_DEBUG( "setsockopt SO_SNDBUF successfully\n" );	

	int rcvbuf = SO_RECV_BUF_MAX;
	if( setsockopt( sockfd, SOL_SOCKET, SO_RCVBUF, (const void *)&rcvbuf, (socklen_t)sizeof(int) ) < 0 )
	{
		SERVER_DEBUG( "setsockopt SO_RCVBUF error:%s\n", strerror(errno) );
		return -1;			
	}
	SERVER_DEBUG( "setsockopt SO_RCVBUF successfully\n" );	

	return 0;
}


int 
parse_http_data( const char* http_data, size_t http_data_len, char *host_ip, size_t host_ip_len )
{
	assert( NULL != http_data );
	assert( NULL != host_ip );
	memset( host_ip, 0, host_ip_len );

	SERVER_DEBUG( "#########dump http###########\n" );
	SERVER_DEBUG( "\n%s", http_data );
	SERVER_DEBUG( "#########dump http###########\n" );

	if( 0 == strncasecmp( "CONNECT", http_data, strlen("CONNECT") ) )
	{
		SERVER_DEBUG( "do not support https right now, refuse\n" );	
		return -1;			
	}

	//parse host value
	char host[HOST_NAME_LEN_MAX] = {0};
	memset( host, 0, sizeof(host) );
	int index = 0;
	do {
		char line[HTTP_LINE_LEN_MAX] = {0};
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
				SERVER_DEBUG( "len = %d\n", len );
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

	if( 0 == strlen(host) )
	{
		SERVER_DEBUG( "lookup host error\n" );	
		return -1;
	}
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


const char* get_time_string( void )
{
	static char strtime[TIME_STRING_LEN_MAX] = {0};
	memset( strtime, 0, sizeof(strtime) );
	
	time_t tt = time( NULL );
	const char* p = asctime( localtime(&tt) );
	
	strncpy( strtime, p, strlen(p)-1 );
	
	return strtime;
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
			return -1;			
		}

		int flag = 0;
		if( flag = fcntl( sockfd_new, F_GETFL,0 ) != -1 )
		{
			if( fcntl( sockfd_new, F_SETFL, flag|O_NONBLOCK ) < 0 )
			{
				SERVER_DEBUG( "fcntl error:%s\n", strerror(errno) );
				return -1;				
			}
			SERVER_DEBUG( "fcntl O_NONBLOCK successfully\n" );	
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
				SERVER_DEBUG( "process_http_proxy error\n" );
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
			int status;
			SERVER_DEBUG( "waitpid\n" );
			close( sockfd_new );	
			waitpid( -1, &status, WNOHANG );
		}
	}
	
	close( sockfd );
	SERVER_DEBUG( "server close\n" );
	
	return 0;	
}


int 
process_server( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen )
{
	assert( NULL != addr );
	assert( NULL != addrlen );
	
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

		tv.tv_sec = TIME_OUT_DEFAULT;
		tv.tv_usec = 0;
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv) ) < 0 )
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
process_proxy( int sockfd_new, struct sockaddr_in *addr, socklen_t *addrlen, const char* host_ip, int host_port )
{
	assert( NULL != addr );
	assert( NULL != addrlen );
	assert( NULL != host_ip );
	
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
	SERVER_DEBUG( "client socket successfully\n" );	

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
	
		int nRet = 0;

		if( ( nRet = select(sockfd_new+1, &readfd, NULL, NULL, &tv) ) < 0 )
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


