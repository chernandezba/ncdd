/*

    ncdd

    Copyright (c) 2002 Cesar Hernandez <chernandezba@hotmail.com>

    This file is part of ncdd

    ncdd is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


*/

/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer ncdd_client.c
* Programa client de ncdd
*
*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <asm/atomic.h>

#include "ncdd.h"
#include "ncdd_util.h"

//#define DEBUG

int pid=1; //It's 0 for child processes, different to 0 for main process
int pid_pare;

//received signals
atomic_t /*rebuda_senyal_open,*/rebuda_senyal_altres;
int rebuda_senyal_close;


/* ------------- For child processes ------------- */
unsigned char minor;	//minor number of the device to open
unsigned int flags;		//opening flags
unsigned int mode;		//opening mode
int dispositius;				//total devices
int f_control;					//NCDD device file
char buffer_petit[300]; //small buffer for communicating

//Imported devices array
struct t_taula_importacio taula_importacio[NCDD_MAX_DISPOSITIUS];

/* --------------------------------- */

struct file *file; //File number to use

//Imported devices structure
struct t_taula_importacio {
  unsigned char minor;
	char host[255];
	char device[255];
};


FILE *f_configuracio;	//client config file

//Used while reading config file
char s0[80];
char s1[80];
char s2[80];

//Identifier string Cadena identificativa usada en la connexio amb el modul NCDD
char identificacio[]=NCDD_IDENTIFICACIO;

//Definició de funció controladora de senyals
typedef void (*sighandler_t)(int);

//Numero de fills creats
atomic_t fills_oberts;

time_t tiempo;
char *buffer_tiempo;


//L'altra tipus de senyal és interceptada pels fills ??
void tractament_senyals (int s) {

	int status,pid_actual;

	pid_actual=getpid();

	switch (s) {
/*		case NCDD_SENYAL_PETICIO_OPEN:
			atomic_inc(&	rebuda_senyal_open);
			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client: Rebuda senyal open (%d)\n",NCDD_SENYAL_PETICIO_OPEN);
#endif

			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill: Rebuda senyal open (%d)\n",NCDD_SENYAL_PETICIO_OPEN);
#endif
			}

		break;*/

		case NCDD_SENYAL_PETICIO_CLOSE:
		case SIGTERM:
			rebuda_senyal_close=1;
			if (pid_actual==pid_pare) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client: Rebuda senyal close (%d)\n\n",NCDD_SENYAL_PETICIO_CLOSE);
				printf ("ncdd_client: Esperant a terminar els fills\n\n");
#endif

				do {
					sleep (1);
				}	while (atomic_read(&fills_oberts));

#ifdef DEBUG
				printf ("ncdd_client: Tots els fills tancats. Acabar\n\n");
#endif
				tiempo=time(NULL);
				buffer_tiempo=ctime(&tiempo);

				printf("\nStopping ncdd_client at %s",buffer_tiempo);

				exit (0);


			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill: Rebuda senyal close (%d)\n",NCDD_SENYAL_PETICIO_CLOSE);
#endif
			}

		break;

		case NCDD_SENYAL_PETICIO_ALTRES:
			//atomic_inc(&rebuda_senyal_altres);
			atomic_set(&rebuda_senyal_altres,1);
			if (pid_actual==pid_pare) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client (pid=%d): Rebuda senyal altres (%d)\n",
								getpid(),NCDD_SENYAL_PETICIO_ALTRES);
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill (pid=%d): Rebuda senyal altres (%d)\n",
								getpid(),NCDD_SENYAL_PETICIO_ALTRES);
#endif
			}

		break;

		case SIGCHLD:

			if (pid_actual==pid_pare) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client: Rebuda senyal SIGCHLD\n");
#endif
				//Recuperar proces zombie
				do {
					status=waitpid(-1,NULL,WNOHANG | WUNTRACED);
					if (status>0) {
						atomic_dec(&fills_oberts);
#ifdef DEBUG
					printf ("ncdd_client: Recuperat estat fill terminat\n");
#endif
					}
				} while (status>0);

			}

			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill: Rebuda senyal SIGCHLD\n\n");
#endif
			}

		break;

		case SIGHUP:

			if (pid_actual==pid_pare) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client: Rebuda senyal SIGHUP\n\n");
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill: Rebuda senyal SIGHUP\n\n");
#endif
				rebuda_senyal_close=1;

			}

		break;


		default:
		break;
	}
}

void registra_senyals(void)
{
	/*if (signal(NCDD_SENYAL_PETICIO_OPEN,tractament_senyals)==SIG_ERR) {
	  printf ("ncdd_client: Error al registrar senyal open\n");
		exit(-1);
	}*/

	if (signal(NCDD_SENYAL_PETICIO_CLOSE,tractament_senyals)==SIG_ERR) {
	  printf ("ncdd_client: Error al registrar senyal close\n");
		exit(-1);
	}

	if (signal(NCDD_SENYAL_PETICIO_ALTRES,tractament_senyals)==SIG_ERR) {
	  printf ("ncdd_client: Error al registrar senyal altres\n");
		exit(-1);
	}

	if (signal(SIGCHLD,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGCHLD\n");
		exit(-1);
	}

	if (signal(SIGHUP,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGHUP\n");
		exit(-1);
	}

	if (signal(SIGTERM,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGTERM\n");
		exit(-1);
	}


}

extern void proces_fill(void);

int main (void) {
	int linia_nova;	//usat en la lectura del fitxer de configuracio
	int i;					//usat en la lectura del fitxer de configuracio
	char *p;				//punter usat en la transmissio de dades
	int f_log;

	atomic_set(&fills_oberts,0);

#ifndef DEBUG

	//printf ("GG\n");

	if ((f_log=open(NCDD_CLIENT_LOG,O_WRONLY | O_APPEND | O_CREAT))==-1) {
		printf ("ncdd_client: Error opening log file: %s\n",NCDD_CLIENT_LOG);
		exit(1);
	}

	if (dup2(f_log,1)==-1) {
		printf ("ncdd_client: Error running dup\n");
		exit(1);

	}

	//printf ("JJ\n");


	if (daemon(0,1)==-1) {
		printf ("ncdd_client: Error running daemon\n");
		exit(1);
	}

#endif

	tiempo=time(NULL);
	buffer_tiempo=ctime(&tiempo);

	printf("\nStarting ncdd_client at %s",buffer_tiempo);


	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"Programa client de ncdd\n"
			"\n");

	//Obrir fitxer configuracio
	if ((f_configuracio=fopen(NCDD_IMPORTA,"r"))==NULL) {
	  printf ("ncdd_client: Fitxer %s no trobat!\n",NCDD_IMPORTA);
		exit(1);
  }

	//Llegir Dades
	dispositius=0;
	do {
	  linia_nova=1;
	  i=ncdd_llegeix_n_camps(f_configuracio,&linia_nova,3,s0,s1,s2);
		if (i<=0) {
		  if (i==NCDD_NO_MES_DADES) printf ("ncdd_client: No hi ha més dades. OK\n");
		  if (i==NCDD_FALTEN_PARAMETRES) {
			  printf ("ncdd_client: Falten parametres a la linia\n");
				exit(i);
      }
			if (i==NCDD_SOBREN_PARAMETRES) {
			  printf ("ncdd_client: Sobren parametres a la linia\n");
				exit(i);
      }
    }
		else {
		  taula_importacio[dispositius].minor=atoi(s0);
		  strcpy(taula_importacio[dispositius].host,s1);
		  strcpy(taula_importacio[dispositius].device,s2);
		  printf ("ncdd_client: Minor: %d Host:%s Device:%s\n",taula_importacio[dispositius].minor,
	            taula_importacio[dispositius].host,taula_importacio[dispositius].device);
      dispositius++;
    }

	} while (!feof(f_configuracio));
	fclose(f_configuracio);

#ifdef DEBUG
	printf ("ncdd_client: Tots els dispositius NCDD tenen un major igual a %d\n",NCDD_MAJOR);
	printf ("ncdd_client: El valor minor pel control del NCDD té un valor igual a %d\n",NCDD_CONTROL);
#endif

	pid_pare=getpid();

#ifdef DEBUG
	printf ("ncdd_client: Pid del pare: %d\n",pid_pare);
#endif


	//A partir d'aqui, hem de registrar el client

	//Obrir dispositiu de control NCDD
	if ((f_control=open("/dev/ncdd/ncdd",O_RDWR))<0) {
    printf ("ncdd_client: Error al obrir dispositiu\n");
    exit (-1);
  }

	//Enviar cadena identificativa

	if (ioctl(f_control,NCDD_IOCTL_REGISTRAR_CLIENT,&identificacio)<0) {
    printf ("ncdd_client: Error al enviar cadena identificativa %d\n",errno);
		exit (-1);
  }

#ifdef DEBUG
	printf ("ncdd_client: Client Registrat\n");
#endif
	/*atomic_set(&rebuda_senyal_open,0); //=0;*/
	rebuda_senyal_close=0;
	atomic_set(&rebuda_senyal_altres,0);
	registra_senyals();

	fflush(stdout);


	//A partir d'aqui, esperar senyal SIGALARM
	do {
		/*if (!atomic_read(&rebuda_senyal_open)) pause();*/

			//atomic_dec(&rebuda_senyal_open);

		//Llegir valors de la peticio, minor, file
			if ( (ioctl(f_control,NCDD_IOCTL_LLEGIR_OPEN,
				&buffer_petit)) <0) {
#ifdef DEBUG
				printf ("ncdd_client: Error al llegir la peticio open. Senyal rebuda?\n");
#endif
			}

			else {

				p=&buffer_petit[0];
				file=VALOR_file(p);
				minor=VALOR_unsigned_char(&buffer_petit[4]);
				flags=VALOR_unsigned_int(&buffer_petit[5]);
				mode=VALOR_unsigned_int(&buffer_petit[9]);
				printf ("ncdd_client: Peticio open: file:%p minor:%d flags:%d"
								" flags & 3:%d mode:%o\n",file,(int)minor,flags,flags&3,mode);

				//Creem el fill
				//Aqui sorgeix un problema. El pare ha de crear el proces fill,
				//i despres avisar el modul ncdd que el registri. Pero desde el
				//moment que crea el fill, s'executa, i no ha de fer res
				//abans que estigui registrat pel pare. Per tant, no ha d'executar
				//res fins que no rebi una senyal, i la rebra quan estigui registrat

				atomic_inc(&fills_oberts);

				pid=fork();
				if (!pid) { //Proces fill
					//pid=0; //???
					proces_fill();
				}

#ifdef DEBUG
				printf ("ncdd_client: Proces fill creat PID:%d\n",pid);
#endif
				//Avisar el modul del proces fill
				*(int *)(&buffer_petit[4])=pid;
				if ( ioctl(f_control,NCDD_IOCTL_ASSIGNAR_FILL,&buffer_petit) <0) {
					printf ("ncdd_client: Error al assignar fill. Matar-lo\n");

					//Matar proces fill creat
					kill(pid,SIGKILL);
				}

				else {
#ifdef DEBUG
					printf ("ncdd_client: Avisem al proces fill que ja esta registrat pid:%d\n",pid);
#endif
					kill (pid,NCDD_SENYAL_PETICIO_ALTRES);
				}
				//Aqui s'acaben les feines del client pare

			}

  } while (1);
}
