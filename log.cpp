/*************************************************************************
	> File Name: log.cpp
	> Author: lihy
	> Mail: lihaiyangcsu@gmail.com 
	> Created Time: Fri 20 Jun 2014 03:49:38 PM CST
 ************************************************************************/

#include"defs.h"  
#include"log.h"  

const char* 
get_time_string( void )
{
	static char strtime[TIME_STRING_LEN_MAX] = {0};
	memset( strtime, 0, sizeof(strtime) );
	
	time_t tt = time( NULL );
	const char* p = asctime( localtime(&tt) );
	
	strncpy( strtime, p, strlen(p)-1 );
	
	return strtime;
}

int 
logprintf()
{

	return 0;
}
