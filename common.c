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
    /* not_prime */
    Instr_Drop,          // nmax, c
    Instr_Inc,           // nmax, c+1
    Instr_Jump, -28, /* back */   // nmax, c
    /* end: */
    Instr_Halt           // nmax, c (== nmax)
};

const Instr_t Instr_Rot_Test[PROGRAM_SIZE] = {
    Instr_Push, 1,
    Instr_Push, 5,
    Instr_Push, 8,
    Instr_Rot,
    Instr_Halt
};

const Instr_t Instr_Logic_Test[PROGRAM_SIZE] = {
   Instr_Push, 1,
   Instr_Push, 2,
   Instr_Xor,
   Instr_Print,
   Instr_Push, 1,
   Instr_Push, 2,
   Instr_Or,
   Instr_Print,
   Instr_Push, 1,
   Instr_Push, 2,
   Instr_And,
   Instr_Print,
   Instr_Halt
};

const Instr_t Instr_SHx_Test[PROGRAM_SIZE] = {
   Instr_Push, 1,
   Instr_Push, 3,
   Instr_SHL,
   Instr_Print,
   Instr_Push, 1,
   Instr_Push, 3,
   Instr_SHR,
   Instr_Print,
   Instr_Halt
};

/* Choose a program we are about to simulate */
//const Instr_t* Program = Primes;
const Instr_t* Program = Instr_SHx_Test;

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


