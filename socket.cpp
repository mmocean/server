/*************************************************************************
	> File Name: socket.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Sun 14 Jun 2015 08:32:18 PM CST
 ************************************************************************/

#include"socket.h"
#include"log.h"

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
data_exchange( int sockfd_dest, int sockfd_src, char* buf, size_t count )
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


int initial_socket( void )
{
	int sockfd = 0;
	if( (sockfd = socket(AF_INET,SOCK_STREAM,0) ) == -1 )
	{
		SERVER_DEBUG( "socket error:%s\n", strerror(errno) );
		return -1;
	}
	SERVER_DEBUG( "client socket successfully\n" );	

	if( setsockopt_wrapper( sockfd ) < 0 )
	{
		SERVER_DEBUG( "setsockopt_wrapper error:%s\n", strerror(errno) );
		close( sockfd );
		return -1;
	}
	return sockfd;
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
setsockopt_nonblock( int sockfd )
{						
	int flag = 0;
	if( flag = fcntl( sockfd, F_GETFL,0 ) != -1 )
	{
		if( fcntl( sockfd, F_SETFL, flag|O_NONBLOCK ) < 0 )
		{
			SERVER_DEBUG( "fcntl error:%s\n", strerror(errno) );
			return -1;				
		}
		SERVER_DEBUG( "fcntl O_NONBLOCK successfully\n" );	
	}
	return 0;
}



int 
parse_http_data( const char* http_data, int http_data_len, char *host_ip, int host_ip_len )
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


