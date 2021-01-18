#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>

#define JR3_MAX_DEVICES 			20

#define ARG_QUIET					0x01
#define ARG_DEBUG					0x02

typedef struct{
	unsigned int period_us;
	unsigned int arguments;
	unsigned int jr3_devices;
	char * fifo_file;
} task_t;

typedef struct{
	unsigned long timestamp;
	int sensors_number;
	float force[6];
	float torque[6];
} jr3_force_torque_t;


extern pthread_mutex_t jr3_data_mutex;

void *jr3_thread(void *data);

void * writer_thread(void * data);

#endif

