/*
 * 11-4.cpp
 *
 *  Created on: 2015年7月30日
 *      Author: duoyi
 */
#include <time.h>

#define TIME_OUT 5000

void CallAtTime ( )
{
	int timeout = TIME_OUT;

	time_t start = time ( NULL );
	time_t end = time ( NULL );

	while ( 1 )
	{
		printf ( " the timeout is now %d mil-seconds\n" ,
				timeout );
		start = time ( NULL );
		int number = epoll_wait ( epollfd , events ,
				MAX_EVENT_NUMBER , timeout );
		if ( ( number < 0 ) && ( errno != EINTR ) )
		{
			printf ( "epoll failure\n" );
			break;
		}
		if ( number == 0 )
		{
			timeout = TIME_OUT;
			continue;
		}

		end = time ( NULL );

		timeout -= ( end - start ) * 1000;

		if ( timeout <= 0 )
		{
			timeout = TIME_OUT;
		}
	}
}

