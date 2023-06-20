/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova3.c
* Prova d'enviament de dades de un dispositiu a un altre
* Fent us d'un buffer de dades
*
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>


int tamany_buffer;

unsigned char *buffer;
char *dispositiu_origen;
char *dispositiu_desti;

int main (int argc,char *argv[])
{

	int file_out;
	int file_in;

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova3\n"
			"Prova d'enviament de dades de un dispositiu a un altre\n"
			"Fent us d'un buffer de dades\n"
			"\n");


	if (argc!=4) {
		printf ("Sintaxi: prova3 <dispositiu origen> <dispositiu desti> <tamany buffer>\n");
		return -1;
	}

	dispositiu_origen=argv[1];
	dispositiu_desti=argv[2];
	tamany_buffer=atoi(argv[3]);

	printf ("dispositiu origen: %s\n"
					"dispositiu desti: %s\n"
					"tamany del buffer: %u\n",
					dispositiu_origen,dispositiu_desti,tamany_buffer);


	if (tamany_buffer<1) {
		printf ("Tamany de buffer incorrecte!\n");
		return -1;
	}

	if ( (buffer=malloc(tamany_buffer))==NULL) {
		printf ("Error al assignar memoria!\n");
		return -1;
	}


	if ((file_out=open(dispositiu_desti,O_WRONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_desti);
		perror("");
		return -1;
	}

	if ((file_in=open(dispositiu_origen,O_RDONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_origen);
		perror("");
		return -1;
	}

	while (1) {
	if (read(file_in,buffer,tamany_buffer)<0) {
		perror("");
		return -1;
	}

	if (write(file_out,buffer,tamany_buffer)<0) {
		perror("");
		return -1;
	}

  }
  close(file_out);
  close(file_in);
}
