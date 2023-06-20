/*

    ncdd

    Copyright (c) 2002 Cesar Hernandez <chernandezba@hotmail.com>

    This file is part of ncdd

    ncdd is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


*/

/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Ba�
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer ncdd.h
* Definicions usades en la comunicacio entre el modul i el client
*
*/


#ifndef NCDD_H
#define NCDD_H

#include <linux/ioctl.h>
#include "ncdd_date.h"

#define NCDD_VERSION "1.2beta2"

//Fitxer de dispositius que l'ordinador importa (pel client)
#define NCDD_IMPORTA "/etc/ncdd_imports"
//#define NCDD_IMPORTA "ncdd_importa"

//Fitxer de dispositius que l'ordinador exporta (pel servidor)
//(NO IMPLEMENTAT)
#define NCDD_EXPORTA "/etc/ncdd_exports"
//#define NCDD_EXPORTA "ncdd_exporta"

//Log files
#define NCDD_SERVIDOR_LOG "/var/log/ncdd_server.log"
#define NCDD_CLIENT_LOG "/var/log/ncdd_client.log"

//Numero major del modul del kernel, que comparteixen tots els dispositius
//importats i el de control
#define NCDD_MAJOR 251

//Minor devices to allocate
#define NCDD_MINORS 256

//Numero maxim de dispositius que es poden importar
#define NCDD_MAX_DISPOSITIUS 250

//Numero maxim de dispositius que es poden exportar
#define NCDD_MAX_DISPOSITIUS_EXPORTAR 500

//Numero maxim de peticions simultanies que es poden atendre
#define NCDD_MAX_PETICIONS_SIMULTANIES 512

//Numero minor de control del modul del kernel (pel client)
#define NCDD_CONTROL NCDD_MAX_DISPOSITIUS

//Numero maxim de clients fill
#define NCDD_MAX_CLIENTS_FILL 64

//Tamany del buffer d'intercanvi de cada client per les ordres
//read, write, ioctl
#define NCDD_TAMANY_BUFFER_INTERCANVI 16384

//Cadena identificativa que ha d'enviar el client al modul del kernel
//al moment d'iniciar-se, per registrar-se. S'usa per evitar
//que accidentalment obrim el dispositiu i registrem algun programa que
//no sigui client, o per controlar diverses versions del protocol
#define NCDD_IDENTIFICACIO "Protocol comunicacio amb modul NCDD V1.0"

//Estat del modul (registrat o no)
#define NCDD_ESTAT_CONTROL_INICI 0
#define NCDD_ESTAT_CONTROL_REGISTRAT 1

//Valors de les ordres a realitzar amb dispositius
//S'envia entre el modul i el client
#define NCDD_ORDRE_OPEN 1
#define NCDD_ORDRE_CLOSE 2
#define NCDD_ORDRE_READ 3
#define NCDD_ORDRE_WRITE 4
#define NCDD_ORDRE_IOCTL 5
#define NCDD_ORDRE_LLSEEK 6

//Longituts i formats per les ordres de comunicacio entre NCDD i client
//Ordre per llegir parametres d'una operacio open
#define NCDD_LONGITUT_OPEN 4+1+4+4

	//Int: file *file
	//Byte: minor
	//Int: flags
	//Int: mode

//Dades de retorn d'una operacio open
#define NCDD_LONGITUT_RETORNA_OPEN 4

	//Int: valor de retorn pel proces

#define NCDD_LONGITUT_RETORNA_CLOSE 4
	//Int: valor de retorn pel proces

//Ordre per llegir parametres d'una operacio read
#define NCDD_LONGITUT_READ 1+4
	//Byte: NCDD_ORDRE_READ
	//Int: longitut a llegir

//Ordre per retornar parametres d'una operacio read
#define NCDD_LONGITUT_RETORNA_READ 4+4
	//Int: valor de retorn pel proces (si >0, longitut llegida)
	//Int: direccio on estan les dades llegides

//Ordre per llegir parametres d'una operacio write
#define NCDD_LONGITUT_WRITE 1+4
	//Byte: NCDD_ORDRE_WRITE
	//Int: longitut a escriure

//Ordre per retornar parametres d'una operacio write
#define NCDD_LONGITUT_RETORNA_WRITE 4
	//Int: valor de retorn pel proces (si >0, longitut escrita)

#define NCDD_LONGITUT_IOCTL 1+4
	//Byte: NCDD_ORDRE_IOCTL
	//Int: comanda ioctl (cmd)

//Ordre per retornar parametres d'una operacio ioctl
#define NCDD_LONGITUT_RETORNA_IOCTL 4+4
	//Int: valor de retorn pel proces
	//Int: direccio on estan les dades llegides

#define NCDD_LONGITUT_LLSEEK 1+8+4
	//Byte: NCDD_ORDRE_LLSEEK
	//64 bits: offset
	//32 bits: whence

#define NCDD_LONGITUT_RETORNA_LLSEEK 8
	//64 bits: valor de retorn pel proces


//IOCTLS per comunicacio entre client i el modul
#define NCDD_IOCTL_MAGIC 0xCD

#define NCDD_IOCTL_REGISTRAR_CLIENT _IO(NCDD_IOCTL_MAGIC,1)
#define NCDD_IOCTL_LLEGIR_OPEN _IO(NCDD_IOCTL_MAGIC,2)
#define NCDD_IOCTL_ASSIGNAR_FILL _IO(NCDD_IOCTL_MAGIC,3)
#define NCDD_IOCTL_RETORNA_OPEN _IO(NCDD_IOCTL_MAGIC,4)
#define NCDD_IOCTL_RETORNA_CLOSE _IO(NCDD_IOCTL_MAGIC,5)

#define NCDD_IOCTL_LLEGIR_ALTRES _IO(NCDD_IOCTL_MAGIC,6)
//lectura de peticions read, write, ioctl,...

#define NCDD_IOCTL_RETORNA_READ _IO(NCDD_IOCTL_MAGIC,7)
#define NCDD_IOCTL_LLEGIR_WRITE _IO(NCDD_IOCTL_MAGIC,8)
#define NCDD_IOCTL_RETORNA_WRITE _IO(NCDD_IOCTL_MAGIC,9)

#define NCDD_IOCTL_LLEGIR_IOCTL _IO(NCDD_IOCTL_MAGIC,10)
#define NCDD_IOCTL_RETORNA_IOCTL _IO(NCDD_IOCTL_MAGIC,11)
#define NCDD_IOCTL_RETORNA_LLSEEK _IO(NCDD_IOCTL_MAGIC,12)

//SIGCHLD no s'ha de definir mai, s'utilitza per eliminar fills "zombies"
//#define NCDD_SENYAL_PETICIO_OPEN SIGUSR1
#define NCDD_SENYAL_PETICIO_CLOSE SIGINT //QUIT //TERM
#define NCDD_SENYAL_PETICIO_ALTRES SIGUSR2
//SIGINT=CTRL+C. Capturar-la i:
//si es el fill, fer-la com una peticio close
//si es el pare, esperar a tancar tots els fills

#define VALOR_unsigned_char(p) (*((unsigned char *)(p)) )
#define VALOR_unsigned_int(p) (*((unsigned int *)(p)) )
#define VALOR_int(p) (*((int *)(p)) )
#define VALOR_file(f) (*(struct file **)(f))
#define VALOR_long_long(p) (*((long long *)(p)) )

//If we use atomic_* internal kernel functions
//#define NCDD_USES_KERNEL_ATOMIC

#endif
