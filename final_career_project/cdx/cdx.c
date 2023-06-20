/*
*
* Controlador de dispositius en xarxa
*
* Treball de fi de carrera
* Alumne: Cesar Hernandez Baño
* Carrera: Enginyeria Tecnica Informatica de Sistemes
*
* Fitxer cdx.c
* Modul del sistema operatiu
*
*/

#include <linux/config.h>

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/wrapper.h>
#include <linux/signal.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "cdx.h"

#define DEVICE_NAME "cdx"

#define SUCCESS 0

//Peticio lliure per ser usada per una de nova
#define CDX_PETICIO_LLIURE 0

//Peticio pendent de ser recollida pel client
#define CDX_PETICIO_PENDENT 1

//Peticio que ha estat recollida pel client i pendent de retornar dades
#define CDX_PETICIO_PENDENT_RETORN 2

//Peticio que ja te les dades retornades
#define CDX_PETICIO_RETORNADA 3

//Valors de retorn:
//En funcions que retornen enter:
//0: OK
//-1: Error
//En funcions que retornen punter:
//<>0: OK
//0: Error


#define DEBUG //indica si s'han de generar missatges de depuracio

int cdx_estat_control;

//Cadena identificativa usada en la connexio amb el client
char *identificacio=CDX_IDENTIFICACIO;

//Index dins la cadena identificativa
unsigned int index_identificacio=0;

pid_t pid_client; //Pid del client registrat


//Estructura d'una petició
struct e_peticio {
	//Aquestes dades s'intercanvien entre client-servidor i entre client i modul
	size_t length; //longitut de la peticio
	unsigned char ordre; //OPEN, READ, WRITE...

	//Aquestes dades només s'intercanvien entre client i modul
	struct file *file;
	unsigned char buffer_transmissio[20];

	//Aquestes dades no s'intercanvien
	struct task_struct *proces; //procés que havia fet la petició


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
struct e_fills_client fills_client[CDX_MAX_CLIENTS_FILL];

//Diu a quina posicio maxima
//s'ha afegit algun client. Quan s'esborra la informacio del client,
//si es l'ultim del vector, s'actualitza pos_max_client
int pos_max_client;


//Quantes peticions hi caben per bloc de memoria assignat
#define CDX_PETICIONS_PER_BLOC 50

//Longitut de cada bloc de memoria que es sol.licita
#define CDX_LONGITUT_BLOC_MEMORIA ((sizeof(struct e_peticio))*CDX_PETICIONS_PER_BLOC)

//Número màxim de blocs de memoria.
#define CDX_MAX_BLOCS_MEMORIA (1+(CDX_MAX_PETICIONS_SIMULTANIES/CDX_PETICIONS_PER_BLOC))

//Estructura d'un bloc de memoria assignat
struct e_cdx_bloc_memoria {
	unsigned char *direccio;							//adreça del bloc assignat
	int peticions_assignades;		//número de peticions assignades a aquest bloc
};

//Vector on es guarda la informacio de cada bloc de memoria assignat
struct e_cdx_bloc_memoria blocs_memoria[CDX_MAX_BLOCS_MEMORIA];

//Número de blocs assignats
int blocs_assignats=0;

//Demanar un bloc de memoria mes
//Retorna:
//-1 si hi ha error al assignar memoria
//0 si no hi ha error
int assignar_bloc_memoria
(void)
{
	char *p;

	if (blocs_assignats==CDX_MAX_BLOCS_MEMORIA) {
		printk ("CDX: Error: S'han assignat el maxim de blocs de memoria\n");
		return -1;
  }
	p=kmalloc(CDX_LONGITUT_BLOC_MEMORIA,GFP_KERNEL);
	if (p==NULL) {
		printk ("CDX: Error al assignar bloc de memoria\n");
		return -1;
  }

#ifdef DEBUG
	printk ("CDX: Bloc numero %d assignat a la direccio %p\n",blocs_assignats,p);
#endif
	blocs_memoria[blocs_assignats].direccio=p;
	blocs_memoria[blocs_assignats].peticions_assignades=0;

	blocs_assignats++;

	return 0;
}

//Liberar tots els blocs de memoria assignats
//Aixo es fa nomes al treure el modul de memoria, o al posar-lo
//en estat CDX_ESTAT_CONTROL_INICI
void liberar_blocs_memoria
(void)
{

	int i;
	for (i=0;i<blocs_assignats;i++) {
#ifdef DEBUG
		printk ("CDX: Liberant bloc memoria %d assignat a la direccio %p\n",
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
		if (assignats==CDX_PETICIONS_PER_BLOC) {
			//S'ha de demanar un bloc nou
			r=assignar_bloc_memoria();
			if (r==-1) return 0;
			assignats=0;
		}
	}

	if (CDX_PETICIONS_PER_BLOC*(blocs_assignats-1)+assignats==CDX_MAX_PETICIONS_SIMULTANIES) {
		printk ("CDX: Error: S'ha assignat el maxim de peticions\n");
		return 0;
  }

	offset=assignats*sizeof(struct e_peticio);

	blocs_memoria[blocs_assignats-1].peticions_assignades++;

	p=blocs_memoria[blocs_assignats-1].direccio+offset;
#ifdef DEBUG
	printk ("CDX: Assignada peticio %d ",assignats);
	printk ("bloc %d a direccio %p\n",blocs_assignats-1,p);
#endif
	return p;

}

//Afegeix una peticio a la seguent posicio lliure
//Comprova cada peticio, una per una, fins trobar la primera amb
//l'atribut estat==CDX_PETICIO_LLIURE
//Si no hi ha cap, assignar una de nova
//Retorna: Punter a la nova peticio, posant el seu estat a CDX_PETICIO_PENDENT
struct e_peticio *afegir_peticio
(void)
{
	struct e_peticio *p,*anterior;//,*seguent;

	if (blocs_assignats==0) {
	  //No hi ha cap peticio feta. Assignar una de nova
		p=(struct e_peticio *)assignar_peticio();
		if (p==NULL) return NULL;
		p->estat=CDX_PETICIO_PENDENT;
		p->seguent=NULL;

#ifdef DEBUG
		printk ("CDX: Afegida peticio (inicial) direccio: %p\n",p);
#endif
		return p;
	}

	anterior=NULL;
	p=(struct e_peticio *)blocs_memoria[0].direccio;

	do {
		if (p->estat==CDX_PETICIO_LLIURE) {
			p->estat=CDX_PETICIO_PENDENT;

#ifdef DEBUG
			printk ("CDX: Afegida peticio (lliure) direccio: %p\n",p);
#endif
			return p;
    	}
		anterior=p;
		p=p->seguent;

	} while (p!=NULL);

	//No hi ha cap peticio lliure. Assignar una de nova
	p=(struct e_peticio *)assignar_peticio();
	if (p==NULL) return NULL;
	p->estat=CDX_PETICIO_PENDENT;
	p->seguent=NULL;

	anterior->seguent=p;
#ifdef DEBUG
	printk ("CDX: Afegida peticio (nova) direccio: %p\n",p);
#endif

	return p;

}

//Diu si el dispositiu és valid (esta dins el rang)
int si_dispositiu_valid
(int minor)
{
  return (minor>=0 && minor<CDX_CONTROL);
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
	for (i=0;i<CDX_MAX_CLIENTS_FILL;i++) {
		if (fills_client[i].pid==0) { //Afegir-ho aqui
			buffer_intercanvi=kmalloc(CDX_TAMANY_BUFFER_INTERCANVI,GFP_KERNEL);
			if (buffer_intercanvi==NULL) {
				printk ("CDX: Error al assignar buffer d'intercanvi pel client\n");
				return -1;
			}

			fills_client[i].pid=pid;
			fills_client[i].file=file;
			fills_client[i].buffer_intercanvi=buffer_intercanvi;

			//Es major que pos_max_client?
			if (i>pos_max_client) pos_max_client=i;
#ifdef DEBUG
			printk ("CDX: Afegit client fill pid %d a la posicio %d , buffer_intercanvi: %p\n"
							,pid,i,buffer_intercanvi);
#endif
			return 0;
		}
	}
	//Error, no hi ha cap disponible
	printk ("CDX: S'ha arribat al maxim de fills del client\n");
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
				printk ("CDX: Client fill trobat file:%p pid:%d\n",
								fills_client[i].file,fills_client[i].pid);
#endif
				return &fills_client[i];
			}
		}
	}

	//Error, fill no trobat
#ifdef DEBUG
	printk ("CDX: No s'ha trobat el client fill de file:%p\n",file);
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
			printk ("CDX: Client fill trobat file:%p pid:%d\n",
							fills_client[i].file,fills_client[i].pid);
#endif
			return &fills_client[i];
		}
	}
	//Error, fill no trobat
#ifdef DEBUG
	printk ("CDX: No s'ha trobat el client fill de pid:%u\n",pid);
#endif
	return NULL;

}


//Elimina l'entrada d'un client fill
//Usat per eliminar_fill i inicialitzar_cdx
//Entrada: index del vector de fills
//Retorna: 0 si tot va be
//-1 en cas d'error
int eliminar_fill_index
(int i)
{

	if (fills_client[i].pid!=0) {
#ifdef DEBUG
		printk ("CDX: Client fill trobat file:%p pid:%d buffer_intercanvi:%p->Eliminat\n",
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
	printk ("CDX: No s'ha trobat el client fill de file:%p per eliminarlo\n",file);
#endif
	return -1;
}



//Funció per inicialitzar i/o reiniciar les estructures de dades i variables
void inicialitzar_cdx
(void)
{
	int i;

	cdx_estat_control=CDX_ESTAT_CONTROL_INICI;

	for (i=0;i<=pos_max_client;i++) {
		eliminar_fill_index(i);
	}

	pos_max_client=0;
	cdx_estat_control=CDX_ESTAT_CONTROL_INICI;
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
			if (o==CDX_ORDRE_READ || o==CDX_ORDRE_WRITE ||
				o==CDX_ORDRE_IOCTL || o==CDX_ORDRE_LLSEEK) {
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
			if (o==CDX_ORDRE_READ || o==CDX_ORDRE_WRITE ||
				o==CDX_ORDRE_IOCTL || o==CDX_ORDRE_LLSEEK) {
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


//Escriu la direcció d'una comanda ioctl
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
	printk("CDX: proces %d dormint\n",current->pid);
#endif
	current->state=TASK_INTERRUPTIBLE;
	schedule();
}

//Ordre d'apertura de dispositiu
int device_open
(struct inode *inode,struct file *file)
{

	unsigned char minor;				//valor minor del dispositiu
	struct e_peticio *peticio;		//punter cap a la petició assignada
	char *p;										//punter auxiliar

	int valor_retorn;						//valor de retorn de l'ordre


#ifdef DEBUG
	printk ("CDX: Dispositiu obert (inode:%p,file:%p)\n", inode, file);
	printk("CDX: Dispositiu %d.%d\n",
				MAJOR(inode->i_rdev), MINOR(inode->i_rdev) );
#endif

	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==CDX_CONTROL) {
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
			printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_OPEN;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_OPEN;

		p=peticio->buffer_transmissio;

		//Dades a retornar al client

		VALOR_file(p)=file;
		VALOR_unsigned_char(p+4)=minor;
		VALOR_unsigned_int(p+4+1)=file->f_flags;
		VALOR_unsigned_int(p+4+1+4)=file->f_mode;

#ifdef DEBUG
		printk ("CDX: Peticio open minor: %d, file: %p\n",minor,file);
		printk ("CDX: f_flags:%u f_mode:%o\n",file->f_flags,file->f_mode);
#endif

		//Avisem al client pare
		if (kill_proc(pid_client,CDX_SENYAL_PETICIO_OPEN,1)<0) {
			//Error, no s'ha pogut avisar al client, reiniciar cdx
			printk ("CDX: Error, no s'ha pogut avisar al client\n");
			return -EINVAL;
		}

		//I posem al proces que ha fet la peticio en estat d'espera

		dormir_proces();

#ifdef DEBUG
		printk ("CDX: proces %d despertat\n",current->pid);
#endif



		if (peticio->estat==CDX_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("CDX: Valor de retorn: %d\n",valor_retorn);
#endif
		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("CDX: Ordre open interrompuda\n");
#endif

		}

		peticio->estat=CDX_PETICIO_LLIURE;


		//El valor de retorn, normalment sera <0 si hi ha error, o
		//0 si no hi ha. De totes maneres, protegim el modul perque
		//no retorni valors positius (no error), ja que si no es poden
		//produir resultats inesperats
		if (valor_retorn>=0) return SUCCESS;
		else {
			//Llavors, eliminar el client fill
#ifdef DEBUG
			printk ("CDX: Eliminar client fill ja que open ha retornat error\n");
#endif
			if (eliminar_fill(file)<0) {
#ifdef DEBUG
				printk ("CDX: Error al intentar eliminar fill amb file %p\n",file);
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
	struct e_peticio *peticio;		//punter cap a la petició assignada

	int valor_retorn;						//valor de retorn de la ordre
	int pid;											//pid del client fill


	minor=MINOR(file->f_dentry->d_inode->i_rdev);

#ifdef DEBUG
  printk ("CDX: Dispositiu liberat (inode:%p,file:%p)\n", inode, file);
#endif


	if (current->pid==pid_client) {
		printk ("CDX: Client terminat.\n");

		return 0;
  }

	if (minor==CDX_CONTROL) {
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
		  printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
		  return -EINVAL;
      }

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}


		//Assignar peticio de tancament
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_CLOSE;
		peticio->estat=CDX_PETICIO_PENDENT_RETORN;
		//Aquesta peticio no necessita del client per recollir les dades

		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_OPEN;

#ifdef DEBUG
		printk ("CDX: Peticio close minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
#endif

		//Avisem al client fill corresponent

		//Buscar el fill que correspon al file
		pid=buscar_pid_fill(file);
		if (pid<0) {
			printk ("CDX: No s'ha trobat el client fill de file:%p al fer close\n",file);
			return -EIO;
		}

		if (kill_proc(pid,CDX_SENYAL_PETICIO_CLOSE,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("CDX: Error, no s'ha pogut avisar al client fill\n");
			return -EIO;
		}

		//I posem al proces que ha fet la peticio en estat d'espera

		dormir_proces();


#ifdef DEBUG
		printk ("CDX: proces %d despertat\n",current->pid);
#endif

		if (peticio->estat==CDX_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("CDX: Valor de retorn: %d\n",valor_retorn);
#endif
		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("CDX: Ordre close interrompuda\n");
#endif

		}

		peticio->estat=CDX_PETICIO_LLIURE;
		//Treure el fill de la llista de clients fill

		if (eliminar_fill(file)<0) {
#ifdef DEBUG
			printk ("CDX: Error al intentar eliminar fill amb file %p\n",file);
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
	struct e_peticio *peticio;			//punter cap a la petició assignada

	int valor_retorn;							//valor de retorn de la ordre
	int pid;												//pid del client fill

	char *p;											//punter auxiliar
	struct e_fills_client *client;	//fill que gestiona el file determinat

	int longitut_total=0;					//usat en la descomposicio en troços de les dades
	int longitut_fragment;				//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==CDX_CONTROL) {
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
		  printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
		  return -EINVAL;
		}

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}


		//Assignar peticio de lectura
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_READ;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_READ;

		p=peticio->buffer_transmissio;
		*p=CDX_ORDRE_READ;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("CDX: No s'ha trobat el client fill de file:%p al fer read\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		//Bucle que descomposa les dades en troços iguals a CDX_TAMANY_BUFFER_INTERCANVI
		do {
			if (length-longitut_total>CDX_TAMANY_BUFFER_INTERCANVI)
				longitut_fragment=CDX_TAMANY_BUFFER_INTERCANVI;
			else longitut_fragment=length-longitut_total;

			VALOR_unsigned_int(p+1)=longitut_fragment;

#ifdef DEBUG
			printk ("CDX: Peticio read minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
			printk ("CDX: longitut:%d\n",longitut_fragment);
#endif


			//Avisem al client fill corresponent

			if (kill_proc(pid,CDX_SENYAL_PETICIO_ALTRES,1)<0) {
				//Error, no s'ha pogut avisar al client fill
				printk ("CDX: Error, no s'ha pogut avisar al client fill\n");
				return -EIO;
			}


			//I posem al proces que ha fet la peticio en estat d'espera
			dormir_proces();

#ifdef DEBUG
			printk ("CDX: proces %d despertat\n",current->pid);
#endif

			if (peticio->estat==CDX_PETICIO_RETORNADA) {
				valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
				printk ("CDX: Valor de retorn: %d\n",valor_retorn);
#endif

				if (valor_retorn>0) {
					//Traspassar dades del client_fill cap al proces que ha fet la peticio

#ifdef DEBUG
					printk ("CDX: Movent %d dades de %p a %p de resultat de read\n",
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
				printk ("CDX: Ordre read interrompuda\n");
#endif
				peticio->estat=CDX_PETICIO_LLIURE;

				return valor_retorn;

			}
		peticio->estat=CDX_PETICIO_PENDENT;
		} while (longitut_total <length && valor_retorn!=0);

		peticio->estat=CDX_PETICIO_LLIURE;

		return longitut_total;


	}


}


//Ordre d'escriptura de dispositiu
ssize_t device_write
(struct file *file,const char *buffer,size_t length,loff_t *offset)
{
	unsigned char minor;					//valor minor del dispositiu
	struct e_peticio *peticio;			//punter cap a la petició assignada

	int valor_retorn;							//valor de retorn de la ordre
	int pid;												//pid del client fill

	char *p;											//punter auxiliar
	struct e_fills_client *client;	//fill que gestiona el file determinat

	int longitut_total=0;					//usat en la descomposicio en troços de les dades
	int longitut_fragment;				//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);

	if (minor==CDX_CONTROL) {
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
			printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}


		//Assignar peticio d'escriptura
		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_WRITE;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_WRITE;

		p=peticio->buffer_transmissio;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("CDX: No s'ha trobat el client fill de file:%p al fer write\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		do { //Bucle que descomposa les dades en troços iguals a CDX_TAMANY_BUFFER_INTERCANVI
			if (length-longitut_total>CDX_TAMANY_BUFFER_INTERCANVI)
				longitut_fragment=CDX_TAMANY_BUFFER_INTERCANVI;
			else longitut_fragment=length-longitut_total;

			*p=CDX_ORDRE_WRITE;
			VALOR_unsigned_int(p+1)=longitut_fragment;

#ifdef DEBUG
			printk ("CDX: Peticio write minor: %d, file: %p, pid: %d\n",minor,file,current->pid);
			printk ("CDX: longitut:%d\n",longitut_fragment);
#endif

			//Traspassar les dades del proces cap al kernel
			if (longitut_fragment) {
				//Traspassar dades del client_fill cap al proces que ha fet la peticio
#ifdef DEBUG
				printk ("CDX: Movent %u dades de %p a %p de write\n",
				longitut_fragment,buffer,client->buffer_intercanvi);
#endif
				llegir_de_usuari(client->buffer_intercanvi,
														(unsigned long)buffer,longitut_fragment);
			}

			//Avisem al client fill corresponent
			if (kill_proc(pid,CDX_SENYAL_PETICIO_ALTRES,1)<0) {
				//Error, no s'ha pogut avisar al client fill
				printk ("CDX: Error, no s'ha pogut avisar al client fill\n");
				return -EIO;
			}


			//I posem al proces que ha fet la peticio en estat d'espera
			dormir_proces();

#ifdef DEBUG
			printk ("CDX: proces %d despertat\n",current->pid);
#endif

			if (peticio->estat==CDX_PETICIO_RETORNADA) {
				valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
				printk ("CDX: Valor de retorn: %d\n",valor_retorn);
#endif

			}

			else { //Proces interromput per una senyal
				valor_retorn=-EINTR;
#ifdef DEBUG
				printk ("CDX: Ordre write interrompuda\n");
#endif
				peticio->estat=CDX_PETICIO_LLIURE;

				return valor_retorn;

			}
			peticio->estat=CDX_PETICIO_PENDENT;
			*offset +=valor_retorn;
			buffer +=valor_retorn;
			longitut_total +=valor_retorn;

		} while (longitut_total <length && valor_retorn!=0);

		peticio->estat=CDX_PETICIO_LLIURE;

		return longitut_total;


	}
	return -EINVAL;
}

//Ordre ioctl de dispositiu
int device_ioctl
(struct inode *inode, struct file *file,unsigned int cmd, unsigned long arg)
{

	unsigned char minor;						//valor minor del dispositiu
	struct e_peticio *peticio;				//punter cap a la petició assignada

	pid_t pid;												//pid del client fill


	int valor_retorn;								//valor de retorn de la ordre
	long long valor_retorn64;				//valor de retorn de la ordre llseek
	char *direccio_llegida;					//direccio on estan les dades d'una operacio read
	struct e_fills_client *client;		//fill que gestiona el file determinat

	int size;												//dades de la ordre ioctl
	int direction;										//

	char c;													//variables auxiliars
	unsigned char *punter;					//
	struct file *f;										//
	char buffer[16];								//
	unsigned int longitut_escriure;	//


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==CDX_CONTROL) {
/*
 *
 * Comunicacio amb el client pare
 *
 *
*/

		if (cmd==CDX_IOCTL_REGISTRAR_CLIENT) {
			if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
				//No esta registrat encara

				for (;index_identificacio<(sizeof(CDX_IDENTIFICACIO))+1;
					index_identificacio++) {
					//Es posa sizeof +1 per a que es llegeixi tambe el codi 0 de final

					get_user(c,(unsigned long*)(arg++));
					if (c!=identificacio[index_identificacio]) {
						printk ("CDX: Identificacio no valida\n");
						index_identificacio=0;

						return -EINVAL;
					}

				}
				cdx_estat_control=CDX_ESTAT_CONTROL_REGISTRAT;
				//Guardar el pid del client
				pid_client=current->pid;
				printk ("CDX: Client registrat amb pid: %d\n",pid_client);

				//Llavors, tornar al client.
				//El client, a partir d'ara, ha d'esperar la senyal open

				return 0;

			}
			else {
				printk ("CDX: Client ja registrat!\n");
				return -EINVAL;
			}
		}

		if (cmd==CDX_IOCTL_LLEGIR_OPEN) {
			if (current->pid!=pid_client) {
				printk ("CDX: Client no autoritzat\n");
				return -EINVAL;
			}

			//Donar al client pare el minor i el file
			//Buscar peticions que estiguin de tipus CDX_ORDRE_OPEN
			peticio=buscar_peticio(CDX_PETICIO_PENDENT,CDX_ORDRE_OPEN);
			if (peticio==NULL) {
				printk ("CDX: No hi ha cap peticio open pendent al fer CDX_IOCTL_LLEGIR_OPEN\n");
				return -EIO;
			}

			//Donar la informacio
			punter=&peticio->buffer_transmissio[0];
			escriure_a_usuari(punter,arg,CDX_LONGITUT_OPEN);

			return 0;
		}

		if (cmd==CDX_IOCTL_ASSIGNAR_FILL) {
			if (current->pid!=pid_client) {
				printk ("CDX: Client no autoritzat\n");
				return -EINVAL;
			}

			//llegir dades del fill (file,pid)
			llegir_de_usuari((char *)&f,arg,4);
			llegir_de_usuari((char *)&pid,arg+4,4);

#ifdef DEBUG
			printk ("CDX: Intentant Registrar fill PID:%d f:%p\n",pid,f);
#endif

			//Posar peticio open a estat pendent de retorn, que correspongui a f
#ifdef DEBUG
			printk ("CDX: Buscant peticio pendent file:%p\n",f);
#endif
			peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT,CDX_ORDRE_OPEN,f);
			if (peticio==NULL) {
				printk ("CDX: No hi ha cap peticio open pendent\n");
				return -EINVAL;
			}

#ifdef DEBUG
			printk("CDX: Fill PID:%d f:%p Registrat\n",pid,f);
#endif
			peticio->estat=CDX_PETICIO_PENDENT_RETORN;
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
			printk ("CDX: Client fill no autoritzat\n");
			return -EINVAL;
		}

		//Tractar les diferents comandes de comunicacio cdx<->client
		switch (cmd) {

			case CDX_IOCTL_RETORNA_OPEN:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_OPEN);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio open pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_OPEN,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio open pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=CDX_PETICIO_RETORNADA;
				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;
			break;

			case CDX_IOCTL_RETORNA_CLOSE:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_OPEN);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio close pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_CLOSE,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio close pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=CDX_PETICIO_RETORNADA;
				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case CDX_IOCTL_LLEGIR_ALTRES:
				f=client->file;

				//Donar al client fill les dades de la peticio emesa
				//Buscar peticions que siguin tipus CDX_ORDRE_READ, WRITE, IOCTL

				peticio=buscar_peticio_altres_amb_file(CDX_PETICIO_PENDENT,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio altres pendent\n");
					return -EINVAL;
				}

				//Donar la informacio
				punter=&peticio->buffer_transmissio[0];
				escriure_a_usuari(punter,arg,peticio->longitut_buffer);
				peticio->estat=CDX_PETICIO_PENDENT_RETORN;

				return 0;
			break;

			case CDX_IOCTL_RETORNA_READ:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_READ);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio read pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_READ,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio read pendent de retorn\n");
					return -EINVAL;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				direccio_llegida=(char *)VALOR_unsigned_int(&buffer[4]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=CDX_PETICIO_RETORNADA;

				if (valor_retorn>0) {
#ifdef DEBUG
					printk ("CDX: Movem dades de client %p a cdx %p\n",
									direccio_llegida,client->buffer_intercanvi);
#endif

					llegir_de_usuari(client->buffer_intercanvi,
													(unsigned long)direccio_llegida,valor_retorn);
				}

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case CDX_IOCTL_LLEGIR_WRITE:
				f=client->file;

#ifdef DEBUG
				printk ("CDX: Buscant peticio write pendent, file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_WRITE,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio write pendent\n");
					return -EIO;
				}

				longitut_escriure=VALOR_int(&peticio->buffer_transmissio[1]);
				VALOR_int(&peticio->buffer_transmissio[0])=longitut_escriure;

				if (longitut_escriure>0) {

#ifdef DEBUG
					printk ("CDX: Movem %u dades de cdx %p a client %lx\n",
									longitut_escriure,client->buffer_intercanvi,arg);
#endif

					escriure_a_usuari(client->buffer_intercanvi,
													(unsigned long)arg,longitut_escriure);
				}
				return SUCCESS;

			break;

			case CDX_IOCTL_RETORNA_WRITE:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_WRITE);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio write pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_WRITE,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio write pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				peticio->valor_retorn=valor_retorn;

				peticio->estat=CDX_PETICIO_RETORNADA;

				wake_up_process(peticio->proces);
				schedule();
				return SUCCESS;

			break;

			case CDX_IOCTL_LLEGIR_IOCTL:
				f=client->file;

#ifdef DEBUG
				printk ("CDX: Buscant peticio ioctl pendent, file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_IOCTL,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio ioctl pendent\n");
					return -EIO;
				}

				cmd=peticio->cmd;
				size=_IOC_SIZE(cmd);
				direction=_IOC_DIR(cmd);

				//Veure si s'han de transmetre dades
				if (direction==_IOC_NONE) {
					//Donar només el valor de arg
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

			case CDX_IOCTL_RETORNA_IOCTL:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_IOCTL);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio ioctl pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_IOCTL,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio ioctl pendent de retorn\n");
					return -EIO;
				}

				valor_retorn=VALOR_int(&buffer[0]);
				direccio_llegida=(char *)VALOR_unsigned_int(&buffer[4]);
				peticio->valor_retorn=valor_retorn;
				peticio->estat=CDX_PETICIO_RETORNADA;

				direction=_IOC_DIR(peticio->cmd);
				size=_IOC_SIZE(peticio->cmd);
				if (direction==_IOC_READ || direction==(_IOC_READ | _IOC_WRITE) ) {
					if (size>0 && valor_retorn>=0) {
#ifdef DEBUG
						printk ("CDX: Movem dades de client (%p) a cdx (%p)\n",
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

			case CDX_IOCTL_RETORNA_LLSEEK:
				llegir_de_usuari(buffer,arg,CDX_LONGITUT_RETORNA_LLSEEK);
				f=client->file;
#ifdef DEBUG
				printk ("CDX: Buscant peticio llseek pendent de retorn file:%p\n",f);
#endif
				peticio=buscar_peticio_amb_file(CDX_PETICIO_PENDENT_RETORN,CDX_ORDRE_LLSEEK,f);
				if (peticio==NULL) {
					printk ("CDX: No hi ha cap peticio llseek pendent de retorn\n");
					return -EINVAL;
				}

				//L'Ordre llseek és la única que retorna un enter de 64 bits
				valor_retorn64=VALOR_long_long(&buffer[0]);
				peticio->valor_retorn=valor_retorn64;

				peticio->estat=CDX_PETICIO_RETORNADA;

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
			printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
			return -EINVAL;
		}

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}


		size=_IOC_SIZE(cmd);
		direction=_IOC_DIR(cmd);

#ifdef DEBUG

		printk ("CDX: Ordre ioctl:\n");
		printk ("CDX: type:%u\n",_IOC_TYPE(cmd));
		printk ("CDX: number:%u\n",_IOC_NR(cmd));
		printk ("CDX: size:%u\n",size);
		printk ("CDX: direction:%u=",direction);
		escriu_direccio_ioctl(direction);

#endif

		//Assignar peticio de ioctl

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_IOCTL;
		peticio->file=file;
		peticio->cmd=cmd;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_IOCTL;

		punter=peticio->buffer_transmissio;
		*punter=CDX_ORDRE_IOCTL;
		VALOR_unsigned_int(punter+1)=cmd;

		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("CDX: Client fill no trobat\n");
#endif
			return -EINVAL;
		}

		//A partir d'aqui, traspassar les dades a escriure de l'espai
		//d'usuari cap el buffer_intercanvi del client_fill

		if (direction==_IOC_NONE) {
			//Donar només el valor de arg
#ifdef DEBUG
			printk ("CDX: Passem el valor %u cap el buffer_intercanvi\n",(unsigned int)arg);
#endif
			VALOR_unsigned_int(client->buffer_intercanvi)=(unsigned int)arg;
		}

		else if (direction==_IOC_WRITE || direction==(_IOC_WRITE | _IOC_READ) ) {
			if (size>0) {
				//Traspassar dades (tamany=size) de espai d'usuari (*arg) cap a
				//buffer_intercanvi.
#ifdef DEBUG
			printk ("CDX: Traspassem %u dades de %x cap el buffer_intercanvi (%p)\n",
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

		if (kill_proc(client->pid,CDX_SENYAL_PETICIO_ALTRES,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("CDX: Error, no s'ha pogut avisar al client fill\n");
			return -EIO;
		}

		//I posem al proces que ha fet la peticio en estat d'espera
		dormir_proces();


#ifdef DEBUG
		printk ("CDX: proces %d despertat\n",current->pid);
#endif

		if (peticio->estat==CDX_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("CDX: Valor de retorn ioctl: %d\n",valor_retorn);
#endif

			//Traspassar dades de kernel a proces (si fa falta)
			if (direction==_IOC_READ || direction==(_IOC_READ | _IOC_WRITE)) {
				if (size>0 && valor_retorn>=0) {
					//Traspassar dades del client_fill cap al proces que ha fet la peticio
					//
#ifdef DEBUG
					printk ("CDX: Movent %u dades de %p a %x de resultat de ioctl\n",
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
			printk ("CDX: Ordre ioctl interrompuda\n");
#endif

		}

		peticio->estat=CDX_PETICIO_LLIURE;

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
	struct e_peticio *peticio;				//punter cap a la petició assignada

	long long valor_retorn;					//valor de retorn de la ordre
	pid_t pid;												//pid del client fill
	struct e_fills_client *client;		//fill que gestiona el file determinat

	unsigned char *p;								//punter auxiliar


	minor=MINOR(file->f_dentry->d_inode->i_rdev);


	if (minor==CDX_CONTROL) {
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
		  printk ("CDX: Dispositiu %d no valid\n",minor);
#endif
		  return -EINVAL;
      }

		//Veure si esta registrat
		if (cdx_estat_control==CDX_ESTAT_CONTROL_INICI) {
			printk ("CDX: Client no registrat\n");
			return -ENODEV;
		}


#ifdef DEBUG
		printk ("CDX: Ordre llseek: offset:%Ld, whence: %d\n",offset,whence);
#endif

		//Assignar peticio de lectura

		//Notificar al client
		peticio=afegir_peticio();
		if (peticio==NULL) return -EINVAL;

		peticio->proces=current;

		//Referent a la peticio
		peticio->ordre=CDX_ORDRE_LLSEEK;
		peticio->file=file;

		//Referent a la informacio transmesa
		peticio->longitut_buffer=CDX_LONGITUT_LLSEEK;

		p=peticio->buffer_transmissio;

		*p=CDX_ORDRE_LLSEEK;
		VALOR_long_long(p+1)=offset;
		VALOR_long_long(p+1+8)=whence;

		//Buscar fill que gestiona el file
		client=buscar_fill(file);
		if (client==NULL) {
#ifdef DEBUG
			printk ("CDX: No s'ha trobat el client fill de file:%p al fer llseek\n",file);
#endif
			return -EIO;
		}
		pid=client->pid;

		//Avisem al client fill corresponent

		if (kill_proc(pid,CDX_SENYAL_PETICIO_ALTRES,1)<0) {
			//Error, no s'ha pogut avisar al client fill
			printk ("CDX: Error, no s'ha pogut avisar al client fill\n");
			return -EIO;
		}


		//I posem al proces que ha fet la peticio en estat d'espera
		dormir_proces();


#ifdef DEBUG
		printk ("CDX: proces %d despertat\n",current->pid);
#endif

		if (peticio->estat==CDX_PETICIO_RETORNADA) {
			valor_retorn=peticio->valor_retorn;
#ifdef DEBUG
			printk ("CDX: Valor de retorn llseek: %Ld\n",valor_retorn);
#endif


			if (valor_retorn>=0) {
				file->f_pos=valor_retorn;
			}

		}

		else {
			valor_retorn=-EINTR;
#ifdef DEBUG
			printk ("CDX: Ordre llseek interrompuda\n");
#endif
			peticio->estat=CDX_PETICIO_LLIURE;

		}

		peticio->estat=CDX_PETICIO_LLIURE;
		return valor_retorn;

	}


}

static int Major;


//Operacions sobre el dispositiu
struct file_operations Fops = {
	owner: THIS_MODULE,
	//Identifica el mòdul. Aixo fa que automaticament s'incrementi o decrementi
	//el "usage count" del mòdul

	llseek: device_llseek,
 	read: device_read,
	write: device_write,
	ioctl: device_ioctl,
	open: device_open,
	release: device_release
};


//Funció d'inicialització del mòdul
int init_module()
{

	int i;

	Major=CDX_MAJOR;
	if (register_chrdev(Major,DEVICE_NAME,&Fops)<0) {
		printk ("CDX: Error registrant dispositiu CDX amb numero major %d\n",Major);
		return Major;
	}

	printk ("CDX: Modul CDX registrat amb numero major %d\n",Major);
	printk ("CDX: Control CDX minor %d\n",CDX_CONTROL);

#ifdef DEBUG
	printk ("CDX: sizeof (e_peticio): %d\n",sizeof(struct e_peticio));
	printk ("CDX: Numero maxim de peticions simultanies: %d\n",CDX_MAX_PETICIONS_SIMULTANIES);
	printk ("CDX: Peticions per cada bloc de memoria: %d\n",CDX_PETICIONS_PER_BLOC);
	printk ("CDX: Longitut de cada bloc de memoria: %d\n",CDX_LONGITUT_BLOC_MEMORIA);
	printk ("CDX: Numero maxim de blocs de memoria: %d\n",CDX_MAX_BLOCS_MEMORIA);
	printk ("CDX: Tamany del buffer d'intercanvi: %d\n",CDX_TAMANY_BUFFER_INTERCANVI);
#endif

	//Inicialitzar vector de clients fill
	for (i=0;i<CDX_MAX_CLIENTS_FILL;i++) {
		fills_client[i].pid=0;
	}

	inicialitzar_cdx();

   return 0;
}


//Funcio d'eliminacio del mòdul de memòria
void cleanup_module()
{
	int ret;

	inicialitzar_cdx();

	//Liberar el modul
	ret = unregister_chrdev(Major, DEVICE_NAME);

	if (ret < 0)
		printk("CDX: Error liberant modul CDX: %d\n", ret);
	else printk ("CDX: Modul CDX liberat\n");
}
