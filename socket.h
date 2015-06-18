/*************************************************************************
	> File Name: socket.h
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Sun 14 Jun 2015 08:32:10 PM CST
 ************************************************************************/

#ifndef _SOCKET_H
#define _SOCKET_H

int 
initial_socket( void );

int 
setsockopt_nonblock( int sockfd );

int 
setsockopt_wrapper( int sockfd );

ssize_t
read_wrapper( int fd, char *buf, size_t count );

ssize_t
write_wrapper( int fd, const char *buf, size_t count );

int 
data_exchange( int sockfd_dest, int sockfd_src, char* buf, size_t count );

int 
parse_http_data( const char* http_data, int http_data_len, char *host_ip, int host_ip_len );

#endif

