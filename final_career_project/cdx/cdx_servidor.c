/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_servidor.c
* Programa servidor de cdx
*
*/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <sys/wait.h>
#include <asm/atomic.h>

#include "cdx.h"
#include "cdx_util.h"
#include "cdx_xarxa.h"

#define DEBUG

//Usat en la connexió per xarxa
struct sockaddr_in adr;
int sock,sock_conectat;
unsigned int long_adr;
//

int pid=1; //Val 0 pels fills, i diferent de 0 pel pare

extern void proces_fill();
extern void tancar_dispositiu(void);

typedef void (*sighandler_t)(int);

atomic_t fills_oberts;

void tractament_senyals (int s) {

	int status;

	switch (s) {

		case SIGCHLD:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_servidor: Rebuda senyal SIGCHLD\n");
#endif
				//Recuperar proces zombie
				do {
					status=waitpid(-1,NULL,WNOHANG | WUNTRACED);
					if (status>0) {
						atomic_dec(&fills_oberts);
#ifdef DEBUG
					printf ("cdx_servidor: Recuperat estat fill terminat\n");
#endif
					}
				} while (status>0);

			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_servidor_fill: Rebuda senyal SIGCHLD\n");
#endif
			}

		break;

		case SIGHUP:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_servidor: Rebuda senyal SIGHUP\n");
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_servidor_fill: Rebuda senyal SIGHUP\n\n");
#endif
				tancar_dispositiu();
			}

		break;

		case SIGINT:
		case SIGTERM:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_servidor: Rebuda senyal SIGINT\n");
				printf ("cdx_servidor: Esperant a terminar els fills\n\n");
#endif

				do {
					sleep (1);
				}	while (atomic_read(&fills_oberts));
#ifdef DEBUG
				printf ("cdx_servidor: Tots els fills tancats. Acabar\n\n");
#endif
				exit (0);
			}

			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_servidor_fill: Rebuda senyal SIGINT\n\n");
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
		printf ("cdx_client: Error al registrar senyal SIGCHLD\n");
		exit(-1);
	}

	if (signal(SIGHUP,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGHUP\n");
		exit(-1);
	}

	if (signal(SIGINT,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGINT\n");
		exit(-1);
	}

	if (signal(SIGTERM,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGTERM\n");
		exit(-1);
	}


}


int main (void) {

	atomic_set(&fills_oberts,0);

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"Programa servidor de cdx\n"
			"\n");


	if ((sock=crear_socket_TCP())<0) {
	  printf ("Error al crear socket TCP\n");
		exit(-1);
	}

	if (assignar_adr_internet(sock,NULL,CDX_PORT)<0) {
	  printf ("Error al assignar adreca socket\n");
		exit(-1);
	}
	if (listen(sock,1)<0) {
	  printf ("Error al fer listen\n");
		exit(-1);
	}

	printf ("Escoltant connexions en port %u\n",CDX_PORT);

	registra_senyals();

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
