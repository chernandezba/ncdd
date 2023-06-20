/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova6.c
* Programa de prova per veure el màxim de peticions admeses
*
*/


#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "ncdd.h"

int tamany_buffer;

unsigned char *buffer;
char *dispositiu="/dev/ncdd/localhost/zero";


int main (int argc,char *argv[])
{

	int file_in;

	int retorn;
	int fills=0;
	int pid;

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova6\n"
			"Programa de prova per veure el màxim de peticions admeses\n"
			"\n");

	tamany_buffer=100;

	printf ("tamany del buffer: %u\n",tamany_buffer);


	if ( (buffer=malloc(tamany_buffer))==NULL) {
		printf ("Error al assignar memoria!\n");
		return -1;
	}


	if ((file_in=open(dispositiu,O_RDONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu);
		perror("");
		return -1;
	}

	do {
		printf ("fills:%d\n",fills++);
		pid=fork();
	} while (pid && fills<NCDD_MAX_PETICIONS_SIMULTANIES+3);

	if (pid) printf ("\nExecutar: killall prova6\n\n");
	else {

		do {
			retorn=lseek(file_in,0,SEEK_SET);


			if (retorn<0) {
				printf ("Error numero %d: %s\n",errno,strerror(errno));
				return -1;
			}

			if (read(file_in,buffer,tamany_buffer)<0) {
				printf ("Error numero %d: %s\n",errno,strerror(errno));
				return -1;
			}

		} while (1);
	}

	close(file_in);

	return 0;
}
