/*************************************************************************
	> File Name: multiprocess.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Sun 14 Jun 2015 08:33:19 PM CST
 ************************************************************************/

#include"defs.h"  
#include"multiprocess.h"
#include"log.h"
#include"socket.h"

extern pid_t parent;

int 
process_http_proxy( int sockfd_new )
{
	while(1)
	{
		fd_set readfd;
		FD_ZERO(&readfd);
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
					int sockfd = 0;
					if( ( sockfd = initial_socket() ) < 0 )
					{
						SERVER_DEBUG( "initial_socket error\n" );
						return -1;			
					}

					struct sockaddr_in client_addr;
					memset( &client_addr, 0, sizeof(client_addr) );					
					client_addr.sin_family = AF_INET;
					client_addr.sin_port = htons(HOST_PORT_DEFAULT);
					inet_pton( AF_INET, host_ip, &client_addr.sin_addr );

					if( connect( sockfd, (sockaddr*)&client_addr, sizeof(client_addr) ) < 0 )
					{
						SERVER_DEBUG( "client connect %s:%d error:%s\n", host_ip, HOST_PORT_DEFAULT, strerror(errno) );
						close( sockfd );
						return -1;			
					} else {
						SERVER_DEBUG( "connect successfully %s:%d \n", host_ip, HOST_PORT_DEFAULT );
						
						if( setsockopt_nonblock(sockfd) < 0 )
						{
							SERVER_DEBUG( "setsockopt_nonblock error\n" );
							close( sockfd );		
							return -1;			
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
							FD_ZERO(&readfd);
							FD_SET(sockfd_new, &readfd);
							FD_SET(sockfd ,&readfd);
							
							tv.tv_sec = TIME_OUT_DEFAULT;
							tv.tv_usec = 0;
							if( select(sockfd+1, &readfd, NULL, NULL, &tv) < 0 )
							{
								if( EINTR == errno )
								{
									continue;
								}
								SERVER_DEBUG( "select error:%s\n", strerror(errno) );
								return -1;			
							} else if( FD_ISSET(sockfd, &readfd) ) {
								SERVER_DEBUG( "receive upstream message:\n" );							
								if( data_exchange( sockfd_new, sockfd, buffer, sizeof(buffer) ) < 0)
								{
									SERVER_DEBUG( "data_exchange error\n" );
									close( sockfd );
									return -1;			
								}
							} else if( FD_ISSET(sockfd_new, &readfd) ) {
								SERVER_DEBUG( "receive downstream message:\n" );
								if( data_exchange( sockfd, sockfd_new, buffer, sizeof(buffer) ) < 0)
								{
									SERVER_DEBUG( "data_exchange error\n" );
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

