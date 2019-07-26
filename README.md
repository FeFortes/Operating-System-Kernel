# Operating-System-Kernel
Coursework 2 of Concurrent Computing, University of Bristol

Check **CourseworkDescription.pdf** to see all the details of what the program should do

## Dependencies

We use QEMU to emulate the RealView Platform Baseboard for Cortex-A8  
(http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0417d/index.html)

## Overview

The repo contains code in both C and ASM. In a nutshell:

*	**image.ld** produces a kernel image (bss segment, data segment, text segment, allocate stack for SVC mode, allocate stack for processes)
* a Process Control Block (PCB) is maintained, storing information about each process: id, status, priority, top of stack (address), context (general purpouse registers - gpr), Program Counter, Current Program Status Register (cpsr) - interruption configuration
*	In **kernel/lolevel.s**: low level handlers save current state of execution -> execute high level handler -> reload the state of execution
*	In **kernel/hilevel.c**: a scheduler and a dispatch function implements time slicing. In a round-robin fashion, each process will be executed for more of less time depending on its priority. An improvement to the system could be implementing a scheduler with more responsiveness (e.g. choose the next process to be executed as the one with the highest priority; and changing to dynamic priority).
* In **user/libc.c**: contains functions in ASM to pass parameters to SVC (supersivor mode) interruptions and return values after its execution
	
Only the **Console** process is initialized. It interracts with the user via terminal, where the user can create/delete processes. To create processes, Fork and Exec are executed. When deleting processes, Kill is executed. If the process reaches its end, Exit is executed.   

Fork, Exec, Kill and Exit are implemented in **kernel/hilevel.c** in the routine hilevel_handler_svc.
