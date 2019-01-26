#include <sys/syscall.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <unistd.h>
#include <stdio.h>



#ifdef __i386__
#define __NR_led_ctl 383
#else
#define __NR_led_ctl 332
#endif

long led_control(unsigned int control){

return (long) syscall(__NR_led_ctl,control);

}

int main(int argc, char* argv[]){
 
	unsigned int control=0;
	

	if(argv[1]==NULL)printf("Usage: ./invoke_leds <ledmask>");
	
	else{	  

		sscanf(argv[1],"%x",&control);
	}
	
 	if(led_control(control)) printf("Error\n");
	
 

return 0;
}
