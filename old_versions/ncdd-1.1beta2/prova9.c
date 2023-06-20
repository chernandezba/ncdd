/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova9.c
* Prova de lectura de dades per stdin i sortida per stdout usant buffers
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


#define SAMPLE 100000

unsigned char buffer[SAMPLE];

//char *dispositiu_desti="/dev/ncdd/localhost/dsp";
//char *dispositiu_desti="/dev/dsp";

int main (int argc,char *argv[])
{

	int so_out;

	int leidos,l;


	while (1) {

		leidos=0;
		do {
			l=read(0,buffer+leidos,SAMPLE-leidos);

			if (l>0) leidos +=l;

		} while (leidos!=SAMPLE);


		if (write(1,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}

	}

	close(so_out);

}
