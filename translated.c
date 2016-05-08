/*  translated.c - a binary translation sample engine
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

#ifndef __x86_64__
/* The program generates machine code, only specific platforms are supported */
#error This program is designed to compile only on Intel64/AMD64 platform.
#error Sorry.
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <setjmp.h>

#include "common.h"

/* setjmp/longjmp context buffer to be reachable from within generated code */
static jmp_buf return_buf;

/* Global pointer to be accessible from generated code.
   Uses GNU extension to statically occupy host R15 register. */
register cpu_t * pcpu asm("r15");

/* Area for generated code. It is put into the .text section to be reachable
   from the rest of the code (relative branch to fit in 32 bits) */
/* For explanation of '#' character,
   see https://gcc.gnu.org/ml/gcc-help/2010-09/msg00088.html */
char gen_code[JIT_CODE_SIZE] __attribute__ ((section (".text#")))
                             __attribute__ ((aligned(4096)));

/* TODO:a global - not good. Should be moved into cpu state or somewhere else */
static long long steplimit = LLONG_MAX;

static inline decode_t decode_at_address(const Instr_t* prog, uint32_t addr) {
    assert(addr < PROGRAM_SIZE);
    decode_t result = {0};
    Instr_t raw_instr = prog[addr];
    result.opcode = raw_instr;
    switch (raw_instr) {
    case Instr_Nop:
    case Instr_Halt:
    case Instr_Print:
    case Instr_Swap:
    case Instr_Dup:
    case Instr_Inc:
    case Instr_Add:
    case Instr_Sub:
    case Instr_Mul:
    case Instr_Rand:
    case Instr_Dec:
    case Instr_Drop:
    case Instr_Over:
    case Instr_Mod:
        result.length = 1;
        break;
    case Instr_Push:
    case Instr_JNE:
    case Instr_JE:
    case Instr_Jump:
        result.length = 2;
        assert(addr+1 < PROGRAM_SIZE);
        result.immediate = (int32_t)prog[addr+1];
        break;
    case Instr_Break:
    default: /* Undefined instructions equal to Break */
        result.length = 1;
        result.opcode = Instr_Break;
        break;
    }
    return result;
}

static void enter_generated_code(void* addr) {
    __asm__ __volatile__ ( "jmp *%0"::"r"(addr):);
}

static void exit_generated_code() {
    longjmp(return_buf, 1);
}

/*** Service routines ***/

#define ADVANCE_PC(length) do {\
    pcpu->pc += length;\
    pcpu->steps++; \
    if (pcpu->state != Cpu_Running || pcpu->steps >= steplimit) \
        exit_generated_code(); \
} while(0);

static inline void push(cpu_t *pcpu, uint32_t v) {
    assert(pcpu);
    if (pcpu->sp >= STACK_CAPACITY-1) {
        printf("Stack overflow\n");
        pcpu->state = Cpu_Break;
        exit_generated_code();
    }
    pcpu->stack[++pcpu->sp] = v;
}

static inline uint32_t pop(cpu_t *pcpu) {
    assert(pcpu);
    if (pcpu->sp < 0) {
        printf("Stack underflow\n");
        pcpu->state = Cpu_Break;
        exit_generated_code();
    }
    return pcpu->stack[pcpu->sp--];
}

typedef void (*service_routine_t)();

void sr_Nop() {
    /* Do nothing */
    ADVANCE_PC(1);
}

void sr_Halt() {
    pcpu->state = Cpu_Halted;
    ADVANCE_PC(1);
    exit_generated_code();
}

void sr_Push(int32_t immediate) {
    push(pcpu, immediate);
    ADVANCE_PC(2);
}

void sr_Print() {
    uint32_t tmp1 = pop(pcpu);
    printf("[%d]\n", tmp1);
    ADVANCE_PC(1);
}

void sr_Swap() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC(1);
}

void sr_Dup() {
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1);
    push(pcpu, tmp1);
    ADVANCE_PC(1);
}

void sr_Over() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp2);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC(1);
}

void sr_Inc() {
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1+1);
    ADVANCE_PC(1);
}

void sr_Add() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 + tmp2);
    ADVANCE_PC(1);
}

void sr_Sub() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 - tmp2);
    ADVANCE_PC(1);
}

void sr_Mod() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    if (tmp2 == 0) {
        pcpu->state = Cpu_Break;
        exit_generated_code();
    }
    push(pcpu, tmp1 % tmp2);
    ADVANCE_PC(1);
}

void sr_Mul() {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 * tmp2);
    ADVANCE_PC(1);
}

void sr_Rand() {
    uint32_t tmp1 = rand();
    push(pcpu, tmp1);
    ADVANCE_PC(1);
}

void sr_Dec() {
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1-1);
    ADVANCE_PC(1);
}

void sr_Drop() {
    (void)pop(pcpu);
    ADVANCE_PC(1);
}

void sr_Je(int32_t immediate) {
    uint32_t tmp1 = pop(pcpu);
    if (tmp1 == 0)
        pcpu->pc += immediate;
    ADVANCE_PC(2);
    if (tmp1 == 0) /* Non-sequential PC change */
        exit_generated_code();
}

void sr_Jne(int32_t immediate) {
    uint32_t tmp1 = pop(pcpu);
    if (tmp1 != 0)
        pcpu->pc += immediate;
    ADVANCE_PC(2);
    if (tmp1 != 0) /* Non-sequential PC change */
        exit_generated_code();
}

void sr_Jump(int32_t immediate) {
    pcpu->pc += immediate;
    ADVANCE_PC(2);
    /* Non-sequential PC change */
    exit_generated_code();
}

void sr_Break() {
    pcpu->state = Cpu_Break;
    ADVANCE_PC(1);
    exit_generated_code();
}

const service_routine_t service_routines[] = {
        &sr_Break, &sr_Nop, &sr_Halt, &sr_Push, &sr_Print,
        &sr_Jne, &sr_Swap, &sr_Dup, &sr_Je, &sr_Inc,
        &sr_Add, &sr_Sub, &sr_Mul, &sr_Rand, &sr_Dec,
        &sr_Drop, &sr_Over, &sr_Mod, &sr_Jump
    };

static void translate_program(const Instr_t *prog,
                           char *out_code, void **entrypoints, int len) {
    assert(prog);
    assert(out_code);
    assert(entrypoints);

    /* An IA-32 instruction "MOV RDI, imm32" is used to pass a parameter
       to a function invoked by a following CALL. */
#ifdef __CYGWIN__ /* Win64 ABI, use RCX instead of RDI */
    const char mov_template_code[]= {0x48, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00};
#else
    const char mov_template_code[]= {0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00};
#endif
    const int mov_template_size = sizeof(mov_template_code);

    /* An IA-32 instruction "CALL rel32" is used as a trampoline to invoke
       service routines. A template for it is "call .+0x00000005" */
    const char call_template_code[] = { 0xe8, 0x00, 0x00, 0x00, 0x00 };
    const int call_template_size = sizeof(call_template_code);

    int i = 0; /* Address of current guest instruction */
    char* cur = out_code; /* Where to put new code */

    /* The program is short, so we can translate it as a whole.
       Otherwise, some sort of lazy decoding will be required */
    while (i < len) {
        decode_t decoded = decode_at_address(prog, i);
        entrypoints[i] = (void*) cur;

        if (decoded.length == 2) { /* Guest instruction has an immediate */
            assert(cur + mov_template_size - out_code < JIT_CODE_SIZE);
            memcpy(cur, mov_template_code, mov_template_size);
            /* Patch template with correct immediate value */
            memcpy(cur + 3, &decoded.immediate, 4);
            cur += mov_template_size;
        }

        assert(cur + call_template_size - out_code < JIT_CODE_SIZE);
        memcpy(cur, call_template_code, call_template_size);
        intptr_t offset = (intptr_t)service_routines[decoded.opcode]
                            - (intptr_t)cur - call_template_size;
        if (offset != (intptr_t)(int32_t)offset) {
            fprintf(stderr, "Offset to service routine for opcode %d"
            " does not fit in 32 bits. Cannot generate code for it, sorry",
            decoded.opcode);
            exit(2);
        }
        uint32_t offset32 = (uint32_t)offset;
        /* Patch template with correct offset */
        memcpy(cur + 1, &offset, 4);
        i += decoded.length;
        cur += call_template_size;
    }
}

int main(int argc, char **argv) {
    steplimit = parse_args(argc, argv);
    cpu_t cpu = init_cpu();

    pcpu = &cpu;

    /* Code section is protected from writes by default, un-protect it */
    if (mprotect(gen_code, JIT_CODE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        perror("mprotect");
        exit(2);
    }
    /* Pre-populate resulting code buffer with INT3 (machine code 0xCC).
       This will help to catch jumps to wrong locations */
    memset(gen_code, 0xcc, JIT_CODE_SIZE);
    void* entrypoints[PROGRAM_SIZE] = {0}; /* a map of guest PCs to capsules */

    translate_program(cpu.pmem, gen_code, entrypoints, PROGRAM_SIZE);

    setjmp(return_buf); /* Will get here from generated code. */

    while (cpu.state == Cpu_Running && cpu.steps < steplimit) {
        if (cpu.pc > PROGRAM_SIZE) {
            cpu.state = Cpu_Break;
            break;
        }
        enter_generated_code(entrypoints[cpu.pc]); /* Will not return */
    }

    assert(cpu.state != Cpu_Running || cpu.steps == steplimit);
    /* Print CPU state */
    printf("CPU executed %lld steps. End state \"%s\".\n",
            cpu.steps, cpu.state == Cpu_Halted? "Halted":
                       cpu.state == Cpu_Running? "Running": "Break");
    printf("PC = %#x, SP = %d\n", cpu.pc, cpu.sp);
    printf("Stack: ");
    for (int32_t i=cpu.sp; i >= 0 ; i--) {
        printf("%#10x ", cpu.stack[i]);
    }
    printf("%s\n", cpu.sp == -1? "(empty)": "");

    free(LoadedProgram);

    return cpu.state == Cpu_Halted ||
           (cpu.state == Cpu_Running &&
            cpu.steps == steplimit)?0:1;
}
