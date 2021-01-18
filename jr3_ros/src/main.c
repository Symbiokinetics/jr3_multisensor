#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "jr3API.h"
#include "threads.h"

int main(int argc, char* argv[])
{
    struct sched_param param;
    pthread_attr_t attr;
    pthread_t thread;
    int ret;
	
	task_t jr3_thread_data;
	task_t writer_thread_data;
	
	int opt;
	unsigned int arguments = 0;
	unsigned int arguments_devices = 2;
	unsigned int arguments_period_jr3 = 1000000;
	unsigned int arguments_period_writer = 1000000;
	char fifo_file[256] = "./jr3_fifo";
	int priority = 80;
	
	while((opt = getopt(argc, argv, "hqd:j:w:f:p:g")) != -1){
		switch(opt){
			case 'h':
				printf("There are two tasks:\n"
				"jr3 task that reads peridocially from jr3 devices\n"
				"writer task that sends data to FIFO\n"
				"\n"
				"%s [-h] [-q] [-d jr3_devices] [-j jr3_period_us] [-w writer_period_us] [-f fifo_file] [-p priority] [-g]\n"
				"\n"
				"-h this help\n"
				"-d N number of devices to read from\n"
				"-j jr3 task period in us\n"
				"-w writer task period in us\n"
				"-f path to FIFO file\n"
				"-p priority (the higher the more real-time it is)\n"
				"-g debug prints\n", argv[0]);
				exit(0);
				break;
			case 'q':
				arguments |= ARG_QUIET;
				break;
			case 'd':
				arguments_devices = atoi(optarg);
				break;
			case 'j':
				arguments_period_jr3 = atol(optarg);
				break;
			case 'w':
				arguments_period_writer = atol(optarg);
				break;
			case 'f':
				strncpy(fifo_file, optarg, 255);
				break;
			case 'p':
				priority = atoi(optarg);
				break;
			case 'g':
				arguments |= ARG_DEBUG;
				break;
		}
	}

	jr3_thread_data.arguments = arguments;
	jr3_thread_data.period_us = arguments_period_jr3;
	jr3_thread_data.jr3_devices = arguments_devices;
	jr3_thread_data.fifo_file = fifo_file;

	writer_thread_data.arguments = arguments;
	writer_thread_data.period_us = arguments_period_writer;
	writer_thread_data.jr3_devices = arguments_devices;
	writer_thread_data.fifo_file = fifo_file;



	pthread_mutex_init( &jr3_data_mutex, NULL);
    /* Lock memory */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
        printf("mlockall failed:\n");
        exit(-2);
    }

    /* Initialize pthread attributes (default values) */
    ret = pthread_attr_init(&attr);
    if (ret) {
        printf("init pthread attributes failed\n");
        goto out;
    }

    /* Set a specific stack size  */
    ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    if (ret) {
        printf("pthread setstacksize failed\n");
        goto out;
    }

    /* Set scheduler policy and priority of pthread */
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret) {
        printf("pthread setschedpolicy failed\n");
        goto out;
    }
    param.sched_priority = priority;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret) {
        printf("pthread setschedparam failed\n");
        goto out;
    }
    /* Use scheduling parameters of attr */
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        printf("pthread setinheritsched failed\n");
        goto out;
    }

	pthread_mutex_init(&jr3_data_mutex, NULL);

    /* Create a pthread with specified attributes */
    ret = pthread_create(&thread, &attr, jr3_thread, &jr3_thread_data);
    if (ret) {
        printf("create jr3 pthread failed\n");
        goto out;
    }

    ret = pthread_create(&thread, &attr, writer_thread, &writer_thread_data);
    if (ret) {
        printf("create writer pthread failed\n");
        goto out;
    }

    /* Join the thread and wait until it is done */
    ret = pthread_join(thread, NULL);
    if (ret){
    	printf("join pthread failed:\n");
	}
 
out:
        return ret;
}
