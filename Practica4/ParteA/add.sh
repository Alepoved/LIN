
#!/bin/bash


	for (( i=0 ; $i<10; i++ )); do
		echo add $i > /proc/modlist
		echo printing $i...
		
		sleep 1
	done
	

echo "finished script 1"
exit 0

