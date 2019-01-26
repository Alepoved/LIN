
#!/bin/bash


	for (( i=1 ; $i<6; i++ )); do
		echo remove $i > /proc/modlist
		echo removing... $i
		sleep 1
	done
	

sleep 4

cat /proc/modlist
echo "finished script 2"
exit 0

