/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer ncdd_xarxa.h
* Definicions per la comunicacio en xarxa del client i servidor
*
*/


#ifndef NCDD_XARXA_H
#define NCDD_XARXA_H

//Port TCP al qual esta lligat el servidor
#define NCDD_PORT 33333

//Ordres de les trames NCDD
#define NCDD_TRAMA_IDENTIFICACIO 1
#define NCDD_TRAMA_RETORNA_IDENTIFICACIO 2
#define NCDD_TRAMA_OPEN 3
#define NCDD_TRAMA_RETORNA_OPEN 4
#define NCDD_TRAMA_CLOSE 5
#define NCDD_TRAMA_RETORNA_CLOSE 6
#define NCDD_TRAMA_READ 7
#define NCDD_TRAMA_RETORNA_READ 8
#define NCDD_TRAMA_WRITE 9
#define NCDD_TRAMA_RETORNA_WRITE 10
#define NCDD_TRAMA_IOCTL 11
#define NCDD_TRAMA_RETORNA_IOCTL 12
#define NCDD_TRAMA_LLSEEK 13
#define NCDD_TRAMA_RETORNA_LLSEEK 14

//Cadena identificativa que ha d'enviar el client al servidor
//al moment de conectar-se. S'usa per evitar connexions no correctes
//al port TCP, o per controlar diverses versions del protocol
#define NCDD_IDENTIFICACIO_XARXA "Protocol comunicacio de xarxa NCDD V1.0"

#endif
