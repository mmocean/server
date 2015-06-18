/*************************************************************************
	> File Name: multipthread.h
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Sun 14 Jun 2015 08:33:37 PM CST
 ************************************************************************/

#ifndef _MULTIPTHREAD_H
#define _MULTIPTHREAD_H

#include<pthread.h>

struct pthread_vargs
{
	//in - parameter
	int sockfd;
	int sockfd_rem; //remote
	//out - result
	int ret_code;
	pthread_t pid;
	long read_from_downstream;	
	long write_to_downstream;	
	long read_from_upstream;	
	long write_to_upstream;	
	time_t start_time;
	time_t stop_time;
	struct pthread_vargs *next;
};

void* 
pthread_http_proxy( void *args );

void 
pthread_exit_wrapper( int ret_code, struct pthread_vargs *pv );

int
pthread_join_wrapper( const struct pthread_vargs *pv );

void 
pthread_vargs_display( const struct pthread_vargs *pv );

void
pthread_vargs_cleanup( const struct pthread_vargs *pv );

#endif

