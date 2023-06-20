/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx.h
* Definicions usades en la comunicacio entre el modul i el client
*
*/


#ifndef CDX_H
#define CDX_H

#include <linux/ioctl.h>

//Fitxer de dispositius que l'ordinador importa (pel client)
//#define CDX_IMPORTA "/etc/cdx_importa"
#define CDX_IMPORTA "cdx_importa"

//Fitxer de dispositius que l'ordinador exporta (pel servidor)
//(NO IMPLEMENTAT)
//#define CDX_EXPORTA "/etc/cdx_exporta"
#define CDX_EXPORTA "cdx_exporta"

//Numero major del modul del kernel, que comparteixen tots els dispositius
//importats i el de control
#define CDX_MAJOR 254

//Numero maxim de dispositius que es poden importar
#define CDX_MAX_DISPOSITIUS 250

//Numero maxim de peticions simultanies que es poden atendre
#define CDX_MAX_PETICIONS_SIMULTANIES 512

//Numero minor de control del modul del kernel (pel client)
#define CDX_CONTROL CDX_MAX_DISPOSITIUS

//Numero maxim de clients fill
#define CDX_MAX_CLIENTS_FILL 64

//Tamany del buffer d'intercanvi de cada client per les ordres
//read, write, ioctl
#define CDX_TAMANY_BUFFER_INTERCANVI 16384

//Cadena identificativa que ha d'enviar el client al modul del kernel
//al moment d'iniciar-se, per registrar-se. S'usa per evitar
//que accidentalment obrim el dispositiu i registrem algun programa que
//no sigui client, o per controlar diverses versions del protocol
#define CDX_IDENTIFICACIO "Protocol comunicacio amb modul CDX V1.0"

//Estat del modul (registrat o no)
#define CDX_ESTAT_CONTROL_INICI 0
#define CDX_ESTAT_CONTROL_REGISTRAT 1

//Valors de les ordres a realitzar amb dispositius
//S'envia entre el modul i el client
#define CDX_ORDRE_OPEN 1
#define CDX_ORDRE_CLOSE 2
#define CDX_ORDRE_READ 3
#define CDX_ORDRE_WRITE 4
#define CDX_ORDRE_IOCTL 5
#define CDX_ORDRE_LLSEEK 6

//Longituts i formats per les ordres de comunicacio entre CDX i client
//Ordre per llegir parametres d'una operacio open
#define CDX_LONGITUT_OPEN 4+1+4+4

	//Int: file *file
	//Byte: minor
	//Int: flags
	//Int: mode

//Dades de retorn d'una operacio open
#define CDX_LONGITUT_RETORNA_OPEN 4

	//Int: valor de retorn pel proces

#define CDX_LONGITUT_RETORNA_CLOSE 4
	//Int: valor de retorn pel proces

//Ordre per llegir parametres d'una operacio read
#define CDX_LONGITUT_READ 1+4
	//Byte: CDX_ORDRE_READ
	//Int: longitut a llegir

//Ordre per retornar parametres d'una operacio read
#define CDX_LONGITUT_RETORNA_READ 4+4
	//Int: valor de retorn pel proces (si >0, longitut llegida)
	//Int: direccio on estan les dades llegides

//Ordre per llegir parametres d'una operacio write
#define CDX_LONGITUT_WRITE 1+4
	//Byte: CDX_ORDRE_WRITE
	//Int: longitut a escriure

//Ordre per retornar parametres d'una operacio write
#define CDX_LONGITUT_RETORNA_WRITE 4
	//Int: valor de retorn pel proces (si >0, longitut escrita)

#define CDX_LONGITUT_IOCTL 1+4
	//Byte: CDX_ORDRE_IOCTL
	//Int: comanda ioctl (cmd)

//Ordre per retornar parametres d'una operacio ioctl
#define CDX_LONGITUT_RETORNA_IOCTL 4+4
	//Int: valor de retorn pel proces
	//Int: direccio on estan les dades llegides

#define CDX_LONGITUT_LLSEEK 1+8+4
	//Byte: CDX_ORDRE_LLSEEK
	//64 bits: offset
	//32 bits: whence

#define CDX_LONGITUT_RETORNA_LLSEEK 8
	//64 bits: valor de retorn pel proces


//IOCTLS per comunicacio entre client i el modul
#define CDX_IOCTL_MAGIC 0xCD

#define CDX_IOCTL_REGISTRAR_CLIENT _IO(CDX_IOCTL_MAGIC,1)
#define CDX_IOCTL_LLEGIR_OPEN _IO(CDX_IOCTL_MAGIC,2)
#define CDX_IOCTL_ASSIGNAR_FILL _IO(CDX_IOCTL_MAGIC,3)
#define CDX_IOCTL_RETORNA_OPEN _IO(CDX_IOCTL_MAGIC,4)
#define CDX_IOCTL_RETORNA_CLOSE _IO(CDX_IOCTL_MAGIC,5)

#define CDX_IOCTL_LLEGIR_ALTRES _IO(CDX_IOCTL_MAGIC,6)
//lectura de peticions read, write, ioctl,...

#define CDX_IOCTL_RETORNA_READ _IO(CDX_IOCTL_MAGIC,7)
#define CDX_IOCTL_LLEGIR_WRITE _IO(CDX_IOCTL_MAGIC,8)
#define CDX_IOCTL_RETORNA_WRITE _IO(CDX_IOCTL_MAGIC,9)

#define CDX_IOCTL_LLEGIR_IOCTL _IO(CDX_IOCTL_MAGIC,10)
#define CDX_IOCTL_RETORNA_IOCTL _IO(CDX_IOCTL_MAGIC,11)
#define CDX_IOCTL_RETORNA_LLSEEK _IO(CDX_IOCTL_MAGIC,12)

//SIGCHLD no s'ha de definir mai, s'utilitza per eliminar fills "zombies"
#define CDX_SENYAL_PETICIO_OPEN SIGUSR1
#define CDX_SENYAL_PETICIO_CLOSE SIGINT //QUIT //TERM
#define CDX_SENYAL_PETICIO_ALTRES SIGUSR2
//SIGINT=CTRL+C. Capturar-la i:
//si es el fill, fer-la com una peticio close
//si es el pare, esperar a tancar tots els fills

#define VALOR_unsigned_char(p) (*((unsigned char *)(p)) )
#define VALOR_unsigned_int(p) (*((unsigned int *)(p)) )
#define VALOR_int(p) (*((int *)(p)) )
#define VALOR_file(f) (*(struct file **)(f))
#define VALOR_long_long(p) (*((long long *)(p)) )

#endif
