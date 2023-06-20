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
* Fitxer ncdd_servidor.c
* Programa servidor de ncdd
*
*/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/atomic.h>

#include "ncdd.h"
#include "ncdd_util.h"
#include "ncdd_net.h"

//#define DEBUG

//Usat en la connexió per xarxa
struct sockaddr_in adr;
int sock,sock_conectat;
unsigned int long_adr;
//

//Estructura de la taula de dispositius exportats
struct t_taula_exportacio {
	char host[255];
	char device[255];
	char permisos[4];
};

//Vector de dispositius exportats
struct t_taula_exportacio taula_exportacio[NCDD_MAX_DISPOSITIUS_EXPORTAR];

FILE *f_configuracio;	//fitxer de configuracio del servidor

//Usat en la lectura del fitxer de configuracio
char s0[80];
char s1[80];
char s2[80];

int dispositius;				//numero total de dispositius exportats


int pid=1; //Val 0 pels fills, i diferent de 0 pel pare

extern void proces_fill();
extern void tancar_dispositiu(void);

typedef void (*sighandler_t)(int);

atomic_t fills_oberts;
time_t tiempo;
char *buffer_tiempo;


void tractament_senyals (int s) {

	int status;

	switch (s) {

		case SIGCHLD:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_servidor: Rebuda senyal SIGCHLD\n");
#endif
				//Recuperar proces zombie
				do {
					status=waitpid(-1,NULL,WNOHANG | WUNTRACED);
					if (status>0) {
						atomic_dec(&fills_oberts);
#ifdef DEBUG
					printf ("ncdd_servidor: Recuperat estat fill terminat\n");
#endif
					}
				} while (status>0);

			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_servidor_fill: Rebuda senyal SIGCHLD\n");
#endif
			}

		break;

		case SIGHUP:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_servidor: Rebuda senyal SIGHUP\n");
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_servidor_fill: Rebuda senyal SIGHUP\n\n");
#endif
				tancar_dispositiu();
			}

		break;

		case SIGINT:
		case SIGTERM:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("ncdd_servidor: Rebuda senyal SIGINT\n");
				printf ("ncdd_servidor: Esperant a terminar els fills\n\n");
#endif

				do {
					sleep (1);
				}	while (atomic_read(&fills_oberts));
#ifdef DEBUG
				printf ("ncdd_servidor: Tots els fills tancats. Acabar\n\n");
#endif

				tiempo=time(NULL);
				buffer_tiempo=ctime(&tiempo);

				printf("\nStopping ncdd_servidor at %s",buffer_tiempo);

				exit (0);
			}

			else { //Codi pel fill
#ifdef DEBUG
				printf ("ncdd_servidor_fill: Rebuda senyal SIGINT\n\n");
#endif
				tancar_dispositiu();
			}

		break;


		default:
		break;
	}
}


void registra_senyals(void)
{

	if (signal(SIGCHLD,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGCHLD\n");
		exit(-1);
	}

	if (signal(SIGHUP,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGHUP\n");
		exit(-1);
	}

	if (signal(SIGINT,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGINT\n");
		exit(-1);
	}

	if (signal(SIGTERM,tractament_senyals)==SIG_ERR) {
		printf ("ncdd_client: Error al registrar senyal SIGTERM\n");
		exit(-1);
	}


}


int main (void) {
	int linia_nova;	//usat en la lectura del fitxer de configuracio
	int i;					//usat en la lectura del fitxer de configuracio

	int f_log;

	atomic_set(&fills_oberts,0);

#ifndef DEBUG

	if ((f_log=open(NCDD_SERVIDOR_LOG,O_WRONLY | O_APPEND | O_CREAT))==-1) {
		printf ("ncdd_servidor: Error opening log file: %s\n",NCDD_SERVIDOR_LOG);
		exit(1);
	}

	if (dup2(f_log,1)==-1) {
		printf ("ncdd_servidor: Error running dup\n");
		exit(1);

	}


	if (daemon(0,1)==-1) {
		printf ("ncdd_servidor: Error running daemon\n");
		exit(1);
	}

#endif

	tiempo=time(NULL);
	buffer_tiempo=ctime(&tiempo);

	printf("\nStarting ncdd_servidor at %s",buffer_tiempo);


	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"Programa servidor de ncdd\n"
			"\n");

			//fflush(stdout); //

	//Obrir fitxer configuracio
	if ((f_configuracio=fopen(NCDD_EXPORTA,"r"))==NULL) {
	  printf ("ncdd_servidor: Fitxer %s no trobat!\n",NCDD_EXPORTA);
		exit(1);
  }

	//Llegir Dades
	dispositius=0;
	do {
	  linia_nova=1;
	  i=ncdd_llegeix_n_camps(f_configuracio,&linia_nova,3,s0,s1,s2);
		if (i<=0) {
		  if (i==NCDD_NO_MES_DADES) printf ("ncdd_servidor: No hi ha més dades. OK\n");
		  if (i==NCDD_FALTEN_PARAMETRES) {
			  printf ("ncdd_servidor: Falten parametres a la linia\n");
				exit(i);
      }
			if (i==NCDD_SOBREN_PARAMETRES) {
			  printf ("ncdd_servidor: Sobren parametres a la linia\n");
				exit(i);
      }

    }

		else if (strlen(s2)>3) {
			printf ("ncdd_servidor: Permisos no valids : %s\n",s2);
			exit(-3);
		}

		else {

		  strcpy(taula_exportacio[dispositius].host,s0);
		  strcpy(taula_exportacio[dispositius].device,s1);
			strcpy(taula_exportacio[dispositius].permisos,s2);
		  printf ("ncdd_servidor: Host:%s Device:%s Permisos:%s\n",
							taula_exportacio[dispositius].host,
							taula_exportacio[dispositius].device,
							taula_exportacio[dispositius].permisos);
      dispositius++;
    }

	} while (!feof(f_configuracio));
	fclose(f_configuracio);



	if ((sock=crear_socket_TCP())<0) {
	  printf ("Error al crear socket TCP\n");
		exit(-1);
	}

	if (assignar_adr_internet(sock,NULL,NCDD_PORT)<0) {
	  printf ("Error al assignar adreca socket\n");
		exit(-1);
	}
	if (listen(sock,1)<0) {
	  printf ("Error al fer listen\n");
		exit(-1);
	}

	printf ("Escoltant connexions en port %u\n",NCDD_PORT);

	registra_senyals();

	fflush(stdout);

	while (1)
	{
	  long_adr=sizeof(adr);
		if ((sock_conectat=accept(sock,(struct sockaddr *)&adr,&long_adr))<0) {
		  printf ("Error al fer accept\n");
			exit(-1);
		}

		printf ("\nRebuda peticio de connexio\n");

		atomic_inc(&fills_oberts);

		if ((pid=fork())<0) {
			printf ("Error al fer fork\n");
			exit(-1);
		}

		if (pid==0) {
			proces_fill();
		}
	}



	return 0;
}
