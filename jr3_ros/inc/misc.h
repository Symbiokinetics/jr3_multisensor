#ifndef MISC_H
#define MISC_H

#include <unistd.h>

typedef struct{
	unsigned long time;
	unsigned long last_time;
	unsigned long delay_us;
} time_delay_t;

unsigned long get_time( void);

void delay_until_init( time_delay_t * time_delay, unsigned long delay_us);

unsigned long delay_until( time_delay_t * time_delay);

#endif
