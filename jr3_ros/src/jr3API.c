#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "jr3API.h"

jr3_t * jr3_init(const char * nDeviceName)
{
  jr3_t * jr3;

  jr3 = malloc(sizeof(jr3_t));

  jr3->DeviceName = malloc(strlen(nDeviceName) + 1);
  strcpy(jr3->DeviceName, nDeviceName);

  return jr3;
}

int jr3_deinit(jr3_t * jr3) {

  free(jr3->DeviceName);

  free(jr3);

  return 0;
}

int jr3_open(jr3_t * jr3) {
  int err = 0;

  jr3->device = open(jr3->DeviceName, 0);
	if (jr3->device < 0) {
		err = -jr3->device;
	}

  return err;
}

int jr3_close(jr3_t * jr3) {

  int err = 0;

  err = close(jr3->device);

  return err;
}

int jr3_ioctl(jr3_t * jr3, unsigned long int request, void * data)
{
	int err = 0;

	err = ioctl(jr3->device, request, data);

	return err;
}

int jr3_get_type( jr3_t * jr3){
	int ret = 0;

	ret = ioctl(jr3->device, IOCTL0_JR3_TYPE, NULL);
	jr3->sensors_number = ret;

	return ret;
}






























