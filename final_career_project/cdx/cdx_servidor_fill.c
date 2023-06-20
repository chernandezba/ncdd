/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_servidor_fill.c
* Programa servidor fill de cdx
*
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>

#include "cdx.h"
#include "cdx_xarxa.h"
#include "cdx_util.h"

#define DEBUG

extern int sock_conectat; //usat en la connexió per xarxa
char buffer_petit[300];   //buffer temporal

//Paràmetres referents al dispositiu tractat
int flags,mode;
char *device;
int file_device;


//Buffer de lectura de dades
//es va ampliant segons convingui amb
//la funcio assigna_memoria
char *buffer_gran;
int longitut_buffer_gran=0;

char identificacio_xarxa[]=CDX_IDENTIFICACIO_XARXA;


//Es defineix la funcio present al sistema. Normalment, la funcio
//lseek treballa amb 32 bits, pero la ordre del dispositiu està preparada
//per 64 bits. Per tant, hem de definir explicitament el prototipus de
//la funcio
extern long long lseek64(int,long long, int);


//Retorna el resultat al client cdx de les operacions open,write,close,ioctl
void retornar_valor_xarxa
(unsigned char tipus_trama,int error_proces,int error_client)
{
		VALOR_unsigned_char(&buffer_petit[0])=tipus_trama;
		VALOR_int(&buffer_petit[1])=error_proces;
		VALOR_int(&buffer_petit[5])=error_client;

		if ( (enviar_trama(sock_conectat,buffer_petit,9))<0) {
			printf ("Error al retornar valor ordre %X\n",tipus_trama);
			exit(-1);
		}
}

//Retorna el resultat al client cdx de l'operacio llseek
void retornar_resultat_llseek
(long long error_proces,int error_client)
{
		VALOR_unsigned_char(&buffer_petit[0])=CDX_TRAMA_RETORNA_LLSEEK;
		VALOR_long_long(&buffer_petit[1])=error_proces;
		VALOR_int(&buffer_petit[1+8])=error_client;

		if ( (enviar_trama(sock_conectat,buffer_petit,1+8+4))<0) {
			printf ("Error al retornar valor ordre LLSEEK\n");
			exit(-1);
		}
}


//Retorna les dades del resultat al client cdx de la operacio read
void retornar_resultat_read
(int error_proces,int error_client,char *trama,int longitut_trama)
{
	VALOR_unsigned_char(trama)=CDX_TRAMA_RETORNA_READ;
	VALOR_int(&trama[1])=error_proces;
	VALOR_int(&trama[5])=error_client;

	if ( (enviar_trama(sock_conectat,trama,longitut_trama))<0) {
		printf ("Error al retornar valor ordre READ\n");
		exit(-1);
	}
}


void tancar_dispositiu(void)
{
	int retorn;

#ifdef DEBUG
	printf ("cdx_servidor_fill: Tancar dispositiu\n");
#endif

	retorn=close(file_device);
	retornar_valor_xarxa(CDX_TRAMA_RETORNA_OPEN,retorn,0);

#ifdef DEBUG
	printf ("cdx_servidor_fill: Terminat\n");
#endif

	close (sock_conectat);

	exit (-1);
}

void llegir_dispositiu(void)
{
	int retorn,longitut,longitut_llegida;

	longitut=VALOR_unsigned_int(&buffer_gran[1]);
#ifdef DEBUG
	printf ("cdx_servidor_fill: Ordre READ %u bytes\n",longitut);
#endif

	if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,longitut+9)<0) {
		printf ("cdx_servidor_fill: Error al assignar buffer de lectura\n");
		//retornar error
		retornar_valor_xarxa(CDX_TRAMA_RETORNA_READ,-EIO,-1);
		return;
	}
#ifdef DEBUG
	printf ("cdx_servidor_fill: Llegim %u bytes\n",longitut);
#endif

	longitut_llegida=read(file_device,&buffer_gran[9],longitut);

	if (longitut_llegida<0) {
#ifdef DEBUG
		printf ("cdx_servidor_fill: Error al llegir dades\n");
#endif
		retorn=-errno;
		longitut_llegida=0;
	}
	else {
#ifdef DEBUG
		printf ("cdx_servidor_fill: Longitut_llegida %u bytes\n\n",longitut_llegida);
#endif
		retorn=longitut_llegida;
	}

	retornar_resultat_read(retorn,0,buffer_gran,longitut_llegida+9);

}

void escriure_dispositiu(void)
{
	int retorn,longitut,longitut_escrita;

	longitut=VALOR_unsigned_int(&buffer_gran[1]);
#ifdef DEBUG
	printf ("cdx_servidor_fill: Ordre WRITE %u bytes\n",longitut);
#endif

	longitut_escrita=write(file_device,buffer_gran+5,longitut);

	if (longitut_escrita<0) {
#ifdef DEBUG
		printf ("cdx_servidor_fill: Error al escriure dades\n");
#endif
		retorn=-errno;
		longitut_escrita=0;
	}
	else {
#ifdef DEBUG
		printf ("cdx_servidor_fill: Longitut_escrita %u bytes\n\n",longitut_escrita);
#endif
		retorn=longitut_escrita;
	}

	retornar_valor_xarxa(CDX_TRAMA_RETORNA_WRITE,retorn,0);

}

//Retorna les dades del resultat al client cdx de la operacio ioctl
//Les dades comencen en trama+9
void retornar_resultat_ioctl
(int error_proces,int error_client,char *trama,int longitut_trama)
{
	VALOR_unsigned_char(trama)=CDX_TRAMA_RETORNA_IOCTL;
	VALOR_int(&trama[1])=error_proces;
	VALOR_int(&trama[5])=error_client;

	if ( (enviar_trama(sock_conectat,trama,longitut_trama))<0) {
		printf ("Error al retornar valor i dades ordre IOCTL\n");
		exit(-1);
	}
}


//Funció per tractar les ordres ioctl
void ioctl_dispositiu(void)
{
	int retorn,cmd,size,direction,arg;

	cmd=VALOR_unsigned_int(&buffer_gran[1]);
	size=_IOC_SIZE(cmd);
	direction=_IOC_DIR(cmd);
#ifdef DEBUG
	printf ("cdx_servidor_fill: Ordre IOCTL , cmd=%X, size=%u\n",cmd,size);
#endif

	if (direction==_IOC_NONE) {
		arg=VALOR_unsigned_int(&buffer_gran[5]);
#ifdef DEBUG
		printf ("cdx_servidor_fill: direction=_IOC_NONE, arg=%u\n",arg);
#endif
		retorn=ioctl(file_device,cmd,arg);
	}
	else if (direction==_IOC_WRITE) {
#ifdef DEBUG
		printf ("cdx_servidor_fill: direction=_IOC_WRITE\n");
#endif
		retorn=ioctl(file_device,cmd,&buffer_gran[5]);
	}

	else if (direction==(_IOC_WRITE | _IOC_READ) ) {
#ifdef DEBUG
		printf ("cdx_servidor_fill: direction=_IOC_WRITE | _IOC_READ\n");
#endif

		if (size>0) memcpy(buffer_petit,&buffer_gran[5],size);
		retorn=ioctl(file_device,cmd,buffer_petit);

		if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,size+9)<0) {
			printf ("cdx_servidor_fill: Error al assignar buffer de lectura ioctl\n");
			//retornar error
			exit(-1);
		}

		if (size>0) memcpy(&buffer_gran[9],buffer_petit,size);

	}

	else if (direction==_IOC_READ)  {
#ifdef DEBUG
		printf ("cdx_servidor_fill: direction=_IOC_READ\n");
#endif
		if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,size+9)<0) {
			printf ("cdx_servidor_fill: Error al assignar buffer de lectura ioctl\n");
			//retornar error
			exit(-1);
		}

		retorn=ioctl(file_device,cmd,&buffer_gran[9]);
	}


	if (retorn<0) {
#ifdef DEBUG
		printf ("cdx_servidor_fill: Error al fer ioctl\n");
#endif
		retorn=-errno;
	}
	else {
		retorn=0;
	}

	if (direction!=_IOC_READ && direction!=(_IOC_READ | _IOC_WRITE) ) {
		retornar_valor_xarxa(CDX_TRAMA_RETORNA_IOCTL,retorn,0);
	}

	else {
		retornar_resultat_ioctl(retorn,0,buffer_gran,size+9);
	}

}


//Funció per tractar l'ordre de posicionament
void llseek_dispositiu(void)
{
	long long retorn;
	int whence;
	long long offset;

	offset=VALOR_long_long(&buffer_gran[1]);
	whence=VALOR_int(&buffer_gran[1+8]);

#ifdef DEBUG
	printf ("cdx_servidor_fill: Ordre llseek offset: %Ld, whence:%d\n",
					offset,whence);
#endif

	retorn=lseek64(file_device,offset,whence);

	if (retorn<0) retorn=-errno;

#ifdef DEBUG
	printf ("cdx_servidor_fill: Resultat ordre lseek: %Ld\n",retorn);
#endif


	retornar_resultat_llseek(retorn,0);

}


//Funció d'identificació del protocol (registre del client)
void identificar_protocol(void)
{

	unsigned char valor_retorn;

	if (llegir_trama(sock_conectat,&buffer_gran,&longitut_buffer_gran)<0) {
		printf ("cdx_servidor_fill: Error al rebre dades : \n");
		perror("");
		exit(-1);
	}

	if (*buffer_gran!=CDX_TRAMA_IDENTIFICACIO) {
		//Trama rebuda erronia
		printf ("cdx_servidor_fill: Trama d'identificacio desconeguda\n");
		exit(-1);
	}

	if (strcmp(identificacio_xarxa,&buffer_gran[1])!=0) {
		//Retornar error
		printf ("cdx_servidor_fill: Protocol de connexio diferent!\n");
		valor_retorn=0;
	}
	else {
		//Retornar error
#ifdef DEBUG
		printf ("cdx_servidor_fill: Protocol de connexio correcte\n");
#endif
		valor_retorn=1;
	}

	//Retornar trama
	buffer_gran[0]=CDX_TRAMA_RETORNA_IDENTIFICACIO;
	buffer_gran[1]=valor_retorn;

	if ( (enviar_trama(sock_conectat,buffer_gran,2))<0) {
		printf ("cdx_servidor_fill: Error al retornar identificacio\n");
		exit(-1);
	}

	if (valor_retorn==0) exit(-1);

}

void proces_fill(void)
{
	int longitut;

	char *p;
	char trama;
	int valor_retorn;

	identificar_protocol();

	do {
	//Bucle connexio


		if ((longitut=llegir_trama(sock_conectat,&buffer_gran,&longitut_buffer_gran))<0) {
			printf ("cdx_servidor_fill: Error al rebre dades : \n");
			perror("");
			exit(-1);
		}
#ifdef DEBUG
		printf ("cdx_servidor_fill: Rebut %d dades\n",longitut);
#endif


		//Obrir dispositiu
		device=buffer_gran+1;
		p=buffer_gran+2+strlen(device);
		flags=VALOR_int(p);
		mode=VALOR_int(p+4);
#ifdef DEBUG
		printf ("cdx_servidor_fill: Dispositiu:%s \n",device);
		printf ("cdx_servidor_fill: Flags: %d, Mode: %o\n",flags,mode);
#endif


		file_device=open(device,flags,mode);
#ifdef DEBUG
		printf ("cdx_servidor_fill: Resultat open: %d\n",file_device);
#endif
		if (file_device<0) 	valor_retorn=-errno;
		else valor_retorn=0;


		retornar_valor_xarxa(CDX_TRAMA_RETORNA_OPEN,valor_retorn,0);

		do {
			if ((longitut=llegir_trama(sock_conectat,&buffer_gran,&longitut_buffer_gran))<0) {
				printf ("cdx_servidor_fill: Error al rebre dades : \n");
				perror("");
				exit(-1);
			}
			trama=buffer_gran[0];
#ifdef DEBUG
			printf ("cdx_servidor_fill: Rebut %d dades\n",longitut);
			printf ("cdx_servidor_fill: Rebuda trama %d\n",(int)trama);
#endif
			switch (trama) {
				case CDX_TRAMA_CLOSE:
					tancar_dispositiu();
				break;

				case CDX_TRAMA_READ:
#ifdef DEBUG
					printf ("cdx_servidor_fill: Llegir dades\n");
#endif
					llegir_dispositiu();

				break;

				case CDX_TRAMA_WRITE:
#ifdef DEBUG
					printf ("cdx_servidor_fill: Escriure dades\n");
#endif
					escriure_dispositiu();

				break;

				case CDX_TRAMA_IOCTL:
#ifdef DEBUG
					printf ("cdx_servidor_fill: IOCTL a dispositiu\n");
#endif
					ioctl_dispositiu();

				break;

				case CDX_TRAMA_LLSEEK:
#ifdef DEBUG
					printf ("cdx_servidor_fill: LLSEEK a dispositiu\n");
#endif
					llseek_dispositiu();

				break;


				default:
					printf ("cdx_servidor_fill: Trama %d desconeguda\n",(int)trama);
			  break;
			}

		} while (1);

		pause();

	} while (1);
}
