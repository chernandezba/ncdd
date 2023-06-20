/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova4.c
* Programa de prova per usar les funcions de posicionament (lseek)
* Es copia la meitat inferior de la pantalla en la meitat superior
*
*/


#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int tamany_buffer;

unsigned char *buffer;
char *dispositiu_desti="/dev/cdx/localhost/fb0";


char *dispositiu_origen="/dev/cdx/localhost/fb0";
/* Es pot veure que el resultat es el mateix si usem el cdx o si no l'usem,
 provem a canviar el dispositiu d'origen per:
 char *dispositiu_origen="/dev/fb0";
 I el resultat sera el mateix, pero sense passar pel modul cdx
*/


#define RESOLUCIO_X 800
#define RESOLUCIO_Y 600
#define BPP 16

int tamany_linia=RESOLUCIO_X*BPP/8;

int main (int argc,char *argv[])
{

	int file_out;
	int file_in;

	int retorn;

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova4\n"
			"Programa de prova per usar les funcions de posicionament (lseek)\n"
			"Es copia la meitat inferior de la pantalla en la meitat superior\n"
			"\n");

	tamany_buffer=tamany_linia*RESOLUCIO_Y/2;

	printf ("tamany del buffer: %u\n",tamany_buffer);


	if ( (buffer=malloc(tamany_buffer))==NULL) {
		printf ("Error al assignar memoria!\n");
		return -1;
	}


	if ((file_out=open(dispositiu_desti,O_WRONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_desti);
		return -1;
	}

	if ((file_in=open(dispositiu_origen,O_RDONLY))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_origen);
		return -1;
	}

  //posicionem el punter de lectura a la meitat de la pantalla
	retorn=lseek(file_in,tamany_buffer,SEEK_SET);


	if (retorn<0) {
		printf ("Error numero %d:\n",errno);
		perror("");
		return -1;
	}

	printf ("Retorn o.k.:%d\n",retorn);


	if (read(file_in,buffer,tamany_buffer)<0) {
		perror("");
		return -1;
	}

	retorn=lseek(file_out,0,SEEK_SET);

	if (retorn<0) {
		printf ("Error numero %d:\n",errno);
		perror("");
		return -1;
	}

	printf ("Retorn o.k.:%d\n",retorn);

	if (write(file_out,buffer,tamany_buffer)<0) {
		perror("");
		return -1;
	}


	close(file_out);
	close(file_in);

	return 0;
}
