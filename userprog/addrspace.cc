// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include "bitmap.h"
//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

//El constructor de la clase AddrSpace realiza la carga del código y los datos del ejecutable en la memoria.
BitMap pagfisica(NumPhysPages);
AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
//añadimos una variable para controlar el tamaño original
    unsigned int i, size;

//leemos el fichero , desde 0 hasta tamaño de la cabecera(leemos la cabecera)
//leemos la cabecera del archivo ejecutable.
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
//si la cabezera es la correcta, pero esta al revés...le da la vuelta
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
//calculamos el tamaño que ocupa el programa. La pila tiene reservado 1024byte
//reservamos las paginas que hacen faltan
// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

/*El numero fisico de paginas debe cambiarse porque
no podemos tomar todas, debemos saber cuantas hay libres. Gestionamos el
espacio libre. Puede realizarse con un vector indicando en el mismo que
paginas estan libres y cuales ocupadas, nos podemos apoyar en la clase
bitmap. Puede usarse para la gestion de memoria por completo.
Para poner variables globales en el nachOS normalemente se colocan en
el fichero system.h en nuestro caso se pondran en dicho fichero una vez
se sepa con seguridad que funcionan.*/
    printf("Numero de paginas necesario es: %d.\n", numPages);
    printf("Espacio ocupado: %d. ", size);
//bitmap es la variable declarada en system.h
//función NumClear() implementada en bitmap.cc que nos da el número de bits vacíos en bitmaps
    printf("Paginas disponibles: %d.\n", pagfisica.NumClear() - numPages);
//Preguntamos si el espacio que necesitamos es mayor del que hay libre
    ASSERT(numPages <= pagfisica.NumClear());		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
//Esta clase está especificada en translate.h
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
//ahora la pagina virtual es distinta de la física
//se debe buscar en cada iteracion una pagina disponible.
    pageTable[i].physicalPage = pagfisica.Find();
	pageTable[i].valid = true;
	pageTable[i].use = false;
	pageTable[i].dirty = false;
	pageTable[i].readOnly = false;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only

// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment

//limpiamos la memoria donde vamos a cargar el proceso
//solo las paginas fisicas que vamos a usar, nada de bloques
//lo hacemos por: Si el programandor no inicializa una variable,
//esto la inicializa a 0, y el otro es por seguridad, ya que borra
//datos de otros programas usados anteriormente
//pone a 0 tantos byte como indica el seg parámetro.
//el pagesize está definido en el machine.h
    bzero(machine->mainMemory+pageTable[i].physicalPage*PageSize,PageSize);
    }
    
//copiamos el código y los datos desde el archivo ejecutable a la memoria.
    int direfich = noffH.code.inFileAddr;
    unsigned int dire = numPages;
    for(int i =0;i < (dire - 1); i++) {
        executable->ReadAt(&(machine->mainMemory[pageTable[i].physicalPage*PageSize]),PageSize,direfich);
        direfich += PageSize;
    }
    
// then, copy in the code and data segments into memory
//calculamos el espacio que queda por copiar
    unsigned int ori_size = noffH.code.inFileAddr+noffH.code.size+noffH.initData.size;
    int aux =  ori_size - (dire-1)*PageSize;
//Copiamos lo que sobra en la última página
    executable->ReadAt(&(machine->mainMemory[pageTable[dire-1].physicalPage*PageSize]),aux,direfich);

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
//Liberamos las páginas en el BitMap para que puedan ser utilizadas de nuevo
//clear en bitmap.c
    for(unsigned int i=0;i<numPages;i++){
        pagfisica.Clear(pageTable[i].physicalPage);
    }
//borramos la tabla de paginas
    delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
