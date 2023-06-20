/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_util.h
* Definicions de rutines comunes a cdx_client i cdx_servidor
*
*/


#ifndef CDX_UTIL_H
#define CDX_UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>

//Funcio per llegir fitxers de configuracio
extern int cdx_llegeix_n_camps
  (FILE *f,int *linia_nova, int n, ...);

//Valors que retorna la funcio cdx_llegeix_n_camps
#define CDX_NO_MES_DADES 0
#define CDX_FALTEN_PARAMETRES -1
#define CDX_SOBREN_PARAMETRES -2

extern int crear_socket_TCP
  (void);
extern int omplir_adr_internet
  (struct sockaddr_in *adr,char *host,unsigned short n_port);
extern int assignar_adr_internet
  (int sock,char *host,unsigned short n_port);
extern int llegir_socket(int sock, char *buffer, int longitut);
extern unsigned int llegir_longitut(int sock);
extern int enviar_trama(int sock,char *buffer,int longitut);
extern int llegir_trama(int sock,char **buffer,int *longitut_buffer);
extern int assigna_memoria
(char **buffer,int *longitut_anterior,int longitut_nova);



#endif
