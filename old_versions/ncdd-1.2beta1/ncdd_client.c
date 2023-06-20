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
#include <asm/alternative.h>
#include <asm/atomic.h>

//new
//#include <linux/bitops.h>

//quitado
//#include <asm/atomic.h>

#include "ncdd.h"
#include "ncdd_util.h"

//#define DEBUG

int pid=1; //It's 0 for child processes, different to 0 for main process
int pid_pare;

//received signals
//atomic_t /*rebuda_senyal_open,*/rebuda_senyal_altres;
atomic_t rebuda_senyal_altres;
int rebuda_senyal_close;


/* ------------- For child processes ------------- */
unsigned char minor;	//minor number of the device to open
unsigned int flags;		//opening flags
unsigned int mode;		//opening mode
int dispositius;				//total devices
int f_control;					//NCDD device file
char buffer_petit[300]; //small buffer for communicating


/* --------------------------------- */

struct file *file; //File number to use

//Imported devices structure
struct t_taula_importacio {
  unsigned char minor;
	char host[255];
	char device[255];
};
//Imported devices array
struct t_taula_importacio taula_importacio[NCDD_MAX_DISPOSITIUS];


FILE *f_configuracio;	//client config file

//Used while reading config file
char s0[80];
char s1[80];
char s2[80];

//Identifier string used to communicate with module NCDD
char identificacio[]=NCDD_IDENTIFICACIO;

//Signaling procedure definition
typedef void (*sighandler_t)(int);

//Numero de fills creats
atomic_t fills_oberts;

time_t tiempo;
char *buffer_tiempo;


//L'altra tipus de senyal � interceptada pels fills ??
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
				printf ("ncdd_client: Received close signal (%d)\n\n",NCDD_SENYAL_PETICIO_CLOSE);
				printf ("ncdd_client: Waiting to terminate all children\n\n");
#endif

				do {
					sleep (1);
				}	while (atomic_read(&fills_oberts));

#ifdef DEBUG
				printf ("ncdd_client: All children closed. Finish\n\n");
#endif
				tiempo=time(NULL);
				buffer_tiempo=ctime(&tiempo);

				printf("\nStopping ncdd_client at %s",buffer_tiempo);

				exit (0);


			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_fill: Received close signal (%d)\n",NCDD_SENYAL_PETICIO_CLOSE);
#endif
			}

		break;

		case NCDD_SENYAL_PETICIO_ALTRES:
			//atomic_inc(&rebuda_senyal_altres);
			atomic_set(&rebuda_senyal_altres,1);
			if (pid_actual==pid_pare) { //Father code
#ifdef DEBUG
				printf ("ncdd_client (pid=%d): Received signal others (%d)\n",
								getpid(),NCDD_SENYAL_PETICIO_ALTRES);
#endif
			}
			else { //Child code
#ifdef DEBUG
				printf ("ncdd_client_fill (pid=%d): Received signal others (%d)\n",
								getpid(),NCDD_SENYAL_PETICIO_ALTRES);
#endif
			}

		break;

		case SIGCHLD:

			if (pid_actual==pid_pare) { //Father code
#ifdef DEBUG
				printf ("ncdd_client: Received signal SIGCHLD\n");
#endif
				//Recuperar proces zombie
				do {
					status=waitpid(-1,NULL,WNOHANG | WUNTRACED);
					if (status>0) {
						atomic_dec(&fills_oberts);
#ifdef DEBUG
					printf ("ncdd_client: Retrieving state child stopped\n");
#endif
					}
				} while (status>0);

			}

			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_child: Received signal SIGCHLD\n\n");
#endif
			}

		break;

		case SIGHUP:

			if (pid_actual==pid_pare) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_client: Received signal SIGHUP\n\n");
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_client_child: Received signal SIGHUP\n\n");
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
	  printf ("ncdd_client: Error registering signal close\n");
		exit(-1);
	}

	if (signal(NCDD_SENYAL_PETICIO_ALTRES,tractament_senyals)==SIG_ERR) {
	  printf ("ncdd_client: Error registering signal altres\n");
		exit(-1);
	}

	if (signal(SIGCHLD,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error registering signal SIGCHLD\n");
		exit(-1);
	}

	if (signal(SIGHUP,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error registering signal SIGHUP\n");
		exit(-1);
	}

	if (signal(SIGTERM,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error registering signal SIGTERM\n");
		exit(-1);
	}


}

extern void proces_fill(void);

int main (void) {
	int linia_nova;	//usat en la lectura del fitxer de configuracio
	int i;					//usat en la lectura del fitxer de configuracio
	char *p;				//punter usat en la transmissio de dades
	int f_log;

	printf ("\n"
			"Network char device driver\n"
			"\n"
			"Created by Cesar Hernandez\n"
			"\n"
			"Client program\n"
			"\n");
	printf ("Becoming a daemon...\n");
	atomic_set(&fills_oberts,0);

#ifndef DEBUG

	

	if ((f_log=open(NCDD_CLIENT_LOG,O_WRONLY | O_APPEND | O_CREAT))==-1) {
		printf ("ncdd_client: Error opening log file: %s\n",NCDD_CLIENT_LOG);
		exit(1);
	}

	if (dup2(f_log,1)==-1) {
		printf ("ncdd_client: Error running dup\n");
		exit(1);

	}

	if (daemon(0,1)==-1) {
		printf ("ncdd_client: Error running daemon\n");
		exit(1);
	}

#endif

	tiempo=time(NULL);
	buffer_tiempo=ctime(&tiempo);

	printf("\nStarting ncdd_client at %s",buffer_tiempo);




	//Obrir fitxer configuracio
	if ((f_configuracio=fopen(NCDD_IMPORTA,"r"))==NULL) {
	  printf ("ncdd_client: File %s not found!\n",NCDD_IMPORTA);
		exit(1);
  }

	//Llegir Dades
	dispositius=0;
	do {
	  linia_nova=1;
	  i=ncdd_llegeix_n_camps(f_configuracio,&linia_nova,3,s0,s1,s2);
		if (i<=0) {
		  if (i==NCDD_NO_MES_DADES) printf ("ncdd_client: No more data. OK\n");
		  if (i==NCDD_FALTEN_PARAMETRES) {
			  printf ("ncdd_client: Not enough parameters in line\n");
				exit(i);
      }
			if (i==NCDD_SOBREN_PARAMETRES) {
			  printf ("ncdd_client: Too much parameters in line\n");
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
	printf ("ncdd_client: All NCDD devices has major %d\n",NCDD_MAJOR);
	printf ("ncdd_client: Minor number to control NCDD is %d\n",NCDD_CONTROL);
#endif

	pid_pare=getpid();

#ifdef DEBUG
	printf ("ncdd_client: Father Pid: %d\n",pid_pare);
#endif


	//A partir d'aqui, hem de registrar el client

	//Obrir dispositiu de control NCDD
	if ((f_control=open("/dev/ncdd/ncdd",O_RDWR))<0) {
    printf ("ncdd_client: Error opening device\n");
    exit (-1);
  }

	//Enviar cadena identificativa

	if (ioctl(f_control,NCDD_IOCTL_REGISTRAR_CLIENT,&identificacio)<0) {
    printf ("ncdd_client: Error sending identifier string %d\n",errno);
		exit (-1);
  }

#ifdef DEBUG
	printf ("ncdd_client: Client Registered\n");
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
				printf ("ncdd_client: Error reading open petition. Signal received?\n");
#endif
			}

			else {

				p=&buffer_petit[0];
				file=VALOR_file(p);
				minor=VALOR_unsigned_char(&buffer_petit[4]);
				flags=VALOR_unsigned_int(&buffer_petit[5]);
				mode=VALOR_unsigned_int(&buffer_petit[9]);
				printf ("ncdd_client: Petition open: file:%p minor:%d flags:%d"
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
				printf ("ncdd_client: Child process created PID:%d\n",pid);
#endif
				//Avisar el modul del proces fill
				*(int *)(&buffer_petit[4])=pid;
				if ( ioctl(f_control,NCDD_IOCTL_ASSIGNAR_FILL,&buffer_petit) <0) {
					printf ("ncdd_client: Error allocating child. Kill him\n");

					//Matar proces fill creat
					kill(pid,SIGKILL);
				}

				else {
#ifdef DEBUG
					printf ("ncdd_client: Tell child process he is registered pid:%d\n",pid);
#endif
					kill (pid,NCDD_SENYAL_PETICIO_ALTRES);
				}
				//Aqui s'acaben les feines del client pare

			}

  } while (1);
}
