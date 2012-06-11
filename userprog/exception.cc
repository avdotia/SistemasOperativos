// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
//#include "assert.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//                           Avanzar
//----------------------------------------------------------------------
//  Avanzar el contador de programa:
//  Incrementamos manualmente el contador de programa del MIPS para que el programa de 
//  usuario avance después de haber capturado una excepción.

//  Cuando se termina el tratamiento de una llamada al sistema, la rutina ExceptionHandler()  
//  retorna y cede el control al emulador MIPS, que continúa la ejecución. La cuestión es que el 
//  contador de progama no avanza, y por tanto el emulador volverá a ejecutar la misma instrucción 
//  que antes.Para evitar el bucle infinito, hay que avanzar el contador de programa antes de 
//  retornar de ExceptionHandler().

void avanzar()
{
    int pc = machine->ReadRegister(PCReg);
    machine->WriteRegister(PrevPCReg,pc);
    pc = machine->ReadRegister(NextPCReg);
    machine->WriteRegister(PCReg,pc);
    pc += 4;
    machine->WriteRegister(NextPCReg,pc);
}


/*Nos basamos en la función StartProcess ya que 
en esencia, esta llamada Exec() ejecutará el mismo código.
El Nachos original no soporta multiprogramación, pero sí permite cargar 
un programa de usuario en memoria y lanzarlo a ejecución. 
De ello se encarga la función StartProcess()*/
//----------------------------------------------------------------------
//                       Exec Monoprogramado
//----------------------------------------------------------------------

void
lanzanuevoproceso(void *space)
{
    AddrSpace *espacio = (AddrSpace *)space;

    espacio->InitRegisters();		// set the initial register values
    espacio->RestoreState();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(false);                      // machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

void
execmulti(const char *filename)
{
//Abre el fichero ejecutable, mediante un objeto de tipo OpenFile.
    OpenFile *executable = fileSystem->Open(filename);
    printf("\nVamos a abrir %s.\n", filename);
    AddrSpace *space;

    if (executable == NULL) {
        printf("No es posible abrir el fichero %s\n", filename);
//comprueba que es ejecutable en el caso de que no me escribe un -­1 en R2
        machine->WriteRegister(2,-1);
        return;
    }
    space = new AddrSpace(executable);
 
    // Creamos nuevo hilo para el nuevo proceso
    Thread *nuevo = new Thread("nuevo");
    nuevo->space = space;

    delete executable;			// close file

    // El nuevo hilo lo inicializamos
    nuevo->Fork(lanzanuevoproceso, space);
    // Le damos el procesador al nuevo hilo, el actual se pone a esperar
    currentThread->Yield();
}
//////

//----------------------------------------------------------------------
//                       ExceptionHandler
//----------------------------------------------------------------------

//  Construir el esqueleto de un manejador de excepciones completo, que sea capaz de reconocer 
//  todos los tipos de eventos gestionados por el Nachos: llamadas al sistema y excepciones. Si ocurre 
//  una llamada al sistema, basta con que se imprima su nombre. Si ocurre cualquier otra excepción que 
//  no sea una llamada al sistema, además se debería abortar el programa de usuario.

void ExceptionHandler(ExceptionType which)
{
//  En el registro 2 es en el que leemos y escribimos
    int type = machine->ReadRegister(2); 
//  ¿El evento que se produce es una llamada al sistema?
    switch(which)
    {
//  SyscallException: Un programa ejecuta una llamada al sistema.
    case SyscallException:
//  Con el uso del "case" lo que se pretende es organizar la estructura para saber que tipo de llamada
//	se está produciendo. Las llamadas toman los valores de 0..10. En cada una de ella se indica por 
//	pantalla su nombre; además en el caso de write and read también son implementadas. Los casos están
//	definidos en el fichero "syscall.h".
        switch(type){               
//  El HALT" se encarga de detener el sistema
        case SC_Halt:   //0
            DEBUG('a',"Se ha producido un %d del tipo %d = SC_Halt.\n", which, type);
            interrupt->Halt();  
            break;
//  El EXIT termina el proceso actual, con cierto valor de retorno, si es "0" es que sale con éxito
        case SC_Exit:   //1
            DEBUG('a',"Se ha producido un %d del tipo %d = SC_Exit.\n", which, type); 
	        interrupt->Halt();
            break;
//  El EXEC carga en memoria un programa de usuario y lo ejecuta. Devuelve un identificador de proceso.
        case SC_Exec:   //2
		{
//----------------------------------------------------------------------
//                       //MONOPROGRAMACION
//----------------------------------------------------------------------
//  (Exec sin multiprogramación) nos permite ejecutar programas distintos, 
//  pero sólo uno cada vez.	
//  Buffer se encargará de ir cogiendo carácter a carácter de  la cadena 
//  que ha pasado el usuario como parámetro. 
            char buffer[100];
			int i,j=0;
//  ReadRegister lee el contenido del registro indicado de la máquina. En este caso
//  en p es la direccion donde empezamos a escribir
            int p = machine->ReadRegister(4);
/*  Cargamos la ruta del programa a cargar en memoria caracter a caracter.
Para ello nos basamos en la implementacion que ya teniamos
desarrollada de la llamada Write().		 
hacemos un bucle en el que vamos cogiendo caracteres hasta que lleguemos 
al caracter nulo que significa el fin de la cadena*/
	      do{
//  leer de la memoria del MIPS
//  lee el byte almacenado en la dirección p e incrementa su dirección, 
//  en i se va guardando lo leido
		      machine->ReadMem (p++,1,&i);
//  guardamos en buffer caracter a caracter	
		      buffer[j++]=i; 			
	      }while ( i != '\0' );
          execmulti(buffer);
          break;
		}
//  El JOIN deja bloqueado al proceso actual hasta que el proceso con identificador id haya terminado. 
//  Devuelve el valor de retorno del proceso.
        case SC_Join:   //3
            DEBUG('a',"Se ha producido un %d del tipo %d = SC_Join.\n", which, type);
            break;
//  El CREATE crea un fichero Nachos. LLevará el nombre pasado por parámetro.
        case SC_Create:   //4
            DEBUG('a',"Se ha producido un %d del tipo %d = SC_Create.\n", which, type);
            break;
//  El OPEN abre un fichero Nachos con el nombre pasado y devuelve un identificador de fichero de Nachos
//  que podrá ser usado para leer y escribir en el fichero.
        case SC_Open:   //5
            
	        DEBUG('a',"Se ha producido un %d del tipo %d = SC_Open.\n", which, type);
            break;
//----------------------------------------------------------------------
//                              SC_Read
//----------------------------------------------------------------------
//  El READ lee un conjunto de caracteres.
        case SC_Read:   //6
        {
//  Buffer se encargará de ir cogiendo carácter a carácter de  la cadena 
//  que ha pasado el usuario como parámetro. 
            char buffer;
//  Contador i que nos indicará el tamaño de la cadena
            int i=0, p;
//  ReadRegister lee el contenido del registro indicado de la máquina. En este caso
//  en p es la direccion dónde empezamos a escribir
            p = machine->ReadRegister(4);
//  Vamos cogiendo los caracteres de la cadena de datos introducida por el usuario
//  que termina cuando se lee un terminador (valor cero) o un salto de línea (carácter '\n' o 10).
            while (((buffer=getchar()) !='\n') && (buffer != '\0'))
	        {
//  Guarda en la memoria del MIPS los caracteres leídos en la cadena que ha pasado 
//  el usuario como parámetro "buffer" en la dirección "p" 
                machine->WriteMem(p, 1,buffer);
//  Incrementamos el contador que nos indica el tamaño de la cadena
                p++;
                i++;
            }
//  Guarda en la memoria del MIPS el caracter nulo que significa el fin 
//  de la cadena en la direccion de p para indicar que termina
            machine->WriteMem(p, 1,'\0');
            machine->WriteRegister(2,i);			 
            break;
            }
//----------------------------------------------------------------------
//                            SC_Write
//----------------------------------------------------------------------
//  El WRITE Escribe un bloque de caracteres.
        case SC_Write:   //7
        {
//  Buffer contiene la cadena que se pasará al usuario como parámetro. 
            int buffer;
//  ReadRegister lee el contenido del registro indicado de la máquina. En este caso
//  en p es la direccion donde empezamos a escribir
            int p = machine->ReadRegister(4);
//  Hacemos un bucle en el que vamos cogiendo caracteres hasta que llegamos 
//  al caracter nulo ('\0') que significa el fin de la cadena
		    do 
			{
//  ReadMem. Lee de la memoria del MIPS
//  Lee el byte almacenado en la dirección p e incrementa su dirección, 
//  mientras en buffer se va guardando lo leido
		        machine->ReadMem (p++,1,&buffer);
//  Se imprime por pantalla lo guardado en buffer caracter a caracter
                printf("%c",buffer);
//  Es necesario vaciar el buffer, para ello usamos el fflush		     
			    fflush(stdout);
            }while (buffer != '\0');
            break;
            }
//  El CLOSE cierra el fichero, hemos terminado de escribir en el y de leerlo.
        case SC_Close:   //8
	        DEBUG('a',"Se ha producido un %d del tipo %d = SC_Close.\n", which, type);
            break;
//  El FORK crea un hilo con un proceso nuevo copia del anterior, pero no en la misma dirección.
        case SC_Fork:   //9
	        DEBUG('a',"Se ha producido un %d del tipo %d = SC_Fork.\n", which, type);
            
            break;
//  El YIELD hace que el hilo actual ceda el procesador
        case SC_Yield:   //10
	        DEBUG('a',"Se ha producido un %d del tipo %d = SC_Yield.\n", which, type);
            
        default:  //macro no definida IN_ASM
            DEBUG('a',"Llamada al sistema no definida\n");      
        }  //fin switch type
//  fin case syscallexception

//  Excepciones:
//  En el caso de que no sea una llamada al sistema será una excepción o una interrupción del hardware.
//  Las excepciones ocurren cuando el SO intenta hacer algo inusual.
//  El tratado de las excepciones será mediante un tipo "enum" que se encuentra en el fichero "machine.h.
//  Los tipos de excepciones que encontramos son:

//  NoException: Indica que todo va bien.
    case NoException:
        break;
//  PageFaultException: no se encuentra traducción válida, es decir, fallo cuando se accede a memoria virtual
//  ya que no existe o no está bien la dirección de memoria a la que se accedía. 
    case PageFaultException:
        DEBUG('a',"Se ha producido una excepción de tipo %d , PageFault \n",which);
        ASSERT(false);
//  ReadOnlyException: Se ha intentado escribir en una porción de memoria de sólo lectura. 
    case ReadOnlyException:
        DEBUG('a',"Se ha producido una excepción de tipo %d \n",which);
        ASSERT(false);
//  BusErrorException: La traducción devuelve una dirección de memoria física inválida. 
    case BusErrorException:
        DEBUG('a',"Se ha producido una excepción de tipo %d BusError \n",which);
        ASSERT(false);
//  AddressErrorException: Referencia no alineada o más grande que el espacio de la memoria.
//  Si las páginas son de tamaño de 4KB y accedo a la posición 6KB es un error, 
//  o si la memoria es de 128KB y accedo a la posición 132KB.
    case AddressErrorException:
        DEBUG('a',"Se ha producido una excepción de tipo %d AddressError \n",which);
        ASSERT(false);
//  OverflowException: Desbordamiento de entero en suma o resta.
    case OverflowException:
        DEBUG('a',"Se ha producido una excepción de tipo %d Overflow \n",which);
        ASSERT(false);
//  IllegalInstrException: Instrucción no implementada o reservada. NumExceptionTypes
    case IllegalInstrException:
        DEBUG('a',"Se ha producido una excepción de tipo %d IllegalInst \n",which);
        ASSERT(false);
    default:
//  En otro caso
        DEBUG('a',"Error desconocido, %d \n",which);
        ASSERT(false);
    }  //fin switch which
//  Avanzar el contador de programa:
//  Incrementamos manualmente el contador de programa del MIPS para que el programa de 
//  usuario avance después de haber capturado una excepción.
    avanzar();
}


  
