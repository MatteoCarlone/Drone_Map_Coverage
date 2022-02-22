#!/bin/bash

FOLDER="$(find -iname src -type d)"
if [ -z "${FOLDER}" ]
then
    echo "Source is not unzipped yet."
else
	read lower_port upper_port < /proc/sys/net/ipv4/ip_local_port_range
	while :
	do
		PORT="`shuf -i $lower_port-$upper_port -n 1`"
		ss -lpn | grep -q ":$PORT " || break
	done

	echo "Port number $PORT selected" 

	if [[ -d logfile ]] 
	then 
		echo "/logfile already exists on your filesystem." 
		rm -r logfile
	fi
	mkdir logfile

	konsole -p 'TerminalColumns=44' -p 'TerminalRows=84' -e "./src/dpm403_master/master $PORT" & echo "PID shell of master        : $!"

	sleep 1

	konsole -e "./src/FE007_3_drone/drone $PORT" & echo "PID shell of drone FE007   : $!"
	konsole -e "./src/al9_3_drone/drone $PORT" & echo "PID shell of drone al9     : $!"
	konsole -e "./src/ML99_3_drone/drone $PORT" & echo "PID shell of drone ML99    : $!"
	konsole -e "./src/FA00_3_drone/drone $PORT" & echo "PID shell of drone ale_fab : $!"
fi

