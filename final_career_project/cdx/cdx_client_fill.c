/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_client_fill.c
* Programa client fill de cdx
*
*/


#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <asm/atomic.h>

#include "cdx.h"
#include "cdx_util.h"
#include "cdx_xarxa.h"

#define DEBUG



//senyals rebudes
extern atomic_t rebuda_senyal_open,rebuda_senyal_altres;
extern int rebuda_senyal_close;

extern unsigned char minor;			//numero minor del dispositiu a obrir
extern unsigned int flags;			//flags d'apertura
extern unsigned int mode;				//mode d'apertura
extern int dispositius;					//numero total de dispositius
extern int f_control;						//fitxer dispositiu CDX
extern char buffer_petit[300];	//buffer petit de comunicacio

//Vector de dispositius importats
extern struct t_taula_importacio taula_importacio[CDX_MAX_DISPOSITIUS];

//Estructura de la taula de dispositius importats
struct t_taula_importacio {
	unsigned char minor;
	char host[255];
	char device[255];
};


struct sockaddr_in adr;	//adreça de connexio
int sock;								//socket de connexió
char *host;							//nom de l'ordinador a connectar-se
char *device;						//nom del dispositiu a obrir en l'ordinador servidor
unsigned short port;		//port de connexio CDX

int error_proces,error_client; //errors rebuts del client


/* Buffer d'escriptura de dades
 * es va ampliant segons convingui amb
 * la funcio assigna_memoria
*/
char *buffer_gran;
int longitut_buffer_gran=0;

//Cadena identificativa usada en la connexio amb el servidor CDX
char identificacio_xarxa[]=CDX_IDENTIFICACIO_XARXA;

//Retorna el resultat al modul cdx de les operacions open,write,close,ioctl
int retornar_valor_cdx
(int ioctl_num,int error)
{

	VALOR_int(&buffer_petit[0])=error;
	if ( (ioctl(f_control,ioctl_num,&buffer_petit))<0) {
		printf ("cdx_client_fill: Error al retornar valor ordre %X\n",ioctl_num);
		return(-1);
	}
	return 0;
}

//Retorna el resultat al modul cdx de l'operacio LLSEEK
int retornar_resultat_llseek
(int error)
{

	VALOR_long_long(&buffer_petit[0])=error;
	if ( (ioctl(f_control,CDX_IOCTL_RETORNA_LLSEEK,&buffer_petit))<0) {
		printf ("cdx_client_fill: Error al retornar valor ordre LLSEEK\n");
		return(-1);
	}
	return 0;
}


//Retorna el resultat al modul cdx de l'operacio read o ioctl
int retornar_valor_read_ioctl
(int ordre,int error,char *p)
{

	int longitut;

	longitut=error;

	VALOR_int(&buffer_petit[0])=error;
	VALOR_unsigned_int(&buffer_petit[4])=(unsigned int)p;

	if ( (ioctl(f_control,ordre,&buffer_petit))<0) {
		printf ("cdx_client_fill: Error al retornar valor read/ioctl\n");
		return -1;
	}
	return 0;

}

//Tancar el dispositiu
void tancar_dispositiu
(void)
{

	int longitut;

	//Enviar ordre close
	buffer_petit[0]=CDX_TRAMA_CLOSE;
	longitut=1;

	if ( (enviar_trama(sock,buffer_petit,longitut)) <0) {
		printf ("cdx_client_fill: Error al enviar trama CLOSE : \n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_CLOSE,
											-EIO);
		perror("");
		exit(-1);
	}


	if (llegir_trama(sock,&buffer_gran,&longitut_buffer_gran)<0) {
		printf ("cdx_client_fill: Error al rebre resultat close\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_CLOSE,
											-EIO);
		exit(-1);
	}
	error_proces=VALOR_int(&buffer_gran[1]);
	error_client=VALOR_int(&buffer_gran[5]);
#ifdef DEBUG
	printf ("cdx_client_fill: Resultat close: error_proces: %d error_client: %d\n",
					error_proces,error_client);
#endif

	retornar_valor_cdx(CDX_IOCTL_RETORNA_CLOSE,
											error_proces);

}

//Llegir del dispositiu
int llegir_dispositiu
(void)
{
	int longitut;

	longitut=VALOR_unsigned_int(&buffer_petit[1]);
#ifdef DEBUG
	printf ("cdx_client_fill: Ordre read, longitut: %u\n\n",
					longitut);
#endif

	//Enviar ordre read
	buffer_petit[0]=CDX_TRAMA_READ;
	VALOR_unsigned_int(&buffer_petit[1])=longitut;
	longitut=5;

	if ( (enviar_trama(sock,buffer_petit,longitut)) <0) {
		printf ("cdx_client_fill: Error al enviar trama READ : \n");
		retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_READ,-EIO,buffer_gran+9);
		//encara que buffer_gran+9 no contingui res, al retornar al modul, com hi ha
		//hagut error, no es traspassaran les dades
		perror("");
		return -1;
	}

	if ((longitut=llegir_trama(sock,&buffer_gran,&longitut_buffer_gran))<0) {
		printf ("cdx_client_fill: Error al rebre resultat read\n");
		error_proces=-EIO;
		error_client=0;
	}
	else {
#ifdef DEBUG
		printf ("cdx_client_fill: Rebut %d dades de read\n",longitut);
#endif
		error_proces=VALOR_int(&buffer_gran[1]);
		error_client=VALOR_int(&buffer_gran[5]);
	}

#ifdef DEBUG
	printf ("cdx_client_fill: Resultat read: error_proces: %d error_client: %d\n",
					error_proces,error_client);
#endif


	retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_READ,error_proces,buffer_gran+9);
	return 0;

}

//Escriure al dispositiu
int escriure_dispositiu(void)
{
	int longitut;

	longitut=VALOR_unsigned_int(&buffer_petit[1]);
#ifdef DEBUG
	printf ("cdx_client_fill: Ordre write, longitut: %u\n\n",
					longitut);
#endif

	//Assignar bloc de dades per la ordre write
	//1 byte per la ordre, 4 bytes per la longitut, N dades

	if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,longitut+5)<0) {
		printf ("cdx_client_fill: Error al assignar buffer d'escriptura\n");
		//retornar error
		retornar_valor_cdx(CDX_IOCTL_RETORNA_WRITE,-EIO);
		perror("");
		return -1;
	}

#ifdef DEBUG
	printf ("cdx_client_fill: Escrivim %u bytes\n",longitut);
#endif

	//Enviar ordre write
	buffer_gran[0]=CDX_TRAMA_WRITE;
	VALOR_unsigned_int(&buffer_gran[1])=longitut;


	if (ioctl(f_control,CDX_IOCTL_LLEGIR_WRITE,&buffer_gran[5]) <0) {
		printf ("cdx_client_fill: Error al llegir peticio write.\n");
		//retornar error
		retornar_valor_cdx(CDX_IOCTL_RETORNA_WRITE,-EIO);
		perror("");
		return -1;
	}


	if ( (enviar_trama(sock,buffer_gran,longitut+5)) <0) {
		printf ("cdx_client_fill: Error al enviar trama WRITE : \n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_WRITE,-EIO);
		perror("");
		return -1;
	}


	if ((longitut=llegir_trama(sock,&buffer_gran,&longitut_buffer_gran))<0) {
		printf ("cdx_client_fill: Error al rebre resultat write\n");
		error_proces=-EIO;
		error_client=0;
	}
	else {
#ifdef DEBUG
		printf ("cdx_client_fill: Rebut %d dades de write\n",longitut);
#endif
		error_proces=VALOR_int(&buffer_gran[1]);
		error_client=VALOR_int(&buffer_gran[5]);
	}

#ifdef DEBUG
	printf ("cdx_client_fill: Resultat write: error_proces: %d error_client: %d\n",
					error_proces,error_client);
#endif


	retornar_valor_cdx(CDX_IOCTL_RETORNA_WRITE,error_proces);
	return 0;

}

//Ordre IOCTL al dispositiu
int ioctl_dispositiu(void)
{
	unsigned int cmd,size,direction,llegir_ioctl=0;
	int longitut;

	cmd=VALOR_unsigned_int(&buffer_petit[1]);
#ifdef DEBUG
	printf ("cdx_client_fill: Ordre ioctl, cmd: %X\n",
					cmd);
#endif

	//LLegir bytes del modul cdx (si fa falta)
	direction=_IOC_DIR(cmd);
	size=_IOC_SIZE(cmd);

	if (direction==_IOC_NONE) {
		llegir_ioctl=1;
		size=4;
	}

	else if (direction==_IOC_WRITE || direction==(_IOC_WRITE | _IOC_READ) ) {
					if (size>0) {
						llegir_ioctl=1;
					}
			}

	if (!llegir_ioctl) size=0;


	if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,size+5)<0) {
		printf ("cdx_client_fill: Error al assignar buffer de ioctl\n");
		retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_IOCTL,-EIO,buffer_gran+9);
		//retornar error
		return -1;
	}


	//Enviar ordre ioctl
	buffer_gran[0]=CDX_TRAMA_IOCTL;
	VALOR_unsigned_int(&buffer_gran[1])=cmd;

	if (size) {
		if (ioctl(f_control,CDX_IOCTL_LLEGIR_IOCTL,&buffer_gran[5]) <0) {
			printf ("cdx_client_fill: Error al llegir peticio ioctl.\n");
			retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_IOCTL,-EIO,buffer_gran+9);
			//retornar error
			return -1;

		}
	}

	if (direction==_IOC_NONE) {
#ifdef DEBUG
		printf ("cdx_client_fill: arg=%u\n",VALOR_unsigned_int(&buffer_gran[5]));
#endif
	}

	//Enviar dades al servidor

	if ( (enviar_trama(sock,buffer_gran,size+5)) <0) {
	  printf ("cdx_client_fill: Error al enviar trama IOCTL : \n");
	  retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_IOCTL,-EIO,buffer_gran+9);
	  perror("");
	}

	//Rebre dades

	if ((longitut=llegir_trama(sock,&buffer_gran,&longitut_buffer_gran))<0) {
		printf ("cdx_client_fill: Error al rebre resultat ioctl\n");
		error_proces=-EIO;
		error_client=0;
	}
	else {
#ifdef DEBUG
		printf ("cdx_client_fill: Rebut %d dades de ioctl\n",longitut);
#endif
		error_proces=VALOR_int(&buffer_gran[1]);
		error_client=VALOR_int(&buffer_gran[5]);
	}

#ifdef DEBUG
	printf ("cdx_client_fill: Resultat ioctl: error_proces: %d error_client: %d\n",
					error_proces,error_client);
#endif

	//Retornar resultat al cdx

	retornar_valor_read_ioctl(CDX_IOCTL_RETORNA_IOCTL,error_proces,buffer_gran+9);
	return 0;

}

//Funcio de posicionament del punter de lectura/escriptura
int llseek_dispositiu(void)
{

	int whence;
	long long offset;
	int longitut;

	offset=VALOR_long_long(&buffer_petit[1]);
	whence=VALOR_int(&buffer_petit[1+8]);
#ifdef DEBUG
	printf ("cdx_client_fill: Ordre llseek offset: %Ld, whence:%d\n",
					offset,whence);
#endif


	//Enviar ordre llseek. El format de la trama de xarxa i
	//el de la ordre llegida del modul cdx es practicament el mateix, nomes
	//varia el primer byte

	buffer_petit[0]=CDX_TRAMA_LLSEEK;

	if ( (enviar_trama(sock,buffer_petit,1+8+4)) <0) {
		printf ("cdx_client_fill: Error al enviar trama LLSEEK : \n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_LLSEEK,
											-EIO);
		perror("");
		exit(-1);
	}


	//Rebre dades

	if ((longitut=llegir_trama(sock,&buffer_gran,&longitut_buffer_gran))<0) {
		printf ("cdx_client_fill: Error al rebre resultat LLSEEK\n");
		error_proces=-EIO;
		error_client=0;
	}

	else {
#ifdef DEBUG
		printf ("cdx_client_fill: Rebut %d dades de llseek\n",longitut);
#endif
		offset=VALOR_long_long(&buffer_gran[1]);
		error_client=VALOR_int(&buffer_gran[1+8]);
	}

#ifdef DEBUG
	printf ("cdx_client_fill: Resultat llseek: error_proces: %Ld error_client: %d\n",
					offset,error_client);
#endif

	//Retornar resultat al cdx

	retornar_resultat_llseek(offset);
	return 0;

}

//Enviar cadena identificativa del protocol de xarxa CDX
void identificar_protocol(void)
{

	if (assigna_memoria(&buffer_gran,&longitut_buffer_gran,2+strlen(identificacio_xarxa))<0) {
		printf ("cdx_client_fill: Error al assignar buffer d'identificacio\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit (-1);
	}


	buffer_gran[0]=CDX_TRAMA_IDENTIFICACIO;
	strcpy(&buffer_gran[1],identificacio_xarxa);

#ifdef DEBUG
	printf ("cdx_client_fill: Identifiquem protocol de xarxa...\n");
#endif

	if ( (enviar_trama(sock,buffer_gran,2+strlen(identificacio_xarxa) )) <0) {
		printf ("cdx_client_fill: Error al enviar trama IDENTIFICACIO : \n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		perror("");
		exit (-1);
	}

	//Llegir resultat de la identificacio
	if (llegir_trama(sock,&buffer_gran,&longitut_buffer_gran)<0) {
		printf ("cdx_client_fill: Error al rebre resultat IDENTIFICACIO\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit (-1);
	}

	if (buffer_gran[1]==0) {
		printf ("cdx_client_fill: Protocol de xarxa CDX diferents!\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit (-1);
	}
#ifdef DEBUG
	printf ("cdx_client_fill: Protocol de xarxa CDX correcte.\n");
#endif

}

void proces_fill(void) {
	int disp;
	int longitut;
	char tipus_peticio;

	host=NULL;

#ifdef DEBUG
	printf ("cdx_client_fill: Esperant assignar fill\n");
#endif

	while (!atomic_read(&rebuda_senyal_open));


	port=CDX_PORT;
#ifdef DEBUG
	printf ("cdx_client_fill: Peticio open: minor=%d flags:%d mode:%o\n",
					(int)minor,flags,mode);
#endif

	//Buscar minor a la taula de dispositius
	for (disp=0;disp<dispositius && host==NULL;disp++) {
		if (taula_importacio[disp].minor==minor) {
			host=taula_importacio[disp].host;
			device=taula_importacio[disp].device;
#ifdef DEBUG
			printf ("cdx_client_fill: Obrir peticio host:%s device:%s\n",
							taula_importacio[disp].host,taula_importacio[disp].device);
#endif
		}
	}

	if (host==NULL) {
		printf ("cdx_client_fill: Dispositiu no trobat\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit(-1);
	}

	if ((sock=crear_socket_TCP())<0) {
		printf ("cdx_client_fill: Error al crear socket TCP!\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit(-1);
	}

	if (omplir_adr_internet(&adr,host,port)<0) {
		printf ("cdx_client_fill: Error al omplir adeca internet\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		exit(-1);
	}

	if (connect(sock,(struct sockaddr *)&adr,sizeof(adr))<0) {
		printf ("cdx_client_fill: Error al establir connexio amb %s:\n",host);
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		perror("");
		exit(-1);
	}

	//Demanar si el client i servidor usen la mateixa versio de protocol
	identificar_protocol();

	//Intercanvi de dades
	//Enviar ordre open
	buffer_petit[0]=CDX_TRAMA_OPEN;
	strcpy(&buffer_petit[1],device);

	longitut=1+strlen(device)+1;

#ifdef DEBUG
	printf ("\n(%s)\n",device);
#endif

	VALOR_unsigned_int(&buffer_petit[longitut])=flags;
	longitut +=4;

	VALOR_unsigned_int(&buffer_petit[longitut])=mode;
	longitut+=4;

	if ( (enviar_trama(sock,buffer_petit,longitut)) <0) {
		printf ("cdx_client_fill: Error al enviar trama OPEN : \n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		perror("");
		exit(-1);
	}

	if (llegir_trama(sock,&buffer_gran,&longitut_buffer_gran)<0) {
		printf ("cdx_client_fill: Error al rebre resultat open\n");
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											-ENODEV);
		perror("");
		exit(-1);
	}
	error_proces=VALOR_int(&buffer_gran[1]);
	error_client=VALOR_int(&buffer_gran[5]);

#ifdef DEBUG
	printf ("cdx_client_fill: Resultat open: error_proces: %d error_client: %d\n",
					error_proces,error_client);
#endif

	retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											error_proces);

	if (error_proces<0) {
		//Terminar
#ifdef DEBUG
		printf ("cdx_client_fill: Error al obrir. Terminat\n");
#endif
		retornar_valor_cdx(CDX_IOCTL_RETORNA_OPEN,
											error_proces);
		exit (-1);
	}

	do {

		if (rebuda_senyal_close) {
#ifdef DEBUG
			printf ("cdx_client_fill: Tanquem dispositiu %s\n",device);
#endif
			tancar_dispositiu();
#ifdef DEBUG
			printf ("cdx_client_fill: Terminat\n");
#endif
			close (sock);
			exit(0);
		}

		if (atomic_read(&rebuda_senyal_altres)) {
			atomic_dec(&rebuda_senyal_altres);

			//Aqui tant pot ser read, write, ioctl,...

			if (ioctl(f_control,CDX_IOCTL_LLEGIR_ALTRES,&buffer_petit) <0) {
				printf ("cdx_client_fill: Error al llegir peticio.\n");
			}

			else {
				tipus_peticio=buffer_petit[0];
#ifdef DEBUG
				printf ("cdx_client_fill: Rebuda peticio %d\n",tipus_peticio);
#endif
				switch (tipus_peticio) {
					case CDX_ORDRE_READ:
						llegir_dispositiu();
					break;
					case CDX_ORDRE_WRITE:
						escriure_dispositiu();
					break;
					case CDX_ORDRE_IOCTL:
						ioctl_dispositiu();
					break;
					case CDX_ORDRE_LLSEEK:
						llseek_dispositiu();
					break;
					default:
						printf ("cdx_client_fill: Peticio desconeguda\n");
					break;
				}
			}

		}

		if (!atomic_read(&rebuda_senyal_altres) && !rebuda_senyal_close) {
			do {
				pause();
			} while (!atomic_read(&rebuda_senyal_altres) && !rebuda_senyal_close);
		}

	} while (1==1);


}

