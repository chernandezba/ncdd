/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_client.c
* Programa client de cdx
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

#include <sys/wait.h>

#include <asm/atomic.h>

#include "cdx.h"
#include "cdx_util.h"

#define DEBUG

int pid=1; //Val 0 pels fills, i diferent de 0 pel pare

//senyals rebudes
atomic_t rebuda_senyal_open,rebuda_senyal_altres;
int rebuda_senyal_close;


/* -------------Pel fill ------------- */
unsigned char minor;	//numero minor del dispositiu a obrir
unsigned int flags;		//flags d'apertura
unsigned int mode;		//mode d'apertura
int dispositius;				//numero total de dispositius
int f_control;					//fitxer dispositiu CDX
char buffer_petit[300]; //buffer petit de comunicacio

//Vector de dispositius importats
struct t_taula_importacio taula_importacio[CDX_MAX_DISPOSITIUS];

/* --------------------------------- */

struct file *file; //Numero del file a usar

//Estructura de la taula de dispositius importats
struct t_taula_importacio {
  unsigned char minor;
	char host[255];
	char device[255];
};


FILE *f_configuracio;	//fitxer de configuracio del client

//Usat en la lectura del fitxer de configuracio
char s0[80];
char s1[80];
char s2[80];

//Cadena identificativa usada en la connexio amb el modul CDX
char identificacio[]=CDX_IDENTIFICACIO;

//Definició de funció controladora de senyals
typedef void (*sighandler_t)(int);

//Numero de fills creats
atomic_t fills_oberts;

//L'altra tipus de senyal és interceptada pels fills ??
void tractament_senyals (int s) {

	int status;

	switch (s) {
		case CDX_SENYAL_PETICIO_OPEN:
			atomic_inc(&	rebuda_senyal_open);
			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_client: Rebuda senyal open (%d)\n",CDX_SENYAL_PETICIO_OPEN);
#endif

			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda senyal open (%d)\n",CDX_SENYAL_PETICIO_OPEN);
#endif
			}

		break;

		case CDX_SENYAL_PETICIO_CLOSE:
		case SIGTERM:
			rebuda_senyal_close=1;
			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_client: Rebuda senyal close (%d)\n\n",CDX_SENYAL_PETICIO_CLOSE);
				printf ("cdx_client: Esperant a terminar els fills\n\n");
#endif

				do {
					sleep (1);
				}	while (atomic_read(&fills_oberts));

#ifdef DEBUG
				printf ("cdx_client: Tots els fills tancats. Acabar\n\n");
#endif
				exit (0);


			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda senyal close (%d)\n",CDX_SENYAL_PETICIO_CLOSE);
#endif
			}

		break;

		case CDX_SENYAL_PETICIO_ALTRES:
			atomic_inc(&rebuda_senyal_altres);
			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_client: Rebuda senyal altres (%d)\n",CDX_SENYAL_PETICIO_ALTRES);
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda senyal altres (%d)\n",CDX_SENYAL_PETICIO_ALTRES);
#endif
			}

		break;

		case SIGCHLD:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_client: Rebuda senyal SIGCHLD\n");
#endif
				//Recuperar proces zombie
				do {
					status=waitpid(-1,NULL,WNOHANG | WUNTRACED);
					if (status>0) {
						atomic_dec(&fills_oberts);
#ifdef DEBUG
					printf ("cdx_client: Recuperat estat fill terminat\n");
#endif
					}
				} while (status>0);

			}

			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda senyal SIGCHLD\n\n");
#endif
			}

		break;

		case SIGHUP:

			if (pid) { //Codi pel pare
#ifdef DEBUG
				printf ("cdx_client: Rebuda senyal SIGHUP\n\n");
#endif
			}
			else { //Codi pel fill
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda senyal SIGHUP\n\n");
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
	if (signal(CDX_SENYAL_PETICIO_OPEN,tractament_senyals)==SIG_ERR) {
	  printf ("cdx_client: Error al registrar senyal open\n");
		exit(-1);
	}

	if (signal(CDX_SENYAL_PETICIO_CLOSE,tractament_senyals)==SIG_ERR) {
	  printf ("cdx_client: Error al registrar senyal close\n");
		exit(-1);
	}

	if (signal(CDX_SENYAL_PETICIO_ALTRES,tractament_senyals)==SIG_ERR) {
	  printf ("cdx_client: Error al registrar senyal altres\n");
		exit(-1);
	}

	if (signal(SIGCHLD,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGCHLD\n");
		exit(-1);
	}

	if (signal(SIGHUP,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGHUP\n");
		exit(-1);
	}

	if (signal(SIGTERM,tractament_senyals)==SIG_ERR) {
		printf ("cdx_client: Error al registrar senyal SIGTERM\n");
		exit(-1);
	}


}

extern void proces_fill(void);

int main (void) {
	int linia_nova;	//usat en la lectura del fitxer de configuracio
	int i;						//usat en la lectura del fitxer de configuracio
	char *p;				//punter usat en la transmissio de dades

	atomic_set(&fills_oberts,0);

	printf ("\n"
			"Controlador de dispositius en xarxa\n"
			"\n"
			"Treball de fi de carrera\n"
			"Alumne: Cesar Hernandez Baño\n"
			"Carrera: Enginyeria Tecnica Informatica de Sistemes\n"
			"\n"
			"Programa client de cdx\n"
			"\n");

	//Obrir fitxer configuracio
	if ((f_configuracio=fopen(CDX_IMPORTA,"r"))==NULL) {
	  printf ("cdx_client: Fitxer %s no trobat!\n",CDX_IMPORTA);
		exit(1);
  }

	//Llegir Dades
	dispositius=0;
	do {
	  linia_nova=1;
	  i=cdx_llegeix_n_camps(f_configuracio,&linia_nova,3,s0,s1,s2);
		if (i<=0) {
		  if (i==CDX_NO_MES_DADES) printf ("cdx_client: No hi ha més dades. OK\n");
		  if (i==CDX_FALTEN_PARAMETRES) {
			  printf ("cdx_client: Falten camps a la linia\n");
				exit(i);
      }
			if (i==CDX_SOBREN_PARAMETRES) {
			  printf ("cdx_client: Sobren parametres\n");
				exit(i);
      }
    }
		else {
		  taula_importacio[dispositius].minor=atoi(s0);
		  strcpy(taula_importacio[dispositius].host,s1);
		  strcpy(taula_importacio[dispositius].device,s2);
		  printf ("cdx_client: Minor: %d Host:%s Device:%s\n",taula_importacio[dispositius].minor,
	            taula_importacio[dispositius].host,taula_importacio[dispositius].device);
      dispositius++;
    }

	} while (!feof(f_configuracio));
	fclose(f_configuracio);

	printf ("cdx_client: Tots els dispositius CDX tenen un major igual a %d\n",CDX_MAJOR);
	printf ("cdx_client: El valor minor pel control del CDX té un valor igual a %d\n",CDX_CONTROL);

	//A partir d'aqui, hem de registrar el client

	//Obrir dispositiu de control CDX
	if ((f_control=open("/dev/cdx/cdx",O_RDWR))<0) {
    printf ("cdx_client: Error al obrir dispositiu\n");
    exit (-1);
  }

	//Enviar cadena identificativa

	if (ioctl(f_control,CDX_IOCTL_REGISTRAR_CLIENT,&identificacio)<0) {
    printf ("cdx_client: Error al enviar cadena identificativa %d\n",errno);
		exit (-1);
  }

	printf ("cdx_client: Client Registrat\n");
	atomic_set(&rebuda_senyal_open,0); //=0;
	rebuda_senyal_close=0;
	atomic_set(&rebuda_senyal_altres,0);
	registra_senyals();


	//A partir d'aqui, esperar senyal SIGALARM
	do {
		if (!atomic_read(&rebuda_senyal_open)) pause();
		if (atomic_read(&rebuda_senyal_open)) {
			atomic_dec(&rebuda_senyal_open);

		//Llegir valors de la peticio, minor, file
			if ( (ioctl(f_control,CDX_IOCTL_LLEGIR_OPEN,
				&buffer_petit)) <0) {
				printf ("cdx_client: Error al llegir la peticio open\n");
			}

			else {

				p=&buffer_petit[0];
				file=VALOR_file(p);
				minor=VALOR_unsigned_char(&buffer_petit[4]);
				flags=VALOR_unsigned_int(&buffer_petit[5]);
				mode=VALOR_unsigned_int(&buffer_petit[9]);
				printf ("cdx_client: Peticio open: file=%p minor=%d flags:%d mode:%o\n",
					file,
					(int)minor,flags,mode);
				//Creem el fill
				//Aqui sorgeix un problema. El pare ha de crear el proces fill,
				//i despres avisar el modul cdx que el registri. Pero desde el
				//moment que crea el fill, s'executa, i no ha de fer res
				//abans que estigui registrat pel pare. Per tant, no ha d'executar
				//res fins que no rebi una senyal, i la rebra quan estigui registrat

				atomic_inc(&fills_oberts);

				pid=fork();
				if (pid==0) { //Proces fill
					proces_fill();
				}

				printf ("cdx_client: Proces fill creat PID:%d\n",pid);
				//Avisar el modul del proces fill
				*(int *)(&buffer_petit[4])=pid;
				if ( ioctl(f_control,CDX_IOCTL_ASSIGNAR_FILL,&buffer_petit) <0) {
					printf ("cdx_client: Error al assignar fill. Matar-lo\n");

					//Matar proces fill creat
					kill(pid,SIGKILL);
				}

				kill (pid,CDX_SENYAL_PETICIO_OPEN);
				//Aqui s'acaben les feines del client pare

			}
		}
  } while (1);
}
