/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx_xarxa.h
* Definicions per la comunicacio en xarxa del client i servidor
*
*/


#ifndef CDX_XARXA_H
#define CDX_XARXA_H

//Port TCP al qual esta lligat el servidor
#define CDX_PORT 33333

//Ordres de les trames CDX
#define CDX_TRAMA_IDENTIFICACIO 1
#define CDX_TRAMA_RETORNA_IDENTIFICACIO 2
#define CDX_TRAMA_OPEN 3
#define CDX_TRAMA_RETORNA_OPEN 4
#define CDX_TRAMA_CLOSE 5
#define CDX_TRAMA_RETORNA_CLOSE 6
#define CDX_TRAMA_READ 7
#define CDX_TRAMA_RETORNA_READ 8
#define CDX_TRAMA_WRITE 9
#define CDX_TRAMA_RETORNA_WRITE 10
#define CDX_TRAMA_IOCTL 11
#define CDX_TRAMA_RETORNA_IOCTL 12
#define CDX_TRAMA_LLSEEK 13
#define CDX_TRAMA_RETORNA_LLSEEK 14

//Cadena identificativa que ha d'enviar el client al servidor
//al moment de conectar-se. S'usa per evitar connexions no correctes
//al port TCP, o per controlar diverses versions del protocol
#define CDX_IDENTIFICACIO_XARXA "Protocol comunicacio de xarxa CDX V1.0"

#endif
