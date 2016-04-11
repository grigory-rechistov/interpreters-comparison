/*  common.c - common code and data for all interpreter variants
    Copyright (c) 2016 Grigory Rechistov. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of interpreters-comparison nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "common.h"

/* Program to print all prime numbers < 10000 */
const Instr_t Primes[PROGRAM_SIZE] = {
    Instr_Push, 100000, // nmax (maximal number to test)
    Instr_Push, 2,      // nmax, c (minimal number to test)
    /* back: */
    Instr_Over,         // nmax, c, nmax
    Instr_Over,         // nmax, c, nmax, c
    Instr_Sub,          // nmax, c, c-nmax
    Instr_JE, +23, /* end */ // nmax, c
    Instr_Push, 2,       // nmax, c, divisor
    /* back2: */
    Instr_Over,         // nmax, c, divisor, c
    Instr_Over,         // nmax, c, divisor, c, divisor
    Instr_Swap,          // nmax, c, divisor, divisor, c
    Instr_Sub,          // nmax, c, divisor, c-divisor
    Instr_JE, +9, /* print_prime */ // nmax, c, divisor
    Instr_Over,          // nmax, c, divisor, c
    Instr_Over,          // nmax, c, divisor, c, divisor
    Instr_Swap,          // nmax, c, divisor, divisor, c
    Instr_Mod,           // nmax, c, divisor, c mod divisor
    Instr_JE, +5, /* not_prime */ // nmax, c, divisor
    Instr_Inc,           // nmax, c, divisor+1
    Instr_Jump, -15, /* back2 */  // nmax, c, divisor
    /* print_prime: */
    Instr_Over,          // nmax, c, divisor, c
    Instr_Print,         // nmax, c, divisor
    /* not_prime: */
    Instr_Drop,          // nmax, c
    Instr_Inc,           // nmax, c+1
    Instr_Jump, -28, /* back */   // nmax, c
    /* end: */
    Instr_Halt           // nmax, c (== nmax)
};

/* Program for counting square root in integer numbers
 * with Newton-Raphson method */
const Instr_t Newton[PROGRAM_SIZE] = {
    Instr_Push, 0,      // d (decreased = false)             0
    Instr_Push, 9999,   // d, n (we are looking for sqrt(n)) 2
    Instr_Push, 1,      // d, n, x (x = sqrt(n))             4
    /* loop: */
    Instr_Over,         // d, n, x, | n                      6
    Instr_Over,         // d, n, x, | n, x                   7
    Instr_Swap,         // d, n, x, | x, n                   8
    Instr_Div,          // d, n, x, | n/x                    9
    Instr_Over,         // d, n, x, | n/x, x                 10
    Instr_Add,          // d, n, x, | x+n/x                  11
    Instr_Push, 2,      // d, n, x, | x+n/x, 2               12
    Instr_Swap,         // d, n, x, | 2, x+n/x               14
    Instr_Div,          // d, n, x, | (x+n/x)/2 = nx         15
    Instr_Over,         // d, n, x, | nx, x                  16
    Instr_Over,         // d, n, x, | nx, x, nx              17
    Instr_Sub,          // d, n, x, | nx, nx-x               18
    Instr_JE, +21, /* end */ // d, n, x, | nx                19
    Instr_Over,         // d, n, x, | nx, x                  21
    Instr_Over,         // d, n, x, | nx, x, nx              22
    Instr_Greater,      // d, n, x, | nx, nx>x               23
    Instr_JNE, +9, /* stop */ // d, n, x, | nx               24
    Instr_Push, 1,      // d, n, x, | nx, 1                  26
    Instr_Push, 0,      // d, n, x, | nx, 1, 0               28
    Instr_Set,          // 1, n, x, | nx                     30
    /* continue: */
    Instr_Swap,         // d, n, nx, x                       31
    Instr_Drop,         // d, n, nx                          32
    Instr_Jump, -29, /* loop */ // d, n, nx                  33
    /* stop: */
    Instr_Push, 0,      // d, n, x, | nx, 0                  35
    Instr_Get,          // d, n, x, | nx, d                  37
    Instr_JNE, +2, /* end */ // d, n, x, | nx                38
    Instr_Jump, -11, /* continue */     // d, n, x, | nx     40
    /* end: */
    Instr_Drop,         // d, n, x                           42
    Instr_Print,        // d, n                              43
    Instr_Halt          //                                   44
};

/* Program for sorting an array with bubblesort */
const Instr_t Bubble[PROGRAM_SIZE] = {
    Instr_Push, 24,     // N                                 0
    Instr_Push, 1,      // N, i                              2
    Instr_Push, 3,      // N, i, j                           4
    Instr_Push, 0,      // N, i, j, 0                        6
    Instr_Get,          // N, i, j, N                        8
    /* gen_num: */
    Instr_Dec,          // N, i, j, a0..ax, N-1              9
    Instr_Rand,         // N, i, j, a0..ax, N-1, a[x+1]      10
    Instr_Swap,         // N, i, j, a0..a[x+1], N-1          11
    Instr_Dup,          // N, i, j, a0..a[x+1], N-1, N-1     12
    Instr_JNE, -6, /* get_num */ // N, i, j, a0..a[x+1], N-1 13
    Instr_Drop,         // N, i, j, a0..a[N-1]               15
    /* start: */
    Instr_Push, 2,      // N, i, j, a0..a[N-1], | 2               16
    Instr_Get,          // .. | j                                 18
    Instr_Dup,          // .. | j, j                              19
    Instr_Get,          // .. | j, a[j-3]                         20
    Instr_Over,         // .. | j, a[j-3], j                      21
    Instr_Inc,          // .. | j, a[j-3], j+1                    22
    Instr_Get,          // .. | j, a[j-3], a[j-2]                 23
    Instr_Over,         // .. | j, a[j-3], a[j-2], a[j-3]         24
    Instr_Over,         // .. | j, a[j-3], a[j-2], a[j-3], a[j-2] 25
    Instr_Greater,      // .. | j, a[j-3], a[j-2], a]j-2]>a[j-3]  26
    Instr_JE, +1, /* no_swap */ // .. | j, a[j-3], a[j-2]         27
    Instr_Swap,         // .. | j, a[j-2], a[j-3]                 29
    /* no_swap: */
    Instr_Push, 2,      // .. | j, a[j-3], a[j-2], 2         30
    Instr_Get,          // .. | j, a[j-3], a[j-2], j         32
    Instr_Inc,          // .. | j, a[j-3], a[j-2], j+1       33
    Instr_Set,          // .. | j, a[j-3]                    34
    Instr_Over,         // .. | j, a[j-3], j                 35
    Instr_Set,          // N, i, j, a0..a[N-1], | j          36
    Instr_Inc,          // N, i, j, a0..a[N-1], | j+1        37
    Instr_Push, 2,      // N, i, j, a0..a[N-1], | j+1, 2     38
    Instr_Set,          // N, i, j+1, a0..a[N-1] |           40
    Instr_Push, 0,      // N, i, j+1, a0..a[N-1] | 0         41
    Instr_Get,          // N, i, j+1, a0..a[N-1] | N         43
    Instr_Inc,          // N, i, j+1, a0..a[N-1] | N+1       44
    Instr_Push, 2,      // N, i, j+1, a0..a[N-1] | N+1, 2    45
    Instr_Get,          // N, i, j+1, a0..a[N-1] | N+1, j+1  47
    Instr_Greater,      // N, i, j+1, a0..a[N-1] | j+1>N+1   48
    Instr_JE, -35, /* start */ // N, i, j+1, a0..a[N-1] |    49
    Instr_Push, 1,      // N, i, j+1, a0..a[N-1] | 1         51
    Instr_Get,          // N, i, j+1, a0..a[N-1] | i         53
    Instr_Inc,          // N, i, j+1, a0..a[N-1] | i+1       54
    Instr_Push, 1,      // N, i, j+1, a0..a[N-1] | j+1 1     55
    Instr_Set,          // N, i+1, j+1, a0..a[N-1] |         57
    Instr_Push, 3,      // N, i+1, j+1, a0..a[N-1] | 3       58
    Instr_Push, 2,      // N, i+1, j+1, a0..a[N-1] | 3, 2    60
    Instr_Set,          // N, i+1, j=3, a0..a[N-1] |         62
    Instr_Push, 0,      // N, i+1, j=3, a0..a[N-1] | 0       63
    Instr_Get,          // N, i+1, j=3, a0..a{N-1] | N       65
    Instr_Push, 1,      // N, i+1, j=3, a0..a[N-1] | N, 1    66
    Instr_Get,          // N, i+1, j=3, a0..a[N-1] | N, i+1  68
    Instr_Greater,      // N, i+1, j=3, a0..a[N-1] | N<i+1   69
    Instr_JE, -56, /* start */ // N, i+1, j=3, a0..a[N-1] |  70
    
    Instr_Push, 0,      // N, i, j, a0..a[N-1] | 0           72
    Instr_Get,          // N, i, j, a0..a[N-1] | N           74
    /* print: */
    Instr_Dec,          // N, i, j, a0..a[N-x] | N-x         75
    Instr_Swap,         // N, i, j, a0..a[N-x-1], N-x,a[N-x] 76
    Instr_Print,        // N, i, j, a0..a[N-x-1], N-x        77
    Instr_Dup,          // N, i, j, a0..a[N-x-1], N-x, N-x   78
    Instr_JNE, -6, /* print */ // N, i, j, N-x               79
    Instr_Halt,         // N, i, j, 0                        81
};

/* Choose a program we are about to simulate */
const Instr_t* Program = Primes;

/* Other programs, kept here just for reference */
const Instr_t OldProgram[PROGRAM_SIZE] = {
    Instr_Nop,
    Instr_Push, 0x11112222,
    Instr_Push, 0xf00d,
    Instr_Print,
    Instr_Push, 0x1,
    Instr_Push, 0x2,
    Instr_Push, 0x3,
    Instr_Push, 0x4,
    Instr_Swap,
    Instr_Dup,
    Instr_Inc,
    Instr_Add,
    Instr_Sub,
    Instr_Mul,
    Instr_Rand,
    Instr_Dec,
    Instr_Drop,
    Instr_Over,
    Instr_Halt,
    Instr_Break
};

const Instr_t Factorial[PROGRAM_SIZE] = {
    Instr_Push, 12, // n,
    Instr_Push, 1,  // n, a
    Instr_Swap,     // a, n
    /* back: */     // a, n
    Instr_Swap,     // n, a
    Instr_Over,     // n, a, n
    Instr_Mul,      // n, a
    Instr_Swap,     // a, n
    Instr_Dec,      // a, n
    Instr_Dup,      // a, n, n
    Instr_JNE, -8,  // a, n
    Instr_Swap,     // n, a
    Instr_Print,    // n
    Instr_Halt
};
