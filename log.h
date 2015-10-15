/*************************************************************************
	> File Name: log.h
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Fri 20 Jun 2014 03:50:23 PM CST
 ************************************************************************/

#ifndef _LOG_H
#define _LOG_H

const char* 
get_time_string( void );

#define SERVER_DEBUG(...)\
do {\
	printf("[DEBUG] [%s] [%d] [%s:%d] ", get_time_string(), getpid(), __FILE__, __LINE__ );\
	printf( __VA_ARGS__ );\
} while(0)

	
int 
log_printf( void );

#endif

