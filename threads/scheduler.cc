//----------------------------------------------------------------------
// Cambios en scheduler.cc
//----------------------------------------------------------------------

/*clase creada para la organizaci�n, ejecuci�n y control de los hilos del sistema. 
Contiene una lista (clase List) con los hilos preparados para ser ejecutados y 
funciones miembros para ejecutar, seleccionar un nuevo hilo o insertar un hilo en 
la lista de preparados. La pol�tica de planificaci�n es FIFO: el hilo en ejecuci�n 
s�lo abandona la CPU cuando la entrega (yield) o cuando se bloquea (con un sem�foro, 
cerrojo o variable condici�n). Utiliza la funci�n switch  para el cambio de contexto.*/

// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------
//Inicializa la lista de preparados menos los hilos vacios
Scheduler::Scheduler()
{ 
    readyList = new List<Thread*>; 
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------
//Borra la lista de hilos preparados
Scheduler::~Scheduler()
{ 
    delete readyList; 
} 

//----------------------------------------------------------------------
// Cambios en ReadyToRun
//----------------------------------------------------------------------

//Cambiamos la politica de FCFS (orden de llegada) por una pol�tica de m�todo con prioridades

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());
//Pone el hilo en estado de preparado (Ready)
    thread->setStatus(READY);
//Pasa a la lista de preparados
/*El 'SortedInsert' lo que hace es insertar en una lista el elemento dado seg�n la prioridad.
Si la lista est� vac�a  es el �nico, si no recorre la lista y lo inserta donde corresponda*/
	readyList->SortedInsert(thread, thread->prio());
}

//----------------------------------------------------------------------
// Cambios en FindNextToRun
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
	int valor_prio;
/* Elimina el primer elemento de la lista ordenada y
 devuelve un puntero al elemento eliminado, 
 NULL si no hay nada en la lista.
 Establece un puntero a la ubicaci�n en la que se almacena la prioridad del
 elemento eliminado (valor_prio)
 (Esto es necesario por interrupt.cc, por ejemplo).
 */
    return (Thread*)readyList->SortedRemove(&valor_prio);
}

//----------------------------------------------------------------------
// Cambios en Run
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
// direccion del hilo antiguo, el que estaba en la CPU
    Thread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;


	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------

static void
ThreadPrint (Thread* t) {
  t->Print();
}

void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Apply(ThreadPrint);
}

