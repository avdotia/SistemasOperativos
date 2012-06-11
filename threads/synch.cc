// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List<Thread*>;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append(currentThread);		// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    interrupt->SetLevel(oldLevel);		// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
/*Lock::Lock(const char* debugName) {}
Lock::~Lock() {}
void Lock::Acquire() {}
void Lock::Release() {}*/
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
//////////////////////Implementar los cerrojos (clase Lock). ////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

//constructor
Lock::Lock(const char* debugName) {
    name = debugName; //nombre 
	value=0; //disponible
	queue= new List<Thread*>; //creamos la cola de procesos
	hilo=NULL; //el hilo actual no tiene cerrojo
}
//destructor
Lock::~Lock() {
	delete queue;
}

//Espera a que el cerrojo esté libre y lo adquiere. 
//Operacion sobre el cerrojo atómica, es decir hay que desactivar las interrupciones,
//y despues se vuelven a activar
void Lock::Acquire() {
	IntStatus oldLevel = interrupt->SetLevel(IntOff); //disable interrupts
    
    while (value == 1) { //mientras el cerrojo no esta disponible
		queue->Append(currentThread); //bloqueamos el proceso actual
		currentThread->Sleep();  //y lo ponemos a dormir
    } 
    value=1; // cerrojo no disponible						
	hilo = currentThread;    
    interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//Libera el cerrojo; si había alguien esperando en Acquire, 
//se lo entrega a alguno de los procesos que esperan. 
void Lock::Release() {
	
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  //disable interrupts
	if (isHeldByCurrentThread())
	{
		Thread *thread;
		thread = queue->Remove();
		if (thread != NULL)	scheduler->ReadyToRun(thread);
		hilo=NULL;
		value=0;	//cerrojo disponible	
	}else currentThread->Finish();
	interrupt->SetLevel(oldLevel);  // re-enable interrupts

}

// devuelve 'true' si el hilo actual es quien posee el cerrojo.
// util para comprobaciones en el Release() y en las variables condiciï¿œn
bool Lock::isHeldByCurrentThread(){
    return currentThread == hilo;
}

/*Condition::Condition(const char* debugName, Lock* conditionLock) { }
Condition::~Condition() { }
void Condition::Wait() { ASSERT(false); }
void Condition::Signal() { }
void Condition::Broadcast() { }*/

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////Implementar variable condición (clase Condition)////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

//constructor
Condition::Condition(const char* debugName, Lock* conditionLock) { 
	name=debugName;
	cerrojo=conditionLock;
	queue=new List<Thread*>;
}

//destructor
Condition::~Condition() {
	delete queue;
}

//Libera el cerrojo, suspende el hilo y espera a que alguien ejecute un Signal.
//Cuando el hilo se desbloquea, se vuelve a recuperar el cerrojo. 
void Condition::Wait() {
		IntStatus oldLevel = interrupt->SetLevel(IntOff);  //se desactivan las interrupciones
		if (cerrojo->isHeldByCurrentThread())
		{
			cerrojo->Release();  //liberamos el cerrojo
			queue->Append(currentThread);		// bloqueamos el hilo actual 
			currentThread->Sleep(); //lo ponemos a dormir
			cerrojo->Acquire(); //adquiere el cerrojo		
		}else currentThread->Finish();
		interrupt->SetLevel(oldLevel); //se activan las interrupciones
}

//Si hay alguien esperando por la condición, despiértalo. Si no, no hagas nada.
void Condition::Signal() {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);  //se desactivan las interrupciones
	if (cerrojo->isHeldByCurrentThread())
	{
		Thread *thread = queue->Remove(); //sacamos el primer hilo de la cola
		if (thread!=NULL)scheduler->ReadyToRun(thread);
	}else currentThread->Finish();
	interrupt->SetLevel(oldLevel); //se activan las interrupciones
}

//Despierta a todos los que esperan por la condición. 
void Condition::Broadcast() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  //se desactivan las interrupciones
	if (cerrojo->isHeldByCurrentThread())
	{
		Thread *thread = queue->Remove(); //sacamos el primer hilo de la cola
		while (thread!=NULL)
		{
			scheduler->ReadyToRun(thread); //vamos despertando a todos los que esperan
			thread = queue->Remove();
		}
	}else currentThread->Finish();
	interrupt->SetLevel(oldLevel); //se activan las interrupciones
}


