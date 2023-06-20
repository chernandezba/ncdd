/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova2.c
* Prova de lectura i escriptura de dades de la targeta de so
* Executant ordres ioctl per canviar la frequencia
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
char *dispositiu_origen="/dev/cdx/localhost/dsp";
char *dispositiu_desti="/dev/cdx/localhost/dsp";

int main (int argc,char *argv[])
{

	int so_out;
	int so_in;

	int freq=FREQ_SAMPLEADO;
	int format=FORMATO;
	int stereo=STEREO;
	int font=SOUND_MASK_CD;
	//Si volem cambiar la font de dades, per exemple, a l'entrada de Línia,
	//modificar-ho per SOUND_MASK_LINE


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
	if ((so_in=open(dispositiu_origen,O_RDONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_origen);
		perror("");
		return -1;
	}

	if (ioctl(so_in,SNDCTL_DSP_SPEED,&freq) < 0) {
		printf ("Error al establir frequencia\n");
		perror("");
		return -1;
	}

	if (ioctl(so_in,SNDCTL_DSP_SETFMT,&format) < 0) {
		printf ("Error al establir 16 bits\n");
		perror("");
		return -1;
	}

	if (ioctl(so_in,SNDCTL_DSP_STEREO,&stereo) < 0) {
		printf ("Error al establir stereo\n");
		perror("");
		return -1;
	}

	if (ioctl(so_in,SOUND_MIXER_WRITE_RECSRC,&font) < 0) {
		printf ("Error al establir grabacio desde CD\n");
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
			"prova2\n"
			"Prova de lectura i escriptura de dades de la targeta de so\n"
			"Executant ordres ioctl per canviar la frequencia\n"
			"\n");


	printf ("Buffer de lectura: %d Bytes\n",
		SAMPLE);


	while (1) {

		if (read(so_in,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}
		if (write(so_out,&buffer,SAMPLE)<0) {
			perror("");
			return -1;
		}


	}

	close(so_out);
	close(so_in);
}
