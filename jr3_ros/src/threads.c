#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "threads.h"
#include "jr3API.h"
#include "misc.h"

pthread_mutex_t jr3_data_mutex;

unsigned long frame_number;
jr3_force_torque_t jr3_force_torque[JR3_MAX_DEVICES];

unsigned long jr3_time[JR3_MAX_DEVICES];

jr3_t * jr3[JR3_MAX_DEVICES];
jr3_force_array jr3_scale[JR3_MAX_DEVICES], jr3_scale2[JR3_MAX_DEVICES];
jr3_six_axis_array jr3_force[JR3_MAX_DEVICES], jr3_force2[JR3_MAX_DEVICES];
jr3_force_torque_t jr3_force_torque_local[JR3_MAX_DEVICES];


jr3_force_torque_t writer_jr3_force_torque_local[JR3_MAX_DEVICES];

void *jr3_thread(void *data)
{
	time_delay_t time_delay;

	int err;
	int last_dev;

	task_t * task;

	char device[32];

	if(data == NULL){
		return NULL;
	}

	task = (task_t *)data;

	if( task->arguments & ARG_DEBUG){
		printf("Preparing jr3s\r\n");
	}
	last_dev = 0;
	for( int i = 0; i < task->jr3_devices; ++i){
		sprintf(device, "/dev/jr3%d", i);
		jr3[i] = jr3_init(device);
		err = jr3_open(jr3[i]);
		if(err){
			printf("Error while opening jr3 device\r\n");
			last_dev = i;
			break;
		}
	}

	if(err){
		//close and deinit
		for(int i =0; i < last_dev; ++i){
			jr3_close(jr3[i]);
			jr3_deinit(jr3[i]);
		}
		jr3_deinit(jr3[last_dev]);

		return NULL;
	}

	if( task->arguments & ARG_DEBUG){
		printf("Get board type from jr3\r\n");
	}
	for(int i =0; i < task->jr3_devices; ++i){
		err = jr3_get_type(jr3[i]);
		if(err < 0){
			printf("Problem getting board type jr3%d\n", i);
			break;
		}

		//initialize data structure
		jr3_force_torque_local[i].sensors_number = jr3[i]->sensors_number;
	}

	if( task->arguments & ARG_DEBUG){
		printf("Get scalles from jr3\r\n");
	}
	for(int i =0; i < task->jr3_devices; ++i){
		err = jr3_ioctl(jr3[i], IOCTL0_JR3_GET_FULL_SCALES, (void *)&jr3_scale[i]);

		if(err){
			printf("Problem getting scalles from jr3%d (0)\n", i);
			break;
		}
		
		if(jr3[i]->sensors_number == JR3_IOCTL_DOUBLE_BOARD){
			err = jr3_ioctl(jr3[i], IOCTL1_JR3_GET_FULL_SCALES, (void *)&jr3_scale2[i]);
		}
	
		if(err){
			printf("Problem getting scalles from jr3%d (1)\n", i);
			break;
		}
	}
	if(err){
		for(int i = 0; i < task->jr3_devices; ++i){
			jr3_close(jr3[i]);
			jr3_deinit(jr3[i]);
		}

		return NULL;
	}

	printf("Starting ...\n");
	delay_until_init(&time_delay, task->period_us);
	for(;;){
		
		for(int i = 0; i < task->jr3_devices; ++i){
			jr3_time[i] = get_time();
			err = jr3_ioctl(jr3[i], IOCTL0_JR3_FILTER0, (void *)&jr3_force[i]);
			if(jr3[i]->sensors_number == JR3_IOCTL_DOUBLE_BOARD){
				err = jr3_ioctl(jr3[i], IOCTL1_JR3_FILTER0, (void *)&jr3_force2[i]);
			}
		}

		
		for(int i = 0; i < task->jr3_devices; ++i){
			jr3_force_torque_local[i].timestamp = jr3_time[i];
			for(int j = 0; j < 3; ++j){
				jr3_force_torque_local[i].force[j] = (float)jr3_force[i].f[j]*(float)jr3_scale[i].f[j]/16384.0;
				jr3_force_torque_local[i].torque[j] = (float)jr3_force[i].m[j]*(float)jr3_scale[i].m[j]/16384.0;
				if(jr3[i]->sensors_number == JR3_IOCTL_DOUBLE_BOARD){
					jr3_force_torque_local[i].force[j] = (float)jr3_force[i].f[j]*(float)jr3_scale[i].f[j]/16384.0;
					jr3_force_torque_local[i].torque[j] = (float)jr3_force[i].m[j]*(float)jr3_scale[i].m[j]/16384.0;
				}
			}
			if(jr3[i]->sensors_number == JR3_IOCTL_DOUBLE_BOARD){
				for(int j = 0; j < 3; ++j){
					jr3_force_torque_local[i].force[j+3] = (float)jr3_force2[i].f[j]*(float)jr3_scale2[i].f[j]/16384.0;
					jr3_force_torque_local[i].torque[j+3] = (float)jr3_force2[i].m[j]*(float)jr3_scale2[i].m[j]/16384.0;

				}
			}
		}

		pthread_mutex_lock(&jr3_data_mutex);
		memcpy(jr3_force_torque, jr3_force_torque_local, sizeof(jr3_force_torque_t) * task->jr3_devices);
		++frame_number;
		pthread_mutex_unlock(&jr3_data_mutex);

		unsigned long ttt = delay_until(&time_delay);
		if( task->arguments & ARG_DEBUG){
			printf("jr3 delay: %ld\n", ttt);
		}
	}

    return NULL;
}

void * writer_thread(void * data){

	time_delay_t time_delay;

	task_t * task;
	unsigned long frame, last_frame;
	char buffer[256];
	int len;

	if(data == NULL){
		return NULL;
	}

	task = (task_t *)data;

	mknod(task->fifo_file, S_IFIFO|0666, 0);
	FILE * f = fopen( task->fifo_file, "a");
	if(f==NULL){
		printf("creating FIFO failed\n");
		return NULL;
	}


	delay_until_init(&time_delay, task->period_us);
	for(;;)
	{

		last_frame = frame;
		pthread_mutex_lock(&jr3_data_mutex);
		memcpy(writer_jr3_force_torque_local, jr3_force_torque, sizeof(jr3_force_torque_t) * task->jr3_devices);
		frame = frame_number;
		pthread_mutex_unlock(&jr3_data_mutex);

		if(!(task->arguments & ARG_QUIET)){
			for(int i = 0; i < task->jr3_devices; ++i){
				printf("jr3%d (%06ld) (%ld): ", i, frame, writer_jr3_force_torque_local[i].timestamp);
				for(int j = 0; j < 3; ++j){
					printf("%5.4f  ", writer_jr3_force_torque_local[i].force[j]);
				}
				for(int j = 0; j < 3; ++j){
					printf("%5.4f  ", writer_jr3_force_torque_local[i].torque[j]);
				}
				if(writer_jr3_force_torque_local[i].sensors_number == JR3_IOCTL_DOUBLE_BOARD){
					for(int j = 3; j < 6; ++j){                                   
					    printf("%5.4f  ", writer_jr3_force_torque_local[i].force[j]);    
					}                                                             
					for(int j = 3; j < 6; ++j){                                   
					    printf("%5.4f  ", writer_jr3_force_torque_local[i].torque[j]);   
					}                                                             
				}
				printf("\r\n");
			}
		}
	
		if(frame != last_frame){

			//new data
			if(f!=NULL){

				len = sprintf(buffer, "frame,%lX,%d\r\n", frame, task->jr3_devices);
				len = fwrite(buffer, len, 1, f);

				for(int i = 0; i < task->jr3_devices; ++i){
					len = sprintf(buffer, "jr3%d,%d,%lX,", i, writer_jr3_force_torque_local[i].sensors_number, 
						writer_jr3_force_torque_local[i].timestamp);
					len = fwrite(buffer, len, 1, f);
					for(int j = 0; j < 3; ++j){
						len = sprintf(buffer,"%X,", *(int*)&writer_jr3_force_torque_local[i].force[j]);
						len = fwrite(buffer, len, 1, f);
					}
					for(int j = 0; j < 3; ++j){
						len = sprintf(buffer,"%X,", *(int*)&writer_jr3_force_torque_local[i].torque[j]);
						len = fwrite(buffer, len, 1, f);
					}
					if(writer_jr3_force_torque_local[i].sensors_number == JR3_IOCTL_DOUBLE_BOARD){
						for(int j = 3; j < 6; ++j){                                                  
					    	len = sprintf(buffer,"%X,", *(int*)&writer_jr3_force_torque_local[i].force[j]); 
						    len = fwrite(buffer, len, 1, f);                                         
						}                                                                            
						for(int j = 3; j < 6; ++j){                                                  
						    len = sprintf(buffer,"%X,", *(int*)&writer_jr3_force_torque_local[i].torque[j]);
						    len = fwrite(buffer, len, 1, f);                                         
						}                                                                            
					}
					len = sprintf(buffer, "\r\n");
					len = fwrite(buffer, len, 1, f);
				}
	
				fflush(f);

			}
			last_frame = frame;
		}

		unsigned long ttt = delay_until(&time_delay);
		if( task->arguments & ARG_DEBUG){
			printf("writer delay: %ld\n", ttt);
		}

	}

	if( task->arguments & ARG_DEBUG){
		printf("writer: bye!\r\n");
	}
	return NULL;
}


