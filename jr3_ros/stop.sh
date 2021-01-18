#!/bin/bash

collector=`pgrep collector | wc -l`
if [[ $collector -gt 0 ]]; then
	echo Killing collector
	kill `pgrep collector`
	
	collector=`pgrep collector | wc -l`
	if [[ $collector -eq 0 ]]; then
		echo collector killed successfullly
	else
		echo Problem killing collector, try it manually
	fi
	
else
	echo No collector
fi

publisher=`pgrep -f publisher.py | wc -l`
if [[ $publisher -gt 0 ]]; then
	echo Killing publisher.py
	kill -9 `pgrep -f publisher.py`

	publisher=`pgrep -f publisher.py | wc -l`
	if [[ $publisher -eq 0 ]]; then
		echo publisher.py killed successfullly
	else
		echo Problem killing publisher.py, try it manually
	fi
else
	echo No publisher.py
fi

