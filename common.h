/*  common.h - common definitons for interpreters written
    for a stack virtual machine.
    Copyright (c) 2015, 2016 Grigory Rechistov. All rights reserved.

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

#include <stdio.h>
#include <stdint.h>

#ifndef COMMON_H_
#define COMMON_H_

/* Instruction Set Architecture: 
   opcodes and arguments for individual instructions.
   Those marked with "imm" use the next machine word 
   in program memory as a signed immediate operand.
 */
enum {
Instr_Break  = 0x0000,/* Abnormal end; 
                         all unitialized memory will trigger a stop */
Instr_Nop    = 0x0001,
Instr_Halt   = 0x0002, /* Normal program end */
Instr_Push   = 0x0003, /* imm */
Instr_Print  = 0x0004,
Instr_JNE    = 0x0005, /* imm */
Instr_Swap   = 0x0006,
Instr_Dup    = 0x0007,
Instr_JE     = 0x0008, /* imm */
Instr_Inc    = 0x0009,
Instr_Add    = 0x000a,
Instr_Sub    = 0x000b,
Instr_Mul    = 0x000c,
Instr_Rand   = 0x000d,
Instr_Dec    = 0x000e,
Instr_Drop   = 0x000f,
Instr_Over   = 0x0010,
Instr_Mod    = 0x0011,
Instr_Jump   = 0x0012, /* imm */

};

typedef enum {
    Cpu_Running = 0,
    Cpu_Halted,
    Cpu_Break
} cpu_state_t;

typedef uint32_t Instr_t;

/* The code for target program for an interpreter to simulate */
#define PROGRAM_SIZE 512

extern const Instr_t* Program;

#define STACK_CAPACITY 32
/* A struct to store information about a decoded instruction */
typedef struct {
    Instr_t opcode; /* Used as an index in switch */
    int length; /* offset of the next instruction, zero for branches */
    int32_t immediate; /* the next word from program memory, if necessary,
                          otherwise undefined */
    const void *sr; /* label to a service routine */
} decode_t;

/* Use up to 16 host bytes for one guest instruction in JIT variants */
#define JIT_CODE_SIZE (PROGRAM_SIZE * 16)

/* Simulated processor state */
typedef struct {
    uint32_t pc; /* Program Counter */
    int32_t sp; /* Stack Pointer */
    cpu_state_t state;
    long long steps; /* Statistics - total number of instructions */
    uint32_t stack[STACK_CAPACITY]; /* Data Stack */
    const Instr_t *pmem; /* Program Memory */
} cpu_t;


#endif /* COMMON_H_ */
