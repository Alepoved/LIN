#include <linux/errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>


#define BLINKSTICK "/dev/usb/blinkstick0"
#define AZUL ":0x000011"
#define AZUL_2 ":0x011011"
#define ROJO ":0x110000"
#define AMARILLO ":0x111000"
#define VERDE ":0x001100"
#define BLANCO ":0x111011"
#define ROSA ":0x100111"
#define VACIO ":0x0"



unsigned char *sample_colors[]={VERDE,AZUL, BLANCO, AZUL_2, AMARILLO,VERDE ,ROSA,BLANCO};

int pintaLeds(char *cadena) {
	
	int fd = open(BLINKSTICK, O_WRONLY,S_IWRITE);	
	if(fd == -1) {
		fprintf(stderr, "Error open /dev/usb/blinkstick0\n");
		exit(1);
	}
	
	write(fd, cadena, sizeof(char)*strlen(cadena));
	close(fd);

}
	
void ronda() {
	int i,tiempo=100000;
	char cadena[50];
	
	
	for(i=0;i<8;i++){
		sprintf(cadena,"%i",i);
		strcat(cadena,sample_colors[i]);strcat(cadena,",");
		pintaLeds(cadena);
		usleep(tiempo);
	}
	
	
}

void procesaRandom(int posJugador,int random,char *color){
	char cadena[30],aux[30];
	
	sprintf(cadena,"%i",random);
	strcat(cadena,sample_colors[random]);strcat(cadena,",");
	sprintf(aux, "%i", posJugador);
	strcat(aux,color);
	strcat(cadena,aux);	
	pintaLeds(cadena);
	sleep(1);
	pintaLeds(aux);
}

void procesaMov(int posJugador,char *color){
	char cadena[30];
	
	sprintf(cadena,"%i",posJugador);
	strcat(cadena,color);strcat(cadena,",");
	pintaLeds(cadena);
	
}

int main(void){
	
	int i=0;
	char movimiento,random;
	int posJugador=0,posBorrar,num_mov=0;
	char *color;	
	int salir=0,nuevo = 1;
	char cadena[10];
	int tiempo=150000;

	srand(time(NULL));
	color = ROJO;

	printf("===================\n");
	printf("--UNE LOS COLORES--\n");
	printf("===================\n");
	printf("Eres el color rojo\n");
	printf("tecla a para ir a la izquierda\n");
	printf("tecla d para ir a la derecha\n");
	printf("1 para salir del juego\n");

	
	while(salir==0){
		system ("/bin/stty raw");
		if(nuevo==1){
			nuevo=0;
			do{
			random = rand() % (8);
			}while(random==posJugador);
			ronda();
			procesaRandom(posJugador,random,color);		
		}
		
		movimiento=getchar();
		num_mov++;
		if(movimiento == 'a'){
			posBorrar=posJugador;
			
			posJugador+=1;
			if(posJugador>=8)posJugador=0;
			procesaMov(posJugador,color);
		}
		else if(movimiento == 'd'){
			posBorrar=posJugador;
			
			posJugador-=1;
			if(posJugador<0)posJugador=7;
			procesaMov(posJugador,color);
		}
		else if (movimiento == '1'){
			salir=1;pintaLeds(" ");
		}
		system ("/bin/stty cooked");
		
		if(posJugador==random){
			
			for(i=0;i<2;i++){
				sprintf(cadena,"%i",posJugador);
				strcat(cadena,VACIO);
				pintaLeds(cadena);
				usleep(tiempo);
				sprintf(cadena,"%i",posJugador);
				strcat(cadena,ROJO);
				pintaLeds(cadena);
				usleep(tiempo);
			}
			
			printf("=>CONSEGUIDO en (%i) movimientos\n",num_mov);
			nuevo=1; num_mov=0;
		}
	}

return 0;
}
