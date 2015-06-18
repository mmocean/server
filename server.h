/*************************************************************************
	> File Name: server.h
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Thu 19 Jun 2014 09:48:44 PM CST
 ************************************************************************/

#ifndef _SERVER_H
#define _SERVER_H

typedef void (*sighandler_t)(int);    

sighandler_t 
signal_intr( int no, sighandler_t func );

void 
SigUserProc( int no );

int 
process_server( int sockfd_new );

int 
process_proxy( int sockfd_new, const char* host_ip, int host_port );

int 
server( int listen_port, const char* host_ip, int host_port );

#endif

