/*

    ncdd.c

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
* File ncdd.c
* Kernel module
*
*/

//kernel 2.6 compliant....
#define NCDD_INDEFINIT 0

#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cesar Hernandez");


#include <linux/init.h>
#include <linux/cdev.h>

#include <linux/fs.h>


#include <linux/signal.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "ncdd.h"

#define DEVICE_NAME "ncdd"

#define SUCCESS 0

//Peticio lliure per ser usada per una de nova
#define NCDD_PETICIO_LLIURE 0

//Peticio pendent de ser recollida pel client
#define NCDD_PETICIO_PENDENT 1

//Peticio que ha estat recollida pel client i pendent de retornar dades
#define NCDD_PETICIO_PENDENT_RETORN 2

//Peticio que ja te les dades retornades
#define NCDD_PETICIO_RETORNADA 3

//Valors de retorn:
//En funcions que retornen enter:
//0: OK
//-1: Error
//En funcions que retornen punter:
//<>0: OK
//0: Error


#define DEBUG //indica si s'han de generar missatges de depuracio

/*
void cause_link_error_by_routine_that_does_not_exist(void)
{
	printk ("NCDD: OpenMosix related error :-( \n");
}
*/

int ncdd_estat_control=NCDD_INDEFINIT;

//Cadena identificativa usada en la connexio amb el client
char *identificacio=NCDD_IDENTIFICACIO;

//Index dins la cadena identificativa
unsigned int index_identificacio=0;

//pid_t pid_client; //Pid del client registrat
struct task_struct *proces_client=NCDD_INDEFINIT; //proces client pare


//Estructura d'una peticio
struct e_peticio {
	//Aquestes dades s'intercanvien entre client-servidor i entre client i modul
	size_t length; //longitut de la peticio
	unsigned char ordre; //OPEN, READ, WRITE...

	//Aquestes dades nom� s'intercanvien entre client i modul
	struct file *file;
	unsigned char buffer_transmissio[20];

	//Aquestes dades no s'intercanvien
	struct task_struct *proces; //proc� que havia fet la petici�

	unsigned char estat; //estat de la peticio (lliure=nul.la o pendent)

	//Longitut de les dades a transmetre en la trama
	//(excepte les dades en les ordres write)
	unsigned char longitut_buffer;

	int cmd; //comanda cridada en una ordre ioctl pendent de retorn
	long long valor_retorn; //valor de retorn pel proces que ha fet la peticio

	struct e_peticio *seguent; //seguent peticio a la llista. NULL si no hi ha seguent

};

//Informacio dels processos fills que crea el client
//Cada un aten les peticions d'un descriptor de fitxer especific
//Quan es fa la ordre close, la informacio es libera (es posa el pid a 0)
struct e_fills_client {
	pid_t pid; 								//pid del proces fill
	struct file *file;					//file que gestiona el fill
	unsigned char *buffer_intercanvi; 	//buffer que emmagatzema les dades de read, write, ioctl
};

//Vector dels processos fill del client
struct e_fills_client fills_client[NCDD_MAX_CLIENTS_FILL];

//Diu a quina posicio maxima
//s'ha afegit algun client. Quan s'esborra la informacio del client,
//si es l'ultim del vector, s'actualitza pos_max_client
int pos_max_client=NCDD_INDEFINIT;


//Quantes peticions hi caben per bloc de memoria assignat
#define NCDD_PETICIONS_PER_BLOC 50

//Longitut de cada bloc de memoria que es sol.licita
#define NCDD_LONGITUT_BLOC_MEMORIA ((sizeof(struct e_peticio))*NCDD_PETICIONS_PER_BLOC)

//Nmero m�im de blocs de memoria.
#define NCDD_MAX_BLOCS_MEMORIA (1+(NCDD_MAX_PETICIONS_SIMULTANIES/NCDD_PETICIONS_PER_BLOC))

//Estructura d'un bloc de memoria assignat
struct e_ncdd_bloc_memoria {
	unsigned char *direccio;							//adre� del bloc assignat
	int peticions_assignades;		//nmero de peticions assignades a aquest bloc
};

//Vector on es guarda la informacio de cada bloc de memoria assignat
struct e_ncdd_bloc_memoria blocs_memoria[NCDD_MAX_BLOCS_MEMORIA];

//Nmero de blocs assignats
int blocs_assignats=0;

//Demanar un bloc de memoria mes
//Retorna:
//-1 si hi ha error al assignar memoria
//0 si no hi ha error
int assignar_bloc_memoria
(void)
{
	char *p;

	if (blocs_assignats==NCDD_MAX_BLOCS_MEMORIA) {
		printk ("NCDD: Error: Maximum memory blocks allocated\n");
		return -1;
  }
	p=kmalloc(NCDD_LONGITUT_BLOC_MEMORIA,GFP_KERNEL);
	if (p==NULL) {
		printk ("NCDD: Error allocating memory blocks\n");
		return -1;
  }

#ifdef DEBUG
	printk ("NCDD: Block number %d allocated to address %p\n",blocs_assignats,p);
#endif
	blocs_memoria[blocs_assignats].direccio=p;
	blocs_memoria[blocs_assignats].peticions_assignades=0;

	blocs_assignats++;

	return 0;
}

//Liberar tots els blocs de memoria assignats
//Aixo es fa nomes al treure el modul de memoria, o al posar-lo
//en estat NCDD_ESTAT_CONTROL_INICI
void liberar_blocs_memoria
(void)
{

	int i;
	for (i=0;i<blocs_assignats;i++) {
#ifdef DEBUG
		printk ("NCDD: Freeing memory block %d allocated to address %p\n",
						i,blocs_memoria[i].direccio);
#endif
		kfree (blocs_memoria[i].direccio);
	}
}

//Assigna una nova peticio dins l'ultim bloc disponible
//Retorna: Punter cap a la direccio. NULL si hi ha error
char *assignar_peticio
(void)
{
	int assignats,r;
	int offset;
	char *p;

	if (blocs_assignats==0) {
		//No hi ha cap bloc. Crear-ne un de nou
		r=assignar_bloc_memoria();
		if (r==-1) return 0;
		assignats=0;
	}

	else {
		assignats=blocs_memoria[blocs_assignats-1].peticions_assignades;
		if (assignats==NCDD_PETICIONS_PER_BLOC) {
			//S'ha de demanar un bloc nou
			r=assignar_bloc_memoria();
			if (r==-1) return 0;
			assignats=0;
		}
	}

	if (NCDD_PETICIONS_PER_BLOC*(blocs_assignats-1)+assignats==NCDD_MAX_PETICIONS_SIMULTANIES) {
		printk ("NCDD: Error: Maximum number of petition reached\n");
		return 0;
  }

	offset=assignats*sizeof(struct e_peticio);

	blocs_memoria[blocs_assignats-1].peticions_assignades++;

	p=blocs_memoria[blocs_assignats-1].direccio+offset;
#ifdef DEBUG
	printk ("NCDD: Allocated petition %d ",assignats);
	printk ("block %d to address %p\n",blocs_assignats-1,p);
#endif
	return p;

}

//Afegeix una peticio a la seguent posicio lliure
//Comprova cada peticio, una per una, fins trobar la primera amb
//l'atribut estat==NCDD_PETICIO_LLIURE
//Si no hi ha cap, assignar una de nova
//Retorna: Punter a la nova peticio, posant el seu estat a NCDD_PETICIO_PENDENT
struct e_peticio *afegir_peticio
(void)
{
	struct e_peticio *p,*anterior;//,*seguent;

	if (blocs_assignats==0) {
	  //No hi ha cap peticio feta. Assignar una de nova
		p=(struct e_peticio *)assignar_peticio();
		if (p==NULL) return NULL;
		p->estat=NCDD_PETICIO_PENDENT;
		p->seguent=NULL;

#ifdef DEBUG
		printk ("NCDD: Added (initial) petition to address: %p\n",p);
#endif
		return p;
	}

	anterior=NULL;
	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==NCDD_PETICIO_LLIURE) {
			p->estat=NCDD_PETICIO_PENDENT;

#ifdef DEBUG
			printk ("NCDD: Added (free) petition to address: %p\n",p);
#endif
			return p;
    	}
		anterior=p;
		p=p->seguent;

	} while (p!=NULL);

	//No hi ha cap peticio lliure. Assignar una de nova
	p=(struct e_peticio *)assignar_peticio();
	if (p==NULL) return NULL;
	p->estat=NCDD_PETICIO_PENDENT;
	p->seguent=NULL;

	anterior->seguent=p;
#ifdef DEBUG
	printk ("NCDD: Added (new) petition to address: %p\n",p);
#endif

	return p;

}

//Diu si el dispositiu � valid (esta dins el rang)
int si_dispositiu_valid
(int minor)
{
  return (minor>=0 && minor<NCDD_CONTROL);
}

//Afegeix la informacio d'un nou fill client
//Retorna: 0 si tot va be
//-1 en cas d'error
int afegir_fill
(pid_t pid,struct file *file)
{
	char *buffer_intercanvi;

	int i;
	//Es busca la primera posicio lliure
	for (i=0;i<NCDD_MAX_CLIENTS_FILL;i++) {
		if (fills_client[i].pid==0) { //Afegir-ho aqui
			buffer_intercanvi=kmalloc(NCDD_TAMANY_BUFFER_INTERCANVI,GFP_KERNEL);
			if (buffer_intercanvi==NULL) {
				printk ("NCDD: Error allocating swapping buffer to client\n");
				return -1;
			}

			fills_client[i].pid=pid;
			fills_client[i].file=file;
			fills_client[i].buffer_intercanvi=buffer_intercanvi;

			//Es major que pos_max_client?
			if (i>pos_max_client) pos_max_client=i;
#ifdef DEBUG
			printk ("NCDD: Added client child pid %d to position %d , buffer_intercanvi: %p\n"
							,pid,i,buffer_intercanvi);
#endif
			return 0;
		}
	}
	//Error, no hi ha cap disponible
	printk ("NCDD: Maximum client child number reached\n");
	return -1;
}

//Retorna l'estructura d'un client fill
//Entrada: file
//Retorna: e_fills_client si tot va be
//NULL en cas d'error
struct e_fills_client *buscar_fill
(struct file *file)
{
	int i;

	for (i=0;i<=pos_max_client;i++) {
		if (fills_client[i].pid!=0) {
			if (fills_client[i].file==file)  {
#ifdef DEBUG
				printk ("NCDD: Client child found file:%p pid:%d\n",
								fills_client[i].file,fills_client[i].pid);
#endif
				return &fills_client[i];
			}
		}
	}

	//Error, fill no trobat
#ifdef DEBUG
	printk ("NCDD: Client child of file:%p not found\n",file);
#endif
	return NULL;

}

//Retorna el pid d'un client
//Entrada: file
//Retorna: pid si tot va be
//-1 en cas d'error
int buscar_pid_fill
(struct file *file)
{
	struct e_fills_client *client;

	client=buscar_fill(file);
	if (client==NULL) return -1;
	else return client->pid;
}

//Retorna l'estructura d'un client fill
//Entrada: pid
//Retorna: e_fills_client si tot va be
//NULL en cas d'error
struct e_fills_client *buscar_fill_per_pid
(int pid)
{
	int i;

	for (i=0;i<=pos_max_client;i++) {
		if (fills_client[i].pid==pid) {
#ifdef DEBUG
			printk ("NCDD: Client child found file:%p pid:%d\n",
							fills_client[i].file,fills_client[i].pid);
#endif
			return &fills_client[i];
		}
	}
	//Error, fill no trobat
#ifdef DEBUG
	printk ("NCDD: Client child of pid:%u not found\n",pid);
#endif
	return NULL;

}


//Elimina l'entrada d'un client fill
//Usat per eliminar_fill i inicialitzar_ncdd
//Entrada: index del vector de fills
//Retorna: 0 si tot va be
//-1 en cas d'error
int eliminar_fill_index
(int i)
{

	if (fills_client[i].pid!=0) {
#ifdef DEBUG
		printk ("NCDD: Client child found file:%p pid:%d buffer_intercanvi:%p->Deleted\n",
						fills_client[i].file,fills_client[i].pid,fills_client[i].buffer_intercanvi);
#endif

		//No mirem si la funcio retorna error; en cas afirmatiu, es queda
		//una zona de memoria sense utilitzar, pero no avisem d'error
		kfree(fills_client[i].buffer_intercanvi);

		fills_client[i].pid=0;
		if (i==pos_max_client && i>0) pos_max_client--;
		return 0;
	}
	return -1;

}

//Elimina l'entrada d'un client fill
//Entrada: file
//Retorna: 0 si tot va be
//-1 en cas d'error
int eliminar_fill
(struct file *file)
{
	int i;

	for (i=0;i<=pos_max_client;i++) {
		if (fills_client[i].pid!=0) {
			if (fills_client[i].file==file)  {
				return (eliminar_fill_index(i));
			}
		}
	}
	//Error, fill no trobat
#ifdef DEBUG
	printk ("NCDD: Cliend child of file:%p not found to delete\n",file);
#endif
	return -1;
}



//Funci�per inicialitzar i/o reiniciar les estructures de dades i variables
void inicialitzar_ncdd
(void)
{
	int i;

	ncdd_estat_control=NCDD_ESTAT_CONTROL_INICI;

	for (i=0;i<=pos_max_client;i++) {
		eliminar_fill_index(i);
	}

	pos_max_client=0;
	ncdd_estat_control=NCDD_ESTAT_CONTROL_INICI;
	index_identificacio=0;

	//Aquesta funcio, logicament no fara res en cas
	//que es cridi quan s'inicialitza, pero si que liberara memoria quan vulguem
	//reiniciar el modul (p.ex: quan hi ha hagut algun error greu i volem reiniciar-lo)
	liberar_blocs_memoria();
	blocs_assignats=0;

}

//Busca la primera peticio que trobi amb l'estat i l'ordre indicats
struct e_peticio *buscar_peticio
(unsigned char estat,unsigned char ordre)
{
	struct e_peticio *p;

	if (blocs_assignats==0) return NULL;

	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==estat && p->ordre==ordre) {
		  return p;
    }
		p=p->seguent;

	} while (p!=NULL);

	return NULL;

}


//Busca la primera peticio que trobi amb l'estat i l'ordre READ, WRITE, IOCTL o LLSEEK
struct e_peticio *buscar_peticio_altres
(unsigned char estat)
{
	struct e_peticio *p;
	unsigned char o;

	if (blocs_assignats==0) return NULL;

	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==estat) {
			o=p->ordre;
			if (o==NCDD_ORDRE_READ || o==NCDD_ORDRE_WRITE ||
				o==NCDD_ORDRE_IOCTL || o==NCDD_ORDRE_LLSEEK) {
				return p;
			}
		}
		p=p->seguent;

	} while (p!=NULL);

	return NULL;

}


//Busca la primera peticio que trobi amb l'estat,l'ordre i el file indicats
struct e_peticio *buscar_peticio_amb_file
(unsigned char estat,unsigned char ordre,struct file *file)
{
	struct e_peticio *p;

	if (blocs_assignats==0) return NULL;

	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==estat && p->ordre==ordre &&
			p->file==file) {
			return p;
			}
		p=p->seguent;

	} while (p!=NULL);

	return NULL;

}

//Busca la primera peticio que trobi amb l'estat,el file indicat i l'ordre
//READ, WRITE, IOCTL o LLSEEK
struct e_peticio *buscar_peticio_altres_amb_file
(unsigned char estat,struct file *file)
{
	struct e_peticio *p;
	unsigned char o;

	if (blocs_assignats==0) return NULL;

	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==estat && p->file==file) {
			o=p->ordre;
			if (o==NCDD_ORDRE_READ || o==NCDD_ORDRE_WRITE ||
				o==NCDD_ORDRE_IOCTL || o==NCDD_ORDRE_LLSEEK) {
				return p;
			}
			return p;
		}
	p=p->seguent;
	} while (p!=NULL);

	return NULL;

}


//Pasar dades de l'espai de memoria del sistema a l'usuari
void escriure_a_usuari
(char *punter,unsigned long arg,int longitut)
{
	copy_to_user((void *)arg,(void *)punter,longitut);
}

//Pasar dades de l'espai de memoria de l'usuari al sistema
void llegir_de_usuari
(char *punter,unsigned long arg,int longitut)
{
	copy_from_user((void *)punter,(void *)arg,longitut);
}


//Escriu la direcci�d'una comanda ioctl
void escriu_direccio_ioctl(int cmd)
{
	if (cmd==_IOC_NONE) printk ("_IOC_NONE\n");
	else if (cmd==_IOC_READ) printk ("_IOC_READ\n");
	else if (cmd==_IOC_WRITE) printk ("_IOC_WRITE\n");
	else if (cmd== (_IOC_READ | _IOC_WRITE) ) printk ("_IOC_READ |  _IOC_WRITE\n");

}

//Dormir proces en curs
void dormir_proces(void)
{
#ifdef DEBUG
	printk("NCDD: Process %d sleeping\n",current->pid);
#endif
	current->state=TASK_INTERRUPTIBLE;
	schedule();
}

//Ordre d'apertura de dispositiu
int device_open
(struct inode *inode,struct file *file)
{

	unsigned char minor;				//valor minor del dispositiu
	struct e_peticio *peticio;		//punter cap a la petici�assignada
	char *p;								//punter auxiliar

	int valor_retorn;					//valor de retorn de l'ordre
	int pid;								//pid del client fill


#ifdef DEBUG
	printk ("NCDD: Device opened (inode:%p,file:%p)\n", inode, file);
	printk("NCDD: Device %d.%d\n",
				MAJOR(inode->i_rdev), MINOR(inode->i_rdev) );
#endif

	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client
 *
 *
*/

		return SUCCESS; //ordre no disponible pel client

  }

	else {
/*
 *
 * Comunicacio amb programes que obren dispositius. Ordres de lectura de dispositius
 *
 *
*/
		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
			printk ("NCDD: Device %d invalid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client not registered\n");
			return -ENODEV;
		}

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_OPEN;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_OPEN;

		p=peticio->buffer_transmissio;

		//Dades a retornar al client

		VALOR_file(p)=file;
		VALOR_unsigned_char(p+4)=minor;
		VALOR_unsigned_int(p+4+1)=file->f_flags;
		VALOR_unsigned_int(p+4+1+4)=file->f_mode;

#ifdef DEBUG
		printk ("NCDD: Petition open minor: %d, file: %p\n",minor,file);
		printk ("NCDD: f_flags:%u f_mode:%o\n",file->f_flags,file->f_mode);
#endif

		//Despertem proces pare de la senyal ioctl
#ifdef DEBUG
		printk ("NCDD: Awakening client father to make a command OPEN\n"); //per atendre una ordre open\n");
#endif


		//Avisem al client pare
		if (kill_proc(proces_client->pid,NCDD_SENYAL_PETICIO_ALTRES,1)<0) {
			//Error, no s'ha pogut avisar al client, reiniciar ncdd
			printk ("NCDD: Error, can't signal client father\n"); //no s'ha pogut avisar al client\n");
			return -EINVAL;
		}

		//Se despierta el padre con se�les, para poder dormir despues
		//al proceso que invoca la orden :-(

//		wake_up_process(proces_client);
	//	schedule();

		//I posem al proces que ha fet la peticio en estat d'espera
#ifdef DEBUG
		printk ("NCDD: Sleeping process to wait return from a command OPEN\n");
#endif

		dormir_proces();

#ifdef DEBUG
		printk ("NCDD: process %d awakened\n",current->pid);
#endif



		if (peticio->estat==NCDD_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("NCDD: Return value: %d\n",valor_retorn);
#endif
		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("NCDD: OPEN command broken\n");
#endif

			//Avisem client fill que acabi

			//Buscar el fill que correspon al file
			pid=buscar_pid_fill(file);
			if (pid<0) {
				printk ("NCDD: Cliend child of file:%p not found doing close\n",file);
				return -EIO;
			}

			if (kill_proc(pid,NCDD_SENYAL_PETICIO_CLOSE,1)<0) {
				//Error, no s'ha pogut avisar al client fill
				printk ("NCDD: Error, can't signal client child\n"); //no s'ha pogut avisar al client fill\n");
				return -EIO;
			}


		}

		peticio->estat=NCDD_PETICIO_LLIURE;


		//El valor de retorn, normalment sera <0 si hi ha error, o
		//0 si no hi ha. De totes maneres, protegim el modul perque
		//no retorni valors positius (no error), ja que si no es poden
		//produir resultats inesperats
		if (valor_retorn>=0) return SUCCESS;
		else {
			//Llavors, eliminar el client fill
#ifdef DEBUG
			printk ("NCDD: Deleting client child so OPEN has returned error\n");
#endif
			if (eliminar_fill(file)<0) {
#ifdef DEBUG
				printk ("NCDD: Error deleting client child of file %p\n",file);
#endif
			}
			return valor_retorn;
		}
	}

}

//Ordre de tancament de dispositiu
int device_release
(struct inode *inode,struct file *file)
{

	unsigned char minor;				//valor minor del dispositiu
	struct e_peticio *peticio;		//punter cap a la petici�assignada

	int valor_retorn;					//valor de retorn de la ordre
	int pid;								//pid del client fill


	minor=MINOR(file->f_dentry->d_inode->i_rdev);

#ifdef DEBUG
  printk ("NCDD: Devide freed (inode:%p,file:%p)\n", inode, file);
#endif


	if (current->pid==proces_client->pid) {
		printk ("NCDD: Client ended.\n");

		return 0;
  }

	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client
 *
 *
*/
	return SUCCESS;
	}

	else {
/*
 *
 * Comunicacio amb programes que obren dispositius. Ordres de lectura de dispositius
 *
 *
*/
		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
		  printk ("NCDD: Device %d invalid\n",minor);
#endif
		  return -EINVAL;
      }

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client not registered\n");
			return -ENODEV;
		}


		//Assignar peticio de tancament
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_CLOSE;
		peticio->estat=NCDD_PETICIO_PENDENT_RETORN;
		//Aquesta peticio no necessita del client per recollir les dades

		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_OPEN;

#ifdef DEBUG
		printk ("NCDD: Petition CLOSE minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
#endif

		//Avisem al client fill corresponent

		//Buscar el fill que correspon al file
		pid=buscar_pid_fill(file);
		if (pid<0) {
			printk ("NCDD: Client child of file:%p not found doing CLOSE\n",file);
			return -EIO;
		}

		if (kill_proc(pid,NCDD_SENYAL_PETICIO_CLOSE,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("NCDD: Error, can't signal client child\n");
			return -EIO;
		}

		//I posem al proces que ha fet la peticio en estat d'espera

		dormir_proces();


#ifdef DEBUG
		printk ("NCDD: Process %d awakened\n",current->pid);
#endif

		if (peticio->estat==NCDD_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("NCDD: Return value: %d\n",valor_retorn);
#endif
		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("NCDD: CLOSE command broken\n");
#endif

		}

		peticio->estat=NCDD_PETICIO_LLIURE;
		//Treure el fill de la llista de clients fill

		if (eliminar_fill(file)<0) {
#ifdef DEBUG
			printk ("NCDD: Error deleting child of file %p\n",file);
#endif
			return -EIO;
		}
		if (valor_retorn>=0) return SUCCESS;
		else  return valor_retorn;

	}


}

//Ordre de lectura de dispositiu
ssize_t device_read
(struct file *file,char *buffer,size_t length,loff_t *offset)

{

	unsigned char minor;					//valor minor del dispositiu
	struct e_peticio *peticio;			//punter cap a la petici�assignada

	int valor_retorn;							//valor de retorn de la ordre
	int pid;												//pid del client fill

	char *p;											//punter auxiliar
	struct e_fills_client *client;	//fill que gestiona el file determinat

	int longitut_total=0;					//usat en la descomposicio en tro�s de les dades
	int longitut_fragment;				//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client
 *
 *
*/
		return -EINVAL; //ordre no disponible pel client
  }

	else {
/*
 *
 * Comunicacio amb programes que obren dispositius. Ordres de lectura de dispositius
 *
 *
*/
		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
		  printk ("NCDD: Device %d invalid\n",minor);
#endif
		  return -EINVAL;
		}

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client not registered\n");
			return -ENODEV;
		}


		//Assignar peticio de lectura
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_READ;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_READ;

		p=peticio->buffer_transmissio;
		*p=NCDD_ORDRE_READ;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("NCDD: Client child of file:%p not found doing READ\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		//Bucle que descomposa les dades en tro�s iguals a NCDD_TAMANY_BUFFER_INTERCANVI
		do {
			if (length-longitut_total>NCDD_TAMANY_BUFFER_INTERCANVI)
				longitut_fragment=NCDD_TAMANY_BUFFER_INTERCANVI;
			else longitut_fragment=length-longitut_total;

			VALOR_unsigned_int(p+1)=longitut_fragment;

#ifdef DEBUG
			printk ("NCDD: Petition READ minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
			printk ("NCDD: length:%d\n",longitut_fragment);
#endif


			//Avisem al client fill corresponent

			if (kill_proc(pid,NCDD_SENYAL_PETICIO_ALTRES,1)<0) {
				//Error, no s'ha pogut avisar al client fill
				printk ("NCDD: Error, can't signal client child\n");
				return -EIO;
			}


			//I posem al proces que ha fet la peticio en estat d'espera
			dormir_proces();

#ifdef DEBUG
			printk ("NCDD: Process %d awakened\n",current->pid);
#endif

			if (peticio->estat==NCDD_PETICIO_RETORNADA) {
				valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
				printk ("NCDD: Return value: %d\n",valor_retorn);
#endif

				if (valor_retorn>0) {
					//Traspassar dades del client_fill cap al proces que ha fet la peticio

#ifdef DEBUG
					printk ("NCDD: Copying %d data from %p to %p from READ return\n",
					valor_retorn,client->buffer_intercanvi,buffer);
#endif
					escriure_a_usuari(client->buffer_intercanvi,
															(unsigned long)buffer,valor_retorn);
					*offset +=valor_retorn;
					buffer +=valor_retorn;
					longitut_total +=valor_retorn;
				}

			}

			else { //Proces interromput per una senyal
				valor_retorn=-EINTR;
#ifdef DEBUG
				printk ("NCDD: READ command broken\n");
#endif
				peticio->estat=NCDD_PETICIO_LLIURE;

				return valor_retorn;

			}
		peticio->estat=NCDD_PETICIO_PENDENT;
		} while (longitut_total <length && valor_retorn!=0);

		peticio->estat=NCDD_PETICIO_LLIURE;

		return longitut_total;


	}


}


//Ordre d'escriptura de dispositiu
ssize_t device_write
(struct file *file,const char *buffer,size_t length,loff_t *offset)
{
	unsigned char minor;					//valor minor del dispositiu
	struct e_peticio *peticio;			//punter cap a la petici�assignada

	int valor_retorn;							//valor de retorn de la ordre
	int pid;												//pid del client fill

	char *p;											//punter auxiliar
	struct e_fills_client *client;	//fill que gestiona el file determinat

	int longitut_total=0;					//usat en la descomposicio en tro�s de les dades
	int longitut_fragment;				//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);

	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client
 *
 *
*/

		return -EINVAL; //ordre no disponible pel client

	}
	else {

/*
 *
 * Comunicacio amb programes que obren dispositius. Ordres d'escriptura a dispositius
 *
 *
*/
		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
			printk ("NCDD: Device %d invalid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client not registered\n");
			return -ENODEV;
		}


		//Assignar peticio d'escriptura
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_WRITE;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_WRITE;

		p=peticio->buffer_transmissio;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("NCDD: Client child of file:%p not found doing WRITE\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		do { //Bucle que descomposa les dades en tro�s iguals a NCDD_TAMANY_BUFFER_INTERCANVI
			if (length-longitut_total>NCDD_TAMANY_BUFFER_INTERCANVI)
				longitut_fragment=NCDD_TAMANY_BUFFER_INTERCANVI;
			else longitut_fragment=length-longitut_total;

			*p=NCDD_ORDRE_WRITE;
			VALOR_unsigned_int(p+1)=longitut_fragment;

#ifdef DEBUG
			printk ("NCDD: Petition WRITE minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
			printk ("NCDD: length:%d\n",longitut_fragment);
#endif

			//Traspassar les dades del proces cap al kernel
			if (longitut_fragment) {
				//Traspassar dades del client_fill cap al proces que ha fet la peticio
#ifdef DEBUG
				printk ("NCDD: Copying %u data from %p to %p of WRITE\n",
				longitut_fragment,buffer,client->buffer_intercanvi);
#endif
				llegir_de_usuari(client->buffer_intercanvi,
														(unsigned long)buffer,longitut_fragment);
			}

			//Avisem al client fill corresponent
			if (kill_proc(pid,NCDD_SENYAL_PETICIO_ALTRES,1)<0) {
				//Error, no s'ha pogut avisar al client fill
				printk ("NCDD: Error, can't signal client child\n");
				return -EIO;
			}


			//I posem al proces que ha fet la peticio en estat d'espera
			dormir_proces();

#ifdef DEBUG
			printk ("NCDD: Process %d awakened\n",current->pid);
#endif

			if (peticio->estat==NCDD_PETICIO_RETORNADA) {
				valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
				printk ("NCDD: Return value: %d\n",valor_retorn);
#endif

			}

			else { //Proces interromput per una senyal
				valor_retorn=-EINTR;
#ifdef DEBUG
				printk ("NCDD: WRITE command broken\n");
#endif
				peticio->estat=NCDD_PETICIO_LLIURE;

				return valor_retorn;

			}
			peticio->estat=NCDD_PETICIO_PENDENT;
			*offset +=valor_retorn;
			buffer +=valor_retorn;
			longitut_total +=valor_retorn;

		} while (longitut_total <length && valor_retorn!=0);

		peticio->estat=NCDD_PETICIO_LLIURE;

		return longitut_total;


	}
	return -EINVAL;
}

//Ordre ioctl de dispositiu
int device_ioctl
(struct inode *inode, struct file *file,unsigned int cmd, unsigned long arg)
{

	unsigned char minor;						//valor minor del dispositiu
	struct e_peticio *peticio;				//punter cap a la petici�assignada

	pid_t pid;									//pid del client fill


	int valor_retorn;							//valor de retorn de la ordre
	long long valor_retorn64;				//valor de retorn de la ordre llseek
	char *direccio_llegida;					//direccio on estan les dades d'una operacio read
	struct e_fills_client *client;		//fill que gestiona el file determinat

	int size;									//dades de la ordre ioctl
	int direction;								//

	char c;										//variables auxiliars
	unsigned char *punter;					//
	struct file *f;							//
	char buffer[16];							//
	unsigned int longitut_escriure;		//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client pare
 *
 *
*/

		if (cmd==NCDD_IOCTL_REGISTRAR_CLIENT) {
			if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
				//No esta registrat encara

				for (;index_identificacio<(sizeof(NCDD_IDENTIFICACIO))+1;
					index_identificacio++) {
					//Es posa sizeof +1 per a que es llegeixi tambe el codi 0 de final

					get_user(c,(unsigned long*)(arg++));
					if (c!=identificacio[index_identificacio]) {
						printk ("NCDD: Invalid identifier\n"); //Identificacio no valida\n");
						index_identificacio=0;

						return -EINVAL;
					}

				}
				ncdd_estat_control=NCDD_ESTAT_CONTROL_REGISTRAT;

				//Guardar estructura de proces del client
				proces_client=current;

				//Guardar el pid del client
				//proces_client->pid_client=current->pid;

				printk ("NCDD: Client registered, pid: %d\n",proces_client->pid);

				//Llavors, tornar al client.
				//El client, a partir d'ara, ha d'esperar la senyal open

				return 0;

			}
			else {
				printk ("NCDD: Client already registered!\n");
				return -EINVAL;
			}
		}

		if (cmd==NCDD_IOCTL_LLEGIR_OPEN) {
			if (current->pid!=proces_client->pid) {
				printk ("NCDD: Client not authorized\n");
				return -EINVAL;
			}

			//Donar al client pare el minor i el file
			//Buscar peticions que estiguin de tipus NCDD_ORDRE_OPEN
			peticio=buscar_peticio(NCDD_PETICIO_PENDENT,NCDD_ORDRE_OPEN);
			if (peticio==NULL) {

				//Cap peticio open pendent de retorn, dormir proces client pare
#ifdef DEBUG
				printk ("NCDD: Client father sleeping\n");
#endif

				dormir_proces();
#ifdef DEBUG
				printk ("NCDD: Client father awakened from NCDD_IOCTL_LLEGIR_OPEN\n");
#endif
				//Aqui s'ha despertat el pare, tant pot ser que l'haguem despertat
				//nosaltres per a que rebi una peticio open, com que hagi rebut
				//alguna senyal
				peticio=buscar_peticio(NCDD_PETICIO_PENDENT,NCDD_ORDRE_OPEN);
				if (peticio==NULL) {
					//Ha rebut una senyal
#ifdef DEBUG
					printk ("NCDD: Client father has received a signal\n");
#endif
					return -EINTR;
				}

				//printk ("NCDD: No hi ha cap peticio open pendent al fer NCDD_IOCTL_LLEGIR_OPEN\n");
				//return -EIO;


			}

			//Donar la informacio
			punter=&peticio->buffer_transmissio[0];
			escriure_a_usuari(punter,arg,NCDD_LONGITUT_OPEN);

			return 0;
		}

		if (cmd==NCDD_IOCTL_ASSIGNAR_FILL) {
			if (current->pid!=proces_client->pid) {
				printk ("NCDD: Client not authorized\n");
				return -EINVAL;
			}

			//llegir dades del fill (file,pid)
			llegir_de_usuari((char *)&f,arg,4);
			llegir_de_usuari((char *)&pid,arg+4,4);

#ifdef DEBUG
			printk ("NCDD: Trying to register child, PID:%d f:%p\n",pid,f);
#endif

			//Posar peticio open a estat pendent de retorn, que correspongui a f
#ifdef DEBUG
			printk ("NCDD: Finding (pendent) petition file:%p\n",f); //Buscant peticio pendent file:%p\n",f);
#endif
			peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT,NCDD_ORDRE_OPEN,f);
			if (peticio==NULL) {
				printk ("NCDD: No hi ha cap peticio open pendent\n");
				return -EINVAL;
			}

#ifdef DEBUG
			printk("NCDD: Fill PID:%d f:%p Registrat\n",pid,f);
#endif
			peticio->estat=NCDD_PETICIO_PENDENT_RETORN;
			//Quan es retorni el valor de la operacio open realitzada
			//pel client, es liberara la peticio

			if (afegir_fill(pid,f)==-1)  return -EINVAL;

			return SUCCESS;

		}

/*
 *
 * Comunicacio amb els clients fill
 *
 *
*/

		//Veure si el client fill esta autoritzat
		client=buscar_fill_per_pid(current->pid);
		if (client==NULL) {
			printk ("NCDD: Client fill no autoritzat\n");
			return -EINVAL;
		}

		//Tractar les diferents comandes de comunicacio ncdd<->client
		switch (cmd) {

			case NCDD_IOCTL_RETORNA_OPEN:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_OPEN);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio open pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_OPEN,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio open pendent de retorn\n");
					return -EIO;
				}
#ifdef DEBUG
				printk ("NCDD: Proces pendent de retorn trobat, pid: %d\n",peticio->proces->pid);
#endif

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=NCDD_PETICIO_RETORNADA;
				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;
			break;

			case NCDD_IOCTL_RETORNA_CLOSE:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_OPEN);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio close pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_CLOSE,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio close pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=NCDD_PETICIO_RETORNADA;
				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			/*
						//Donar al client pare el minor i el file
			//Buscar peticions que estiguin de tipus NCDD_ORDRE_OPEN
			peticio=buscar_peticio(NCDD_PETICIO_PENDENT,NCDD_ORDRE_OPEN);
			if (peticio==NULL) {

				//Cap peticio open pendent de retorn, dormir proces client pare
				dormir_proces();
#ifdef DEBUG
				printk ("NCDD: Client pare despertat de NCDD_IOCTL_LLEGIR_OPEN\n");
#endif
				//Aqui s'ha despertat el pare, tant pot ser que l'haguem despertat
				//nosaltres per a que rebi una peticio open, com que hagi rebut
				//alguna senyal
				peticio=buscar_peticio(NCDD_PETICIO_PENDENT,NCDD_ORDRE_OPEN);
				if (peticio==NULL) {
					//Ha rebut una senyal
#ifdef DEBUG
					printk ("NCDD: Client pare ha rebut una senyal\n");
#endif
					return -EINTR;
				}

				//printk ("NCDD: No hi ha cap peticio open pendent al fer NCDD_IOCTL_LLEGIR_OPEN\n");
				//return -EIO;


			}


			*/

			case NCDD_IOCTL_LLEGIR_ALTRES:
				f=client->file;

				//Donar al client fill les dades de la peticio emesa
				//Buscar peticions que siguin tipus NCDD_ORDRE_READ, WRITE, IOCTL

				peticio=buscar_peticio_altres_amb_file(NCDD_PETICIO_PENDENT,f);
				if (peticio==NULL) {
//
					peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_CLOSE,f);
					if (peticio!=NULL) {
#ifdef DEBUG
						printk ("NCDD: Client fill te pendent una ordre CLOSE\n");
#endif
						return -EINTR;
					}
//


#ifdef DEBUG
					printk ("NCDD: Client fill pid %d dormint\n",current->pid);
#endif

				//Cap peticio open pendent de retorn, dormir proces client fill
					dormir_proces();
#ifdef DEBUG
					printk ("NCDD: Client fill despertat de NCDD_IOCTL_LLEGIR_ALTRES\n");
#endif

					peticio=buscar_peticio_altres_amb_file(NCDD_PETICIO_PENDENT,f);
					if (peticio==NULL) {
					//Ha rebut una senyal
#ifdef DEBUG
						printk ("NCDD: Client fill ha rebut una senyal diferent de altres\n");
#endif
						return -EINTR;
					}
				}


				//Donar la informacio
				punter=&peticio->buffer_transmissio[0];
				escriure_a_usuari(punter,arg,peticio->longitut_buffer);
				peticio->estat=NCDD_PETICIO_PENDENT_RETORN;

				return 0;
			break;

			case NCDD_IOCTL_RETORNA_READ:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_READ);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio read pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_READ,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio read pendent de retorn\n");
					return -EINVAL;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				direccio_llegida=(char *)VALOR_unsigned_int(&buffer[4]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=NCDD_PETICIO_RETORNADA;

				if (valor_retorn>0) {
#ifdef DEBUG
					printk ("NCDD: Movem dades de client %p a ncdd %p\n",
									direccio_llegida,client->buffer_intercanvi);
#endif

					llegir_de_usuari(client->buffer_intercanvi,
													(unsigned long)direccio_llegida,valor_retorn);
				}

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case NCDD_IOCTL_LLEGIR_WRITE:
				f=client->file;

#ifdef DEBUG
				printk ("NCDD: Buscant peticio write pendent, file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_WRITE,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio write pendent\n");
					return -EIO;
				}

				longitut_escriure=VALOR_int(&peticio->buffer_transmissio[1]);
				VALOR_int(&peticio->buffer_transmissio[0])=longitut_escriure;

				if (longitut_escriure>0) {

#ifdef DEBUG
					printk ("NCDD: Movem %u dades de ncdd %p a client %lx\n",
									longitut_escriure,client->buffer_intercanvi,arg);
#endif

					escriure_a_usuari(client->buffer_intercanvi,
													(unsigned long)arg,longitut_escriure);
				}
				return SUCCESS;

			break;

			case NCDD_IOCTL_RETORNA_WRITE:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_WRITE);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio write pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_WRITE,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio write pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;

				peticio->estat=NCDD_PETICIO_RETORNADA;

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case NCDD_IOCTL_LLEGIR_IOCTL:
				f=client->file;

#ifdef DEBUG
				printk ("NCDD: Buscant peticio ioctl pendent, file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_IOCTL,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio ioctl pendent\n");
					return -EIO;
				}

				cmd=peticio->cmd;
				size=_IOC_SIZE(cmd);
				direction=_IOC_DIR(cmd);

				//Veure si s'han de transmetre dades
				if (direction==_IOC_NONE) {
					//Donar nom� el valor de arg
					VALOR_unsigned_int(arg)=VALOR_unsigned_int(client->buffer_intercanvi);
				}

				else if (direction==_IOC_WRITE || direction==(_IOC_WRITE | _IOC_READ) ) {
					if (size>0) {
						//Traspassar dades (tamany=size) d'espai d'usuari (*arg) cap a
						//buffer_intercanvi.
						escriure_a_usuari(client->buffer_intercanvi,
												arg,size);
						}
				}

				return SUCCESS;

			break;

			case NCDD_IOCTL_RETORNA_IOCTL:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_IOCTL);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio ioctl pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_IOCTL,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio ioctl pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				direccio_llegida=(char *)VALOR_unsigned_int(&buffer[4]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=NCDD_PETICIO_RETORNADA;

				direction=_IOC_DIR(peticio->cmd);
				size=_IOC_SIZE(peticio->cmd);
				if (direction==_IOC_READ || direction==(_IOC_READ | _IOC_WRITE) ) {
					if (size>0 && valor_retorn>=0) {
#ifdef DEBUG
						printk ("NCDD: Movem dades de client (%p) a ncdd (%p)\n",
										direccio_llegida,client->buffer_intercanvi);
#endif

						llegir_de_usuari(client->buffer_intercanvi,
														(unsigned long)direccio_llegida,size);
					}
				}

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case NCDD_IOCTL_RETORNA_LLSEEK:
				llegir_de_usuari(buffer,arg,NCDD_LONGITUT_RETORNA_LLSEEK);
				f=client->file;
#ifdef DEBUG
				printk ("NCDD: Buscant peticio llseek pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(NCDD_PETICIO_PENDENT_RETORN,NCDD_ORDRE_LLSEEK,f);
				if (peticio==NULL) {
					printk ("NCDD: No hi ha cap peticio llseek pendent de retorn\n");
					return -EINVAL;
				}

				//L'Ordre llseek � la nica que retorna un enter de 64 bits
				valor_retorn64=VALOR_long_long(&buffer[0]);
				peticio->valor_retorn=valor_retorn64;

				peticio->estat=NCDD_PETICIO_RETORNADA;

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;


			default:
				return -EINVAL;
			break;
		}
  }

	else {
/*
 *
 * Comunicacio amb programes que obren dispositius. Ordres de "ioctl" de dispositius
 *
 *
*/
		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
			printk ("NCDD: Dispositiu %d no valid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client no registrat\n");
			return -ENODEV;
		}


		size=_IOC_SIZE(cmd);
		direction=_IOC_DIR(cmd);

#ifdef DEBUG

		printk ("NCDD: Ordre ioctl:\n");
		printk ("NCDD: type:%u\n",_IOC_TYPE(cmd));
		printk ("NCDD: number:%u\n",_IOC_NR(cmd));
		printk ("NCDD: size:%u\n",size);
		printk ("NCDD: direction:%u=",direction);
		escriu_direccio_ioctl(direction);

#endif

		//Assignar peticio de ioctl

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_IOCTL;
		peticio->file=file;
		peticio->cmd=cmd;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_IOCTL;

		punter=peticio->buffer_transmissio;
		*punter=NCDD_ORDRE_IOCTL;
		VALOR_unsigned_int(punter+1)=cmd;

		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("NCDD: Client fill no trobat\n");
#endif
			return -EINVAL;
		}

		//A partir d'aqui, traspassar les dades a escriure de l'espai
		//d'usuari cap el buffer_intercanvi del client_fill

		if (direction==_IOC_NONE) {
			//Donar nom� el valor de arg
#ifdef DEBUG
			printk ("NCDD: Passem el valor %u cap el buffer_intercanvi\n",(unsigned int)arg);
#endif
			VALOR_unsigned_int(client->buffer_intercanvi)=(unsigned int)arg;
		}

		else if (direction==_IOC_WRITE || direction==(_IOC_WRITE | _IOC_READ) ) {
			if (size>0) {
				//Traspassar dades (tamany=size) de espai d'usuari (*arg) cap a
				//buffer_intercanvi.
#ifdef DEBUG
			printk ("NCDD: Traspassem %u dades de %x cap el buffer_intercanvi (%p)\n",
						size,(unsigned int)arg,client->buffer_intercanvi);
#endif

				llegir_de_usuari(client->buffer_intercanvi,
												arg,size);
				}
		}

		else if (direction==_IOC_READ) {
			//Llavors, no fa falta traspassar cap dada
		}

		//Avisar al client fill

		if (kill_proc(client->pid,NCDD_SENYAL_PETICIO_ALTRES,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("NCDD: Error, no s'ha pogut avisar al client fill\n");
			return -EIO;
		}

		//I posem al proces que ha fet la peticio en estat d'espera
		dormir_proces();


#ifdef DEBUG
		printk ("NCDD: proces %d despertat\n",current->pid);
#endif

		if (peticio->estat==NCDD_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("NCDD: Valor de retorn ioctl: %d\n",valor_retorn);
#endif

			//Traspassar dades de kernel a proces (si fa falta)
			if (direction==_IOC_READ || direction==(_IOC_READ | _IOC_WRITE)) {
				if (size>0 && valor_retorn>=0) {
					//Traspassar dades del client_fill cap al proces que ha fet la peticio
					//
#ifdef DEBUG
					printk ("NCDD: Movent %u dades de %p a %x de resultat de ioctl\n",
					size,client->buffer_intercanvi,(unsigned int)arg);
#endif
					escriure_a_usuari(client->buffer_intercanvi,
															(unsigned long)arg,size);
				}

			}

		}

		else {  //Proces interromput per una senyal
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("NCDD: Ordre ioctl interrompuda\n");
#endif

		}

		peticio->estat=NCDD_PETICIO_LLIURE;

		if (valor_retorn>=0) return SUCCESS;
		else  return valor_retorn;

	}

	return -EINVAL;
}

//Ordre llseek de dispositius
long long device_llseek
(struct file *file, long long offset, int whence)
{

	unsigned char minor;						//valor minor del dispositiu
	struct e_peticio *peticio;				//punter cap a la petici�assignada

	long long valor_retorn;					//valor de retorn de la ordre
	pid_t pid;												//pid del client fill
	struct e_fills_client *client;		//fill que gestiona el file determinat

	unsigned char *p;								//punter auxiliar


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==NCDD_CONTROL) {
/*
 *
 * Comunicacio amb el client
 *
 *
*/

		return -EINVAL; //ordre no disponible pel client

  }

	else {
/*
 *
 * Comunicacio amb processos. Ordres de lseek de dispositius
 *
 *
*/

		if (!si_dispositiu_valid(minor)) {
#ifdef DEBUG
		  printk ("NCDD: Dispositiu %d no valid\n",minor);
#endif
		  return -EINVAL;
      }

		//Veure si esta registrat
		if (ncdd_estat_control==NCDD_ESTAT_CONTROL_INICI) {
			printk ("NCDD: Client no registrat\n");
			return -ENODEV;
		}


#ifdef DEBUG
		printk ("NCDD: Ordre llseek: offset:%Ld, whence: %d\n",offset,whence);
#endif

		//Assignar peticio de lectura

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=NCDD_ORDRE_LLSEEK;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=NCDD_LONGITUT_LLSEEK;

		p=peticio->buffer_transmissio;

		*p=NCDD_ORDRE_LLSEEK;
		VALOR_long_long(p+1)=offset;
		VALOR_long_long(p+1+8)=whence;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("NCDD: No s'ha trobat el client fill de file:%p al fer llseek\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		//Avisem al client fill corresponent

		if (kill_proc(pid,NCDD_SENYAL_PETICIO_ALTRES,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("NCDD: Error, no s'ha pogut avisar al client fill\n");
			return -EIO;
		}


		//I posem al proces que ha fet la peticio en estat d'espera
		dormir_proces();


#ifdef DEBUG
		printk ("NCDD: proces %d despertat\n",current->pid);
#endif

		if (peticio->estat==NCDD_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("NCDD: Valor de retorn llseek: %Ld\n",valor_retorn);
#endif


			if (valor_retorn>=0) {
				file->f_pos=valor_retorn;
			}

		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("NCDD: Ordre llseek interrompuda\n");
#endif
			peticio->estat=NCDD_PETICIO_LLIURE;

		}

		peticio->estat=NCDD_PETICIO_LLIURE;
		return valor_retorn;

	}


}

static int Major;


//Operacions sobre el dispositiu
static struct cdev ncdd_cdev = {
//	.kobj = {.name = DEVICE_NAME, },
	.kobj = {.name = "ncdd", },
};

static const struct file_operations Fops = {
        .owner 	=	THIS_MODULE,
        //Identifica el m�ul. Aixo fa que automaticament s'incrementi o decrementi
        //el "usage count" del m�ul
        .llseek = 	device_llseek,
        .read	=	device_read,
        .write	=	device_write,
        .ioctl	=	device_ioctl,
        .open	=	device_open,
        .release = 	device_release
};



//Funci�d'inicialitzaci�del m�ul

int ncdd_module_init()
{

	int i;

        dev_t ncdd_dev_t;
	int err;

	printk ("Loading NCDD kernel module version " 
		NCDD_VERSION
                " compiled on "
                NCDD_DATE
		"\n");


	Major=NCDD_MAJOR;


	ncdd_dev_t=MKDEV(Major, 0);


	if (register_chrdev_region(ncdd_dev_t, NCDD_MINORS, DEVICE_NAME)) {
		printk ("NCDD: Error registering device NCDD with major %d\n",Major);
		return Major;
	}
	#ifdef DEBUG
	printk ("NCDD: registered\n"); 
	printk ("NCDD: trying to call cdev_init (%x,%x)\n",&ncdd_cdev,&Fops);
	#endif 

//ncdd_cdev.owner = THIS_MODULE;

cdev_init(&ncdd_cdev, &Fops);
	#ifdef DEBUG
	printk ("NCDD: inited\n");
	#endif 
err = cdev_add (&ncdd_cdev, ncdd_dev_t, NCDD_MINORS);

/* Fail gracefully if need be */
if (err) {
	printk(KERN_NOTICE "Error %d adding ncdd", err);

	unregister_chrdev_region(ncdd_dev_t, NCDD_MINORS);
	return err;
}
	#ifdef DEBUG
	printk ("NCDD: added\n");
	#endif 


	printk ("NCDD: Modul NCDD registrat amb numero major %d\n",Major);
	printk ("NCDD: Control NCDD minor %d\n",NCDD_CONTROL);

#ifdef DEBUG
	printk ("NCDD: sizeof (e_peticio): %d\n",sizeof(struct e_peticio));
	printk ("NCDD: Numero maxim de peticions simultanies: %d\n",NCDD_MAX_PETICIONS_SIMULTANIES);
	printk ("NCDD: Peticions per cada bloc de memoria: %d\n",NCDD_PETICIONS_PER_BLOC);
	printk ("NCDD: Longitut de cada bloc de memoria: %d\n",NCDD_LONGITUT_BLOC_MEMORIA);
	printk ("NCDD: Numero maxim de blocs de memoria: %d\n",NCDD_MAX_BLOCS_MEMORIA);
	printk ("NCDD: Tamany del buffer d'intercanvi: %d\n",NCDD_TAMANY_BUFFER_INTERCANVI);
#endif

	//Inicialitzar vector de clients fill
	for (i=0;i<NCDD_MAX_CLIENTS_FILL;i++) {
		fills_client[i].pid=0;
	}

	inicialitzar_ncdd();

   return 0;
}


//Funcio d'eliminacio del m�ul de mem�ia

void ncdd_module_exit()
{


        dev_t ncdd_dev_t;


	inicialitzar_ncdd();


	
	cdev_del (&ncdd_cdev);

        ncdd_dev_t=MKDEV(Major, 0);
        unregister_chrdev_region(ncdd_dev_t, NCDD_MINORS);
	printk ("NCDD: Modul NCDD liberat\n");

	/*if (ret < 0)
		printk("NCDD: Error liberant modul NCDD: %d\n", ret);
	else printk ("NCDD: Modul NCDD liberat\n");*/
}


module_init(ncdd_module_init);
module_exit(ncdd_module_exit);
