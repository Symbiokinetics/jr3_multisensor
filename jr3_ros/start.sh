#!/bin/bash

help(){
echo $0 [-j jr3_task_period] [-w writer_task_period] [-p priority] [-q] [-g] [-h]
}

j=1000000
w=900000
p=90
q=""
g=""

while getopts "j:w:p:qgh" o; do
	case "${o}" in
		j)
			j=${OPTARG}
			;;
		w)
			w=${OPTARG}
			;;
		p)
			p=${OPTARG}
			;;
		q)
			q="-q"
			;;
		g)
			g="-g"
			;;
		h)
			help
			exit 0
	esac
done

echo jr3 task period = $j
echo writer task period = $w
echo priority = $p
echo quiet = "$q"
echo debug = "$g"
echo 
echo ...........
echo ...........
echo ...........
echo 


devices=`dmesg | tail -n 200 | grep "registered, current number of devices:" | tail -n 1 | cut -d ':' -f 2`
if [[ $devices -gt 0 ]]; then
	echo Discovered $devices devices
else
	echo Could not discover devices automatically
	exit -1
fi

echo Sourcing ros

source /opt/ros/melodic/setup.bash

./collector -d $devices -j $j -w $w -p $p $q $g &
sleep 1
PID=`pgrep collector`

echo collector PID = $PID

echo Starting python ROS publisher
./publisher.py &

PIDPub=`pgrep -f publisher.py`


echo publisher.py PID = $PIDPub

