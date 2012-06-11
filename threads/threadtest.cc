// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create several threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Parts from Copyright (c) 2007-2009 Universidad de Las Palmas de Gran Canaria
//

#include <unistd.h>
#include "copyright.h"
#include "system.h"
#include <time.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////
//añadimos librerías que nos hacen falta
#include "synch.h"
#include "math.h"
#include "list.h"
///////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 10 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"name" points to a string with a thread name, just for
//      debugging purposes.
//----------------------------------------------------------------------


//----------------------------------------------------------------------
//----------------------------------------------------------------------
//                      Modelo Buffer limitado     
//----------------------------------------------------------------------
//----------------------------------------------------------------------

//Implementar un modelo de búfer finito
#define tam_buffer 100
//declaramos los semaforos
//Un semáforo mutex provee exclusión mutua para el
//acceso al pool de buffers
Semaphore mutex("Controlador",1);  //exclusión mutua en el acceso al buffer
//declaramos un semaforo full que nos indica el recuento del número de celdas llenas
Semaphore full("Llenos",0); 
//declaramos un semaforo empty que nos indica el recuento del número de celdas vacías
Semaphore empty("Vacios",tam_buffer);  
typedef int bufferlimite;
bufferlimite buffer[tam_buffer];
//Observe que las modificaciones de las variables sale y entra no son secciones críticas, 
//puesto que cada variable es modificada por un solo proceso.
int entra = 0, sale = 0;



//----------------------------------------------------------------------
//                           ConsumidorLimitado
//----------------------------------------------------------------------

/*Si puede saca lo que hay en el buffer*/
void consumidorlimitado(void* valor)
{	
	do {
		bufferlimite buf;
		full.P(); //semaforo llenos
		mutex.P(); //semaforo de exclusion mutua
		//remueve un item desde el buffer a buf
		buf=buffer[sale];
        //ponemos en pantalla el elemento a consumir del bufer
		printf("Lo que se consume es: %d\n",buf);
        //señala a la siguiente posicion de forma modular		
        sale=(sale+1)%tam_buffer; 
		mutex.V(); //Salimos de la seccion critica
		empty.V(); //Aumentamos el semaforo de vacias
	} while (1); //indica que se ejecute siempre que se pueda
}


//----------------------------------------------------------------------
//                           ProductorLimitado
//----------------------------------------------------------------------

/*Mientras puede captura cosas en el buffer */
void productorlimitado(void* valor)
{	
	do{
		empty.P(); //semaforo vacias
		mutex.P(); //semaforo de exclusion mutua
		//agregamos en la posicion entra del buffer el valor tomado mediante valor
		buffer[entra]=(int)valor;
        //señala a la siguiente posicion de forma modular		
		entra=(entra+1)%tam_buffer; 
		printf("El elemento que se agrega es: %d\n", (int)valor);
		mutex.V(); //semaforo de exclusion mutua
		full.V();//semaforo llenos
        currentThread->Yield();
	}while(1);//indica que se ejecute siempre que se pueda
}

//----------------------------------------------------------------------
//                         Procesos en equilibrio
//----------------------------------------------------------------------
/*En un sistema concurrente existen dos tipos de procesos, A y B, que compiten 
por utilizar cierto recurso. Al recurso se accede mediante la rutina de solicitarlo, 
esperar hasta que se pueda usar, usarlo y luego liberarlo.
En cualquier momento puede haber un máximo de N procesos de cualquier tipo usando 
el recurso (N es constante). Por otro lado, para que un proceso de tipo A 
pueda entrar a emplear el recurso, debe haber al menos el doble de procesos de 
tipo B que procesos de tipo A dentro del recurso. */

//declaramos la clase proceso
class Proceso{
public:
    static Lock *cerrojo;
    static Condition *condicion;
    static int contadorA;
    static int contadorB;
//funciones virtuales puras, por eso ponemos el cero, porque no se implementan
    virtual void solicita() = 0;
    virtual void libera() = 0;
};
//declaramos el cerrojo, variable condicion y los contadores de cada proceso
Lock *Proceso::cerrojo = new Lock("cerrojo");
Condition *Proceso::condicion = new Condition("variable condicion", Proceso::cerrojo);
int Proceso::contadorA = 0;
int Proceso::contadorB = 0;

class ProcesoA:public Proceso{
    void libera(){
//adquirimos el cerrojo
        cerrojo->Acquire();
//decrementamos el contador del array del proceso a
        contadorA--;
//como liberamos un proceso a, despertamos con la variable condicion a un proceso a para que pueda entrar
        condicion->Signal();
//liberamos el cerrojo
        cerrojo->Release();
    }
    void solicita(){
        cerrojo->Acquire();
//condicion para despertar al proceso a, donde el numero de hilos del proceso b ha de ser por lo menos el doble de los de A
        while (contadorA*2+1>contadorB){
//ponemos en espera los procesos A hasta que puedan acceder al recurso
//el wait se encarga del  acquire y release del cerrojo
            condicion->Wait();
        }    
        contadorA++;
        cerrojo->Release();
    }
};

class ProcesoB:public Proceso{
    void libera(){
//adquirimos el cerrojo
        cerrojo->Acquire();
//decrementamos el contador del array del proceso b
        contadorB--;
//liberamos el cerrojo
        cerrojo->Release();
    }
    void solicita(){
        cerrojo->Acquire();
        contadorB++;
//como solicita entrar un proceso b, despertamos con la variable condicion a un proceso a por si puede entrar
        condicion->Signal();
        cerrojo->Release();
    }
};


void equilibrium(void* tipo){
// ponemos el * externo para acceder al contenido de la variable tipo
//convertimos el tipo en char* 
    char t=*(char*) tipo;
    Proceso *p;
    if(t == 'A'){
        p = new ProcesoA;
    }else{
        p = new ProcesoB;
    }    
    while (true) {

        printf("Proceso %c solicita entrar al recurso. Hay %d de tipo A  y %d de tipo B dentro.\n", t, Proceso::contadorA, Proceso::contadorB);
//llamamos a la funcion solicita con el proceso que quiere entrar al recurso
        p->solicita();
//mostramos por pantalla como va avanzando el proceso equilibrado
        printf("Proceso %c accede al recurso. Hay %d de tipo A  y %d de tipo B dentro.\n", t, Proceso::contadorA, Proceso::contadorB);
//usamos el recurso
//el yield libera la cpu porque nachos no es expulsivo todavia
        currentThread->Yield();
//llamamos a la funcion libera con el proceso que quiere entrar al recurso
        p->libera();
//mostramos por pantalla como va avanzando el proceso equilibrado
        printf("Proceso %c ha liberado el recurso. Hay %d de tipo A  y %d de tipo B dentro.\n", t, Proceso::contadorA, Proceso::contadorB);
//usamos la función sleep para dormir un tiempo aleatorio el hilo
        sleep(rand() % 5);
    } 
}



//----------------------------------------------------------------------
//                           Vector
//----------------------------------------------------------------------

/*Módulo que manipula un vector de elementos enteros. 
El vector se manipulará mediante tres tipos de hilos, llamados 
«productor», «consumidor» y «reportero». */
struct vector
{
    int *vec, cont, max;
//constructor del vector
    vector() 
    {
        cont = 0;
//el tamaño maximo es de 10 elementos
        max = 10;
        vec = new int[max];
    }
//destructor del vector
    ~vector()
    {
        delete vec;
    }
} v;

//----------------------------------------------------------------------
//                           Productor
//----------------------------------------------------------------------

/*Inserta un número entero al final del vector. Si no hay hueco en el vector 
(es decir, si todos los elementos del vector están ocupados), el productor 
finaliza emitiendo el mensaje «vector lleno». Si hay hueco, almacena el valor 
en el vector y finaliza. */
void productor(void * ve)
{
    int e = (int) ve;
//si hay hueco, porque el contador es menor que el maximo
	if(v.cont<v.max) 
	{
//en el hueco del vector que marca la variable contador ponemos el elemento
	    v.vec[v.cont]=e;
//aumentamos el contador
		v.cont++;
//imprimos por pantalla el elemento que hemos introducido
		printf("Se ha añadido el elemento %d\n",e);	
	}
//si no hay hueco se lo hacemos saber al usuario
	else printf("Vector lleno, no ha podido insertarse el valor.\n");	
}

//----------------------------------------------------------------------
//                           Consumidor
//----------------------------------------------------------------------

/*Extrae el valor entero más antiguo del vector. Si no hay elementos disponibles 
(depositados previamente por un hilo productor), finaliza emitiendo el mensaje 
«vector vacío».*/
void consumidor(void* e)
{
//Si no hay elementos, le decimos al usuario que no se puede extraer nada		
	if(v.cont==0) printf("Vector vacio, no se puede extraer.\n");
	else 
	{
//se considera que el proceso mas antiguo es el primero del vector
		for(int i=1;i<=v.cont;i++) 
		{
//se desplazan todos los procesos para eliminar el primero que se supone mas antiguo
			v.vec[i-1]=v.vec[i]; 
		}
//se disminuye el contador de los elementos dle vector
		v.cont--;
//se comunica que se ha realizado con éxito la operación
		printf("Se ha extraido el elemento mas antiguo correctamente.\n");
	}
}

//----------------------------------------------------------------------
//                           Reportero
//----------------------------------------------------------------------
/*Informa del estado del vector, especificando cuántos elementos están vacíos, 
cuántos están llenos y qué valores almacena. */
void reportero(void* e)
{
//La cuenta de el maximo de elementos que caben en el vector menos el contador de elementos 
//que tenemos nos da el número total de espacios vacíos
//El número de espacios llenos es el contador del vector
	printf("Hay %d espacios vacios\n Hay %d espacios llenos.\n",v.max-v.cont,v.cont);
//Para imprimir los valores del vector usamos un 'for' con el que lo recorremos
	printf("Los valores del vector son:\t");
	for(int i=0;i<v.cont;i++)
//vamos imprimiendo elemento a elemento y tabulamos
		printf("%d\t",v.vec[i]);
	printf("\n");
}

//----------------------------------------------------------------------
//                           SimpleThread
//----------------------------------------------------------------------
//Sólo activamos el Yield
void SimpleThread(void* name)
{
    // Reinterpret arg "name" as a string
//se pasa el argumento nombre a una string
    char* threadName = (char*)name;
    // If the lines dealing with interrupts are commented,
    // the code will behave incorrectly, because
    // printf execution may cause race conditions.
//si se comentan las lineas que tratan con las interrupciones el código no hará lo 
//que debe porque la ejecución del printf dará lugar a condiciones de carrera
    for (int num = 0; num < 10; num++) {
//Guarda el nivel viejo de interrupción(si están activas o no), y las desactiva
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
        printf("*** thread %s looped %d times\n", threadName, num);
//Restaura el nivel viejo de interrupción (las activa si lo estaban, o
//deja desactivadas si no)
    //interrupt->SetLevel(oldLevel);
//acticvamos el Yield para ir cambiando de hilo	
//el hilo actual cede el procesador
        currentThread->Yield();
    }
//Guarda el nivel viejo de interrupción(si están activas o no), y las desactiva
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
    printf(">>> Thread %s has finished\n", threadName);
//Restaura el nivel viejo de interrupción (las activa si lo estaban, o
//deja desactivadas si no)
    //interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
//                           ThreadTest
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

/*Para lanzar los hilos, se habilitará una función que se ejecutará en un bucle infinito. 
En cada iteración, la función solicitará al usuario que elija el tipo de hilo que quiere 
lanzar (productor, consumidor o reportero).
Esto lo hacemos mediante un menú en el que el usuario eligirá la opción deseada*/

void prueba (void* name)
{
/*en la prueba de hilos con prioridad
nos encontramos con que unicamente vemos*/
    char* threadname=(char*)name;
/*el nombre del hilo que se ejecuta para
comprobar que es el que debe ejecutarse.*/
	printf("%s\n",threadname);
	//currentThread-­>Yield();
}

void ThreadTest()
{
    int opcion=4, elemento;
//como el 0 lo usamos para indicar que se termina, mientras no sea cero...
	while (opcion!= 0)
	{
/*el usuario ha de elegir que opción usar
1- para el productor. Inserta un número entero al final del vector si puede.
2- para el consumidor. Extrae el valor entero más antiguo del vector.
3- para el reportero. Informa del estado del vector, especificando cuántos elementos están vacíos, 
cuántos están llenos y qué valores almacena.
4- para el uso de prioridad de hilos
5- para hacer la prueba ping-pong de 10 hilos*/
	    printf("Elija entre estas opciones: 0-Salir\n"); 
		printf("Actividad uno: 1-productor, 2-consumidor, 3-reportero\n"); 
		printf("Actividad dos y tres: 4-Prioridad de hilos\n");
		printf("Actividad dos y tres: 5-Prueba ping-pong diez hilos\n");
        printf("Actividad cuatro: 6-Prueba Buffer Limitado\n");
		printf("Actividad cuatro: 7-Procesos equilibrados\n");
		printf("0-Salir\n");
		scanf("%d",&opcion);
//según la opción elegida entramos en alguno de los siguientes casos	
		switch (opcion)
		{
//Cuando el caso elegido es el del productor
		    case 1:
			{
//se pide al usuario que inserte un elemento
			    printf("Escriba el elemento a insertar: \n");
				scanf("%d",&elemento);
//Se crea un hilo
				Thread * leche = new Thread("Soy la leche\n");
//El hilo creado es hijo copia del padre aunque en diferente dirección, 
//le inserta el elemento al final
				leche -> Fork(productor,(void *)elemento);
//Cambiamos el hilo actual por el siguiente y lo ponemos en la cola
				currentThread->Yield();
				break;	
			}
//Cuando el caso elegido es el del consumidor
			case 2:
			{
//se crea un hilo
				Thread * chocolate = new Thread("Soy el chocolate\n");
//el hilo hijo creado es copia del padre pero se guarda en una dirección diferente, 
//elimina el elemento mas antiguo, que es el primero
				chocolate -> Fork(consumidor,0);
//cambiamos el hilo actual que está en la CPU por el siguiente y lo ponemos en cola
				currentThread->Yield();
				break;	
			}
//Cuando el caso elegido es el de reportero
			case 3:
			{
//se crea un hilo
				Thread * azucar = new Thread("Soy el azucar\n");
//el hilo hijo creado es copia del padre pero se guarda en una dirección diferente, 
//informa del estado del vector
				azucar -> Fork(reportero,0);
//cambiamos el hilo actual que está en la CPU por el siguiente y lo ponemos en cola
				currentThread->Yield();
				break;	
			}
//Prioridad ascendente con productor, consumidor, reportero
			case 4:
			{
//se crean 3 hilos con diferentes prioridades
	        	Thread *h1 = new Thread("prod",-3);
                Thread *h2 = new Thread("cons",-6);
                Thread *h3 = new Thread("repo",-5);
		        Thread *h4 = new Thread("prod",-1);
                Thread *h5 = new Thread("cons",-7);
                Thread *h6 = new Thread("repo",-8);
//Los hilos creados son hijos copia del padre aunque en diferente dirección 
          	    h1->Fork(prueba,(void*)"hasta\n");
//currentThread->Yield();
                h2->Fork(prueba,(void*)"tal\n");
//currentThread->Yield();
                h3->Fork(prueba,(void*)"estas\n");
//currentThread->Yield();
		        h4->Fork(prueba,(void*)"luego\n");
//currentThread->Yield();
                h5->Fork(prueba,(void*)"que\n");
//currentThread->Yield();
                h6->Fork(prueba,(void*)"Hola\n");
//cambiamos el hilo actual que está en la CPU por el siguiente y lo ponemos en cola
                currentThread->Yield();
                break;
			}
//Prueba prioridad diez hilos
			case 5:
			{
				
//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------
                DEBUG('t', "Entering SimpleTest");
//se crean 10 hilos
                for ( int k=1; k<=10; k++) 
                {
                    char* threadname = new char[100];
                    sprintf(threadname, "Hilo %d", k);
                    Thread* newThread = new Thread (threadname);
//Creamos un hilo nuevo.
//De nombre "Hilo 1", "Hilo 2", ..., con el programa SimpleThread.
                    newThread->Fork (SimpleThread, (void*)threadname);
                }
//Ejecuta el SimpleThread desde el padre.    
                SimpleThread( (void*)"Hilo 0");
                currentThread->Yield();
	            break;
            }
			case 6:
			{
//Realizar pruebas sencillas con semáforos (clase Semaphore).
				printf("\nPrograma de Pruebas Buffer Limitado\n");
//se crean 10 hilos con la misma prioridad para que se puedan ejecutar aleatoriamente
//porque si uno de ellos tiene menor prioridad se va a ejecutar ese siempre antes que cualquier otro				
				Thread *t1 = new Thread("productorlimite1",-1);
				Thread *t2 = new Thread("productorlimite2",-1);
				Thread *t3 = new Thread("productorlimite3",-1);
				Thread *t4 = new Thread("productorlimite4",-1);
				Thread *t5 = new Thread("productorlimite5",-1);
				Thread *t6 = new Thread("productorlimite6",-1);
				Thread *t7 = new Thread("productorlimite7",-1);
				Thread *t8 = new Thread("consumidorlimite1",-1);
				Thread *t9 = new Thread("consumidorlimite2",-1);
				Thread *t10 = new Thread("consumidorlimite3",-1);
//ejecutamos los fork
//Los hilos creados son hijos copia del padre aunque en diferente dirección 
				t1->Fork(productorlimitado,(void*)1);
				t2->Fork(productorlimitado,(void*)2);
				t3->Fork(productorlimitado,(void*)3);
				t4->Fork(consumidorlimitado,(void*)4);
				t5->Fork(productorlimitado,(void*)5);
				t6->Fork(productorlimitado,(void*)6);
				t7->Fork(consumidorlimitado,(void*)7);
				t8->Fork(productorlimitado,(void*)8);
				t9->Fork(productorlimitado,(void*)9);
				t10->Fork(consumidorlimitado,(void*)10);
				currentThread->Yield();

				break;
			}
			case 7:
			{
//Realizamos pruebas sencillas con cerrojos y variables condicion.
//declaramos los los dos tipos de proceso
                char tipo_a = 'A', tipo_b = 'B';
				printf("\nPrograma proceso equilibrado\n");
                int tama = 10;
                int tamb = 20;
//declaramos los array de hilos 
                Thread *ha[tama];
                Thread *hb[tamb];
//usamos la función srand para inicializar la "semilla" aleatoria
                srand ( time(NULL) );
                for (int i =0; i<tama; i++)
                {
//mete un hilo en el array de hilos A  
                    ha[i] = new Thread("Proceso A",-1);
//con &tipo le pasamos la dirección de memoria de lo que guarde la variable
                    ha[i]->Fork(equilibrium,&tipo_a);
                }
                for (int i =0; i<tamb; i++)
                {  
//mete un hilo en el array de hilos B
                    hb[i] = new Thread("Proceso B",-1);
//con &tipo le pasamos la dirección de memoria de lo que guarde la variable
                    hb[i]->Fork(equilibrium,&tipo_b);
                }
				currentThread->Yield();						

				break;
			}

//Cuando el caso elegdo es para salir
			case 0:
			{
			    printf("Hasta luego cocodrilo!\n");
			    break;	
			}
//Si no es ninguno de los casos del menu
            default:
//Se indica que la elección es incorrecta
			    printf("Ese numero no es una opcion.\n");
        }//Fin switch opcion		
	}//Fin while opcion
}
