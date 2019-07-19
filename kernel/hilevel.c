/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */



#include "hilevel.h"
#include <stdio.h>

#define MAXPROCESSES 8

uint32_t nProcesses = 1; // Counts the number of processes being executed. In the beggining, only the console is being executed
pcb_t pcb[ MAXPROCESSES ]; pcb_t* current = NULL;
int idxFreeEntry = -1;

int getNextProcess() { //Function called by the schedule to find the next process
  int pcbIndex = (current->pid)%MAXPROCESSES; //current->pid is 1 + current pcb index
  while((pcb[ pcbIndex ].status != STATUS_READY) && (pcb[ pcbIndex ].status != STATUS_CREATED))  { //find the next process with status ready
    pcbIndex = (pcbIndex + 1)%(MAXPROCESSES);
  }
  return pcbIndex;
}
int getNextFreeEntry() { //Functions called by fork to find the next free entry on the pcb table
  int pcbIndex = (current->pid)%MAXPROCESSES; //current->pid is 1 + current pcb index
  while((pcb[ pcbIndex ].status != STATUS_TERMINATED) && (pcb[ pcbIndex ].status != STATUS_NOTCREATED))  { //find the next process with status ready
    pcbIndex = (pcbIndex + 1)%(MAXPROCESSES);
  }
  return pcbIndex;
}


void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );

    current = next;                             // update   executing index   to P_{next}
    TIMER0->Timer1Load  = current->priority * 0x00100000; // (changed) select period = 2^20 ticks ~= 1 sec
  return;
}

void schedule( ctx_t* ctx ) { 
  // observation: pid starts at 1, while the indexes of pcb start at 0. 
  int current_pid = current->pid;
  int idxNextProcess;
  pcb[ current_pid-1 ].status = STATUS_READY;             // stop executing the current process
  idxNextProcess = getNextProcess();                      // process to start being executed

  dispatch( ctx, &pcb[ current_pid -1 ], &pcb[ idxNextProcess ] );  // change the process being executed    
  pcb[ idxNextProcess ].status = STATUS_EXECUTING;         // update the pcb status of the process being executed


  return;
}

extern void     main_console();
extern uint32_t tos_P;

void hilevel_handler_rst( ctx_t* ctx             ) {
  /* Initialise two PCBs, representing user processes stemming from execution
   * of two user programs.  Note in each case that
   *
   * - the CPSR value of 0x50 means the processor is switched into USR mode,
   *   with IRQ interrupts enabled, and
   * - the PC and SP values matche the entry point and top of stack.
   */
   for (int i=0; i<MAXPROCESSES; i++) { //initialise the process table
     memset( &pcb[ i ], 0, sizeof( pcb_t ) );
     pcb[ i ].status   = STATUS_NOTCREATED;
     pcb[ i ].pid      = i+1; // process i has pid i+1 
     pcb[ i ].tos = ( uint32_t ) (&tos_P) - i * 0x00001000; // top of stack
     pcb[ i ].ctx.cpsr = 0x50; // interruption cunfiguration
   }
   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise console
   pcb[ 0 ].pid      = 1;
   pcb[ 0 ].status   = STATUS_READY;
   pcb[ 0 ].ctx.cpsr = 0x50;
   pcb[ 0 ].ctx.pc   = ( uint32_t ) (&main_console); //address of main
   pcb[ 0 ].ctx.sp   = ( uint32_t ) (&tos_P) - 0 * 0x00001000;
   pcb[ 0 ].priority = (uint32_t) (0%4)+1;
   pcb[ 0 ].tos = ( uint32_t ) (&tos_P) - 0 * 0x00001000;

  /* Once the PCBs are initialised, we arbitrarily select the one in the 0-th
   * PCB to be executed: there is no need to preserve the execution context,
   * since it is is invalid on reset (i.e., no process will previously have
   * been executing).
   */

  TIMER0->Timer1Load  = 0x00100000; // (changed) select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();


  dispatch( ctx, NULL, &pcb[ 0 ] );
  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt caused by TIMER0, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    schedule( ctx ); TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}


void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {
  /* Based on the identifier (i.e., the immediate operand) extracted from the
   * svc instruction,
   *
   * - read  the arguments from preserved usr mode registers,
   * - perform whatever is appropriate for this system call, then
   * - write any return value back to preserved usr mode registers.
   */

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      schedule( ctx );

      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;

      break;
    }

      case 0x03 : { 
      //fork 
      //create new entry on PCB, for the new child process (find the first pid not used)
      //copy the context of the parent process to the child process
      if(nProcesses == MAXPROCESSES) { //the table is full; there is no space for more processes
        break;
      }
      idxFreeEntry = getNextFreeEntry();
      // memset( &pcb[ idxFreeEntry ], 0, sizeof( pcb_t ) );
      memcpy( &pcb[ idxFreeEntry ].ctx, ctx, sizeof( ctx_t ) ); //copy the context of the parent into the chil
      memcpy( (uint32_t)pcb[ idxFreeEntry ].tos - 0x00001000, (uint32_t)pcb[ current->pid -1].tos - 0x00001000, 0x00001000 ); //copy stack
      pcb[ idxFreeEntry ].pid      = idxFreeEntry + 1; 
      pcb[ idxFreeEntry ].status   = STATUS_CREATED;
      pcb[ idxFreeEntry ].ctx.sp   =  pcb[ idxFreeEntry ].tos - (pcb[ current->pid -1].tos - pcb[ current->pid -1].ctx.sp); //you cant simply just
      //copy the stack pointer of the parent process into the child process. You have to move the stack pointer in relation to the top of the stack of the process
      pcb[ idxFreeEntry ].priority = (uint32_t) (nProcesses%4)+1;

      ctx->gpr[ 0 ] = pcb[ idxFreeEntry ].pid; // returns pid for the parent
      pcb[ idxFreeEntry ].ctx.gpr[ 0 ] = 0; // returns 0 for the child
      break;
    }

    case 0x04 : { //exit
      int existStatus = (int) ctx->gpr[ 0 ]; //either successful or failure
      int current_pid = current->pid;
      int idxNextProcess;
      pcb[ current_pid-1 ].status = STATUS_TERMINATED;
      idxNextProcess = getNextProcess();

      dispatch( ctx, NULL, &pcb[ idxNextProcess ] ); //use function getNext
      pcb[ idxNextProcess ].status = STATUS_EXECUTING;
      nProcesses--;
      break;
    }

    case 0x05 : { //exec
      ctx->pc   = (uint32_t) ctx->gpr[ 0 ]; // load the address of main to the Program Counter
      ctx->sp = pcb[ current->pid -1 ].tos; // reset the stack pointer to the first position
      nProcesses++;
      break;
    }

    case 0x06 : { //kill
      int pidToKill = (int) ctx->gpr[ 0 ];
      int current_pid = current->pid;

      if(pidToKill != current_pid) {
        pcb[ pidToKill-1 ].status = STATUS_TERMINATED; //the index in the table  is pid - 1
      }
      else { //killing the current process
        schedule( ctx );
        pcb[ pidToKill - 1 ].status = STATUS_TERMINATED;
      }
      nProcesses--;
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
