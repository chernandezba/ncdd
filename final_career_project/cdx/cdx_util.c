/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_util.c
* Rutines comunes a cdx_client i cdx_servidor
*
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#include "cdx_util.h"

#define DEBUG

char buffer[80];

char *cdx_llegeix_camp
(FILE *f, int *linia_nova)
//Retorna: -1 si hi ha error
//Sino, retorna un punter al text
{
	int i;
	char c;

	//llegir inici linia, si es #, seguent linia

	if (feof(f)) return (char *)-1;
	c=fgetc(f);
	if (*linia_nova) {
	  do {
		  if (c=='#') {
		    do {
			    //Saltar línia
			    if (feof(f)) return (char *)-1;
			    c=fgetc(f);
		    } while (c!='\n');
			  c=fgetc(f);
      }
			if (c=='\n') {
			  do {
					c=fgetc(f);
					if (feof(f)) {
#ifdef DEBUG
					  printf ("\ncdx_util: final lectura de fitxer\n");
#endif
						return (char *)-1;
					}
				} while (c=='\n');
			}
	  } while (c=='#' || c=='\n');
	}

	i=0;
	*linia_nova=0;

  //Saltar espais
	while (c==' ' || c=='\t') {
	  if (feof(f)) {
#ifdef DEBUG
			printf ("\ncdx_util: final lectura de fitxer\n");
#endif
			return (char *)-1;
		}
		c=fgetc(f);
  }

	//Llegir cadena
	do {

		if (feof(f) || c=='\n') {
			if (!i) return (char *)-1;
			else return buffer;
		}

		//Posar final de cadena
		buffer[i++]=c;
		buffer[i]=0;

		if (feof(f)) {
#ifdef DEBUG
			printf ("\ncdx_util: final lectura de fitxer\n");
			printf ("%d\n",c);
#endif
			return buffer;
		}

		c=fgetc(f);
		if (c=='\n') {
			*linia_nova=1;
			return buffer;
		}

	} while (c!=' ' && c!='\t');
	return buffer;
}

int cdx_llegeix_final_linia
(FILE *f)
//Retorna: 0 si no hi ha error, si hi ha, retorna -1
{
	char c;

	do {
		if (feof(f)) return 0;
		c=fgetc(f);
		if (c=='\n') return 0;
		if (c!='\t' && c!=' ') return -1;
	} while (1);
}

int cdx_llegeix_n_camps2
(FILE *f,int *linia_nova,int n, va_list args)
//Retorna:
//Numero de parametres llegits
//Si es -1, falten parametres
//Si es -2, sobren parametres
{
	int i;
	char *s,*s0;

	for (i=0;i<n;i++) {
		if (feof(f)) return CDX_NO_MES_DADES;

		s=cdx_llegeix_camp(f,linia_nova);

		if (s==(char *)-1 || (*linia_nova && !i) ) {
			if (!feof(f)) return CDX_FALTEN_PARAMETRES;
			return CDX_NO_MES_DADES;
		}


		s0=va_arg(args,char *);
		strcpy(s0,s);

	}

	if (!(*linia_nova)) {
		if (cdx_llegeix_final_linia(f)==-1) {
			return CDX_SOBREN_PARAMETRES;
		}
	}
	return i;

}

int cdx_llegeix_n_camps
(FILE *f,int *linia_nova, int n, ...)
{
	va_list args;
	int i;

	va_start(args, n);
	i=cdx_llegeix_n_camps2(f,linia_nova,n,args);
	va_end(args);
	return i;
}

//Crea un socket TCP per la connexió en xarxa
int crear_socket_TCP
(void)
{
	return socket(PF_INET,SOCK_STREAM,0);
}

//Omplir l'adreça del socket
//Host ha de ser NULL pel servidor
int omplir_adr_internet
(struct sockaddr_in *adr,char *host,unsigned short n_port)
{
	struct hostent *h;

	adr->sin_family=AF_INET;
	if (host!=NULL) {
		if ((h=gethostbyname(host))==NULL) return -1;
		adr->sin_addr=*(struct in_addr *)h->h_addr;
#ifdef DEBUG
		printf ("\ncdx_util: %s : %d = %lX\n",host,(int)n_port,(unsigned long)adr->sin_addr.s_addr);
#endif
	}
	else {
		adr->sin_addr.s_addr=htonl(INADDR_ANY);
  }
	adr->sin_port=htons(n_port);
	return 0;
}

//Assignar l'adreça al socket
//Host ha de valer NULL si es tracta del servidor
int assignar_adr_internet
(int sock,char *host,unsigned short n_port)
{
	struct sockaddr_in adr;

	if (omplir_adr_internet(&adr,host,n_port)<0) return -1;
	return bind(sock,(struct sockaddr *)&adr,sizeof(adr));
}

//Llegeix n bytes de un socket
//Retorna -1 si hi ha error
int llegir_socket(int sock, char *buffer, int longitut)
{
	int rebuts;
	int llegits;

	rebuts = 0;
	do {
		llegits = read(sock, buffer+rebuts, longitut-rebuts);
		if (llegits==-1) return -1;
		rebuts += llegits;
	}	while (rebuts<longitut);

	return 0;
}


//Llegeix 32 bits de un descriptor de fitxer, o sigui, retorna
//la longitut de la trama CDX
//Retorna -1 si hi ha error
unsigned int llegir_longitut(int sock)
{
	char buffer[4];

	if ( (llegir_socket(sock,buffer,4)) <0 ) {
#ifdef DEBUG
		printf ("\ncdx_util: error llegir_socket\n");
#endif
		return -1;
	}
	return *((unsigned int *)(&buffer[0]));
}

//Envia una trama CDX per un socket
//Envia devant la longitut
//Retorna -1 si hi ha error
int enviar_trama(int sock,char *buffer,int longitut)
{

	int n;

	n=longitut;

	if ( (write(sock,&n,4)) <0 ) return -1;
	if ( (write(sock,buffer,longitut)) <0 ) return -1;

	return 0;
}

//Funcio per assignar memoria, tenint un bloc assignat previ
//Si el bloc de memoria es menor que la longitut que es demana,s'assigna unbloc nou
//longitut_anterior ha de ser zero per la primera vegada que es crida a la funcio
//Si el bloc ha crescut de tamany, es cambia longitut_anterior
int assigna_memoria
(char **buffer,int *longitut_anterior,int longitut_demanada)
{

#ifdef DEBUG
	printf ("\ncdx_util: longitut anterior %d, longitut_demanada:%d\n",
				*longitut_anterior,longitut_demanada);
#endif
	if (longitut_demanada>*longitut_anterior) {
		if (*longitut_anterior) {
			free(*buffer);
#ifdef DEBUG
			printf ("\ncdx_util: Bloc liberat:%p\n",*buffer);
#endif
		}

		*buffer=malloc(longitut_demanada);
		if (*buffer==NULL) return -1;
#ifdef DEBUG
		printf ("\ncdx_util: Bloc assignat:%p\n",*buffer);
#endif
		*longitut_anterior=longitut_demanada;

	}
	return 0;
}

//Llegeix una trama CDX desde un socket.
//El buffer de destinacio es un bloc de memoria assignat, i si
//es menor que la longitut de la trama, s'assigna un bloc nou
//Longitut_buffer ha de ser zero per la primera vegada que es crida a la funcio
//Retorna -1 si hi ha error, o longitut de les dades llegides
int llegir_trama(int sock,char **buffer,int *longitut_buffer)
{
	int longitut;

	longitut=llegir_longitut(sock);
	if ( (assigna_memoria(buffer,longitut_buffer,longitut)) <0) return -1;

	if ((llegir_socket(sock,*buffer,longitut)) <0) return -1;

	return longitut;
}
