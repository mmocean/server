/*************************************************************************
	> File Name: defs.h
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Sun 14 Jun 2015 08:39:57 PM CST
 ************************************************************************/

#ifndef _DEFS_H
#define _DEFS_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<assert.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<getopt.h>
#include<netdb.h>


#ifdef HAVE_CONFIG_H
#include"config.h"
#endif


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

extern int errno;

extern int h_errno;

#endif

