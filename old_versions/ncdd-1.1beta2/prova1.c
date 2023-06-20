/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer prova1.c
* Prova molt simple d'apertura, lectura, ordre ioctl i tancament
*
*/

#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>

#include <sys/types.h>

int fd;
char buf[10000];

unsigned char retorna_ascii(unsigned char c)
{
	if (c<32 || c>127) return '.';
	else return c;
}

void escriu_buffer(char *p,int longitut)
{
	for (;longitut;longitut--,p++)
	printf ("%c",retorna_ascii(*p));
	//printf ("%d ",*p);
}

int main (void) {
	int err;
	int dades_llegides;

	int argument_ioctl;
	int comand_ioctl;

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"prova1\n"
			"Prova molt simple d'apertura, lectura, ordre ioctl i tancament\n"
			"\n");


	printf ("Open\n");

	if ((fd=open ("/dev/ncdd/localhost/dsp",O_RDONLY,0))<0) {
		printf("Error al obrir dispositiu : %d\n",errno);
		perror("");
	 exit(-1);
  }


  printf ("Open Ok fd=%d.\n",fd);

  printf ("Prem una tecla per llegir dades\n");
  getchar();


  printf ("Read\n");
  if ((dades_llegides=read(fd,buf,4097))<0) {
    printf ("Error al llegir : %d\n",errno);
	 perror("");
	 exit(-1);
  }

  escriu_buffer(buf,1000);
	printf ("\nDades llegides: %d\n",dades_llegides);

	printf ("Prem una tecla per cridar a ioctl\n");
	getchar();

	comand_ioctl=SNDCTL_DSP_SPEED;
	argument_ioctl=44100;

	printf ("ioctl, comand:%X, argument: %u\n",comand_ioctl,argument_ioctl);

	if (ioctl(fd,comand_ioctl,&argument_ioctl) < 0) {
		printf ("Error al establir frequencia");
		 perror("");
		 exit(-1);
	}


  printf ("Prem una tecla per tancar dispositiu\n");
  getchar();
  printf ("Close\n");
  if ((err=close(fd))<0) {
    printf("Error al tancar dispositiu : %d\n",errno);
	 perror("");
	 exit(-1);
  }
  printf ("Close Ok err=%d.\n",err);
  return 0;

}
