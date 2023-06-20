/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova5.c
* Programa de prova per usar les funcions de posicionament (lseek)
* S'intercanvien grups de linies aleatoriament en la pantalla
*
*/


#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <time.h>

int tamany_buffer;

unsigned char *buffer_origen,*buffer_desti;
char *dispositiu_desti="/dev/ncdd/localhost/fb0";

char *dispositiu_origen="/dev/ncdd/localhost/fb0";
/* Es pot veure que el resultat es el mateix si usem el ncdd o si no l'usem,
 provem a canviar el dispositiu d'origen per:
 char *dispositiu_origen="/dev/fb0";
 I el resultat sera el mateix, pero sense passar pel modul ncdd
*/


#define RESOLUCIO_X 800
#define RESOLUCIO_Y 600
#define BPP 16

#define NUMERO_LINIES 10

int tamany_linia=RESOLUCIO_X*BPP/8;

int retorna_offset_aleatori(void)
{
	int n;

	n=rand()%(RESOLUCIO_Y/NUMERO_LINIES);
	return (n*NUMERO_LINIES)*tamany_linia;

}

int main (int argc,char *argv[])
{

	int file_desti;
	int file_origen;
	int posicio_origen,posicio_desti;

	int retorn;

	srand(time(NULL));

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova5\n"
			"Programa de prova per usar les funcions de posicionament (lseek)\n"
			"S'intercanvien grups de linies aleatoriament en la pantalla\n"
			"\n");

	tamany_buffer=tamany_linia*NUMERO_LINIES;

	printf ("tamany del buffer: %u\n",tamany_buffer);


	if ( (buffer_origen=malloc(tamany_buffer))==NULL) {
		printf ("Error al assignar memoria!\n");
		return -1;
	}

	if ( (buffer_desti=malloc(tamany_buffer))==NULL) {
		printf ("Error al assignar memoria!\n");
		return -1;
	}


	if ((file_desti=open(dispositiu_desti,O_RDWR))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_desti);
		return -1;
	}

	if ((file_origen=open(dispositiu_origen,O_RDWR))<0) {
		printf ("Error al obrir dispositiu %s\n",dispositiu_origen);
		return -1;
	}

	do {

		posicio_origen=retorna_offset_aleatori();
		retorn=lseek(file_origen,posicio_origen,SEEK_SET);

		if (retorn<0) {
			printf ("Error numero %d:\n",errno);
			perror("");
			return -1;
		}

		if (read(file_origen,buffer_origen,tamany_buffer)<0) {
			perror("");
			return -1;
		}

		posicio_desti=retorna_offset_aleatori();
		retorn=lseek(file_desti,posicio_desti,SEEK_SET);

		if (retorn<0) {
			printf ("Error numero %d:\n",errno);
			perror("");
			return -1;
		}

		if (read(file_desti,buffer_desti,tamany_buffer)<0) {
			perror("");
			return -1;
		}

		//Escriure

		retorn=lseek(file_origen,posicio_origen,SEEK_SET);

		if (retorn<0) {
			printf ("Error numero %d:\n",errno);
			perror("");
			return -1;
		}

		if (write(file_origen,buffer_desti,tamany_buffer)<0) {
			perror("");
			return -1;
		}

		retorn=lseek(file_desti,posicio_desti,SEEK_SET);

		if (retorn<0) {
			printf ("Error numero %d:\n",errno);
			perror("");
			return -1;
		}

		if (write(file_desti,buffer_origen,tamany_buffer)<0) {
			perror("");
			return -1;
		}


	} while (1==1);

	close(file_desti);
	close(file_origen);

	return 0;
}
