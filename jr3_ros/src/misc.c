#include <stdio.h>
#include <sys/time.h>

#include "misc.h"

unsigned long get_time( void){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return 1000000 * tv.tv_sec + tv.tv_usec;
}

void delay_until_init( time_delay_t * time_delay, unsigned long delay_us){
	time_delay->time = get_time();
	time_delay->last_time = get_time();
	time_delay->delay_us = delay_us;
}

unsigned long delay_until( time_delay_t * time_delay){
	unsigned long dt;

	time_delay->time = get_time();
	dt = (time_delay->time - time_delay->last_time);
	if(time_delay->delay_us > dt){
		usleep(time_delay->delay_us - dt);
	}
	time_delay->last_time = get_time();

	return dt;
}


