/***************************************************************************
 *   Copyright (C) 2015 by Wojciech Domski                                 *
 *   Wojciech.Domski@gmail.com                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "jr3API.h"

jr3_t * jr3;

jr3_force_array scale, scale2;
jr3_six_axis_array force, force2;

int main(int argc, char **argv)
{
	int err = 0;
	int i = 0, c = 0;
	float tmp[6], tmp2[6];
	int samples = 10;
	int sensors_number = 0;

	if( argc > 1){
		printf("Openning %s\n", argv[1]);
		jr3 = jr3_init(argv[1]);
	}
	else
		jr3 = jr3_init("jr30");

	if( argc > 2){
		samples = atoi(argv[2]);
	}

	err = jr3_open(jr3);

	if(err < 0)
	{
		printf("Couldn't open jr3 device\n");
		return -1;
	}


	sensors_number = jr3_ioctl(jr3, IOCTL0_JR3_TYPE, NULL);
	if(sensors_number < 0){
		printf("Can't read board type\r\n");
	}else{
		printf("Board has %d sensors\r\n", sensors_number);
	}

	err = jr3_ioctl(jr3, IOCTL0_JR3_GET_FULL_SCALES, (void *)&scale);
	if( err < 0){
		printf("Problem with obtaining scale from jr3\n");
		return -1;
	}
	if( sensors_number == 2){
		err = jr3_ioctl(jr3, IOCTL1_JR3_GET_FULL_SCALES, (void *)&scale2);
		if( err < 0){
			printf("Problem with obtaining scale from jr3 on 2nd sensor (%d)\r\n", err);
			return -1;
		}
	}

	do
	{
		err = jr3_ioctl(jr3, IOCTL0_JR3_FILTER0, (void *)&force);

		if( sensors_number == 2){
			err = jr3_ioctl(jr3, IOCTL1_JR3_FILTER0, (void *)&force2);
		}
		for( i = 0; i < 3; ++i)
		{
			tmp[i] = (float)force.f[i]*(float)scale.f[i]/16384.0;
			tmp[i+3] = (float)force.m[i]*(float)scale.m[i]/16384.0;

			if( sensors_number == 2){
				tmp2[i] = (float)force2.f[i]*(float)scale2.f[i]/16384.0;
				tmp2[i+3] = (float)force2.m[i]*(float)scale2.m[i]/16384.0;
			}
		}

		for( i = 0; i < 6; ++i){
			printf("%5.4f  ", tmp[i]);
		}
		if( sensors_number == 2){
			printf(" () ");
			for( i = 0; i < 6; ++i){
				printf("%5.4f  ", tmp2[i]);
			}	
		}
		printf("\n");

		sleep(1);
		++c;
	}while(c < samples);


	jr3_close(jr3);
	jr3_deinit(jr3);
	return 0;
}




























