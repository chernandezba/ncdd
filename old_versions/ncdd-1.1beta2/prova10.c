/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova10.c
* Prova de lectura de dades per stdin i sortida per la targeta de so
* usant buffers i executant ordres ioctl per canviar la frequencia
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


#define SAMPLE 800*2*300

unsigned char buffer[SAMPLE];

char *dispositiu_origen="/dev/fb0";
char *dispositiu_desti="/dev/ncdd/localhost/fb0";
//char *dispositiu_desti="/dev/dsp";

int main (int argc,char *argv[])
{

	int file_in,file_out;

	int leidos,l;


	/* Inicialitzar escriptura */

	if ((file_out=open(dispositiu_desti,O_WRONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_desti);
		perror("");
		return -1;
	}


	/* Inicializar lectura */

	if ((file_in=open(dispositiu_origen,O_RDONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_origen);
		perror("");
		return -1;
	}


	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova10\n"
			"Prova de lectura per stdin i escriptura de dades de la targeta de so\n"
			"usant buffers i executant ordres ioctl per canviar la frequencia\n"
			"\n");


	printf ("Buffer de lectura: %d Bytes\n",
		SAMPLE);


	while (1) {

		if (lseek(file_in,0,SEEK_SET)<0) {
			perror("");
			return -1;
		}


		if (lseek(file_out,SAMPLE,SEEK_SET)<0) {
			perror("");
			return -1;
		}


		leidos=0;
		do {
			l=read(file_in,buffer+leidos,SAMPLE-leidos);

			if (l>0) leidos +=l;

		} while (leidos!=SAMPLE);


		if (write(file_out,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}


	}

	close(file_out);
	close(file_in);
}
