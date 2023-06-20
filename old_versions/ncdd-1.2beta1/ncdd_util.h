/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer ncdd_util.h
* Definicions de rutines comunes a ncdd_client i ncdd_servidor
*
*/


#ifndef NCDD_UTIL_H
#define NCDD_UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//Funcio per llegir fitxers de configuracio
extern int ncdd_llegeix_n_camps
  (FILE *f,int *linia_nova, int n, ...);

//Valors que retorna la funcio ncdd_llegeix_n_camps
#define NCDD_NO_MES_DADES 0
#define NCDD_FALTEN_PARAMETRES -1
#define NCDD_SOBREN_PARAMETRES -2

extern int comprovar_host_alies
(char *host,char *direccio_client,struct hostent *nom_client);


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
