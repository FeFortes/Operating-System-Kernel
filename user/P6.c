// /* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
//  *
//  * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
//  * which can be found via http://creativecommons.org (and should be included as
//  * LICENSE.txt within the associated archive or repository).
//  */
//
 #include "P6.h"
//
// extern void main_P0();
// extern void main_P1();
// extern void main_P2();
// extern void main_P3();
// extern void main_P4();
// extern void main_P5();
//
int is_prime123( uint32_t x ) {
  if ( !( x & 1 ) || ( x < 2 ) ) {
    return ( x == 2 );
  }

  for( uint32_t d = 3; ( d * d ) <= x ; d += 2 ) {
    if( !( x % d ) ) {
      return 0;
    }
  }

  return 1;
}

 void main_P6() {
   //char toPrint[25] = "ABCDEFGHIJKLMNOPQRSTUVW";
   char snum[2];
   for( int i = 0; i < 25; i++ ) {




     if (i == 20) {
       int pid = fork();
     }
     itoa(snum, i);
     write( STDOUT_FILENO, snum, 2 );





     uint32_t lo = 1 <<  8;
     uint32_t hi = 1 << 16;

     for( uint32_t x = lo; x < hi; x++ ) {
       int r = is_prime123( x );
     }
   }
//   exec( &main_P0 );
//
//   pid_t pid = fork();
//   exec( &main_P1 );
//
//
   write( STDOUT_FILENO, "EXIT", 4 );
   exit( EXIT_SUCCESS );
 }
