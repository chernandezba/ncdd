/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova8.c
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


#define FREQ_SAMPLEADO 44100

#define FORMATO AFMT_S16_LE
#define STEREO 1

#define SAMPLE FREQ_SAMPLEADO*4

unsigned char buffer[SAMPLE];

char *dispositiu_desti="/dev/ncdd/localhost/dsp";
//char *dispositiu_desti="/dev/dsp";

int main (int argc,char *argv[])
{

	int so_out;
	int so_in;

	int freq=FREQ_SAMPLEADO;
	int format=FORMATO;
	int stereo=STEREO;
	int leidos,l;


	/* Inicialitzar escriptura */

	if ((so_out=open(dispositiu_desti,O_WRONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_desti);
		perror("");
		return -1;
	}

	if (ioctl(so_out,SNDCTL_DSP_SPEED,&freq) < 0) {
		printf ("Error al establir frequencia\n");
		perror("");
		return -1;
	}

	if (ioctl(so_out,SNDCTL_DSP_SETFMT,&format) < 0) {
		printf ("Error al establir 16 bits\n");
		perror("");
		return -1;
	}

	if (ioctl(so_out,SNDCTL_DSP_STEREO,&stereo) < 0) {
		printf ("Error al establir stereo\n");
		perror("");
		return -1;
	}


	/* Inicializar lectura */

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova8\n"
			"Prova de lectura per stdin i escriptura de dades de la targeta de so\n"
			"usant buffers i executant ordres ioctl per canviar la frequencia\n"
			"\n");


	printf ("Buffer de lectura: %d Bytes\n",
		SAMPLE);


	while (1) {

		leidos=0;
		do {
			l=read(0,buffer+leidos,SAMPLE-leidos);

			/*if (l<0) {
				perror ("");
				return -1;
			}*/

			if (l>0) leidos +=l;
			//printf ("%d,%d ",leidos,SAMPLE);

		} while (leidos!=SAMPLE);




		/*for (i=0;i<SAMPLE;i++) {
			buffer[i]=fgetc(stdin);
		}*/

		/*if (read(0,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}*/
		if (write(so_out,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}


	}

	close(so_out);
	close(so_in);
}
