/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova7.c
* Prova per comprovar els permisos
*
*/


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>


int main (int argc,char *argv[])
{

	int file_out;


	if ((file_out=open("/dev/ncdd/localhost/fb0",O_RDWR))<0) {
		printf ("Error al obrir dispositiu\n");
		perror("");
		return -1;
	}

	printf ("Obertura correcta!\n");

	close(file_out);

	return 0;
}
