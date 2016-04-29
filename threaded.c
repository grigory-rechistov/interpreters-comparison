/*  threaded.c - a threaded interpreter for a stack virtual machine.
    Copyright (c) 2015 Grigory Rechistov. All rights reserved.

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
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "common.h"

static inline Instr_t fetch(const cpu_t *pcpu) {
    assert(pcpu);
    assert(pcpu->pc < PROGRAM_SIZE);
    return pcpu->pmem[pcpu->pc];
};

static inline Instr_t fetch_checked(cpu_t *pcpu) {
    if (!(pcpu->pc < PROGRAM_SIZE)) {
        printf("PC out of bounds\n");
        pcpu->state = Cpu_Break;
        return Instr_Break;
    }
    return fetch(pcpu);
}

static inline decode_t decode(Instr_t raw_instr, const cpu_t *pcpu) {
    assert(pcpu);
    decode_t result = {0};
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
    case Instr_And:
    case Instr_Or:
    case Instr_Xor:
    case Instr_SHL:
    case Instr_SHR:
    case Instr_Rot:
        result.length = 1;
        break;
    case Instr_Push:
    case Instr_JNE:
    case Instr_JE:
    case Instr_Jump:
        result.length = 2;
        if (!(pcpu->pc+1 < PROGRAM_SIZE)) {
            printf("PC+1 out of bounds\n");
            result.length = 1;
            result.opcode = Instr_Break;
            break;
        }
        result.immediate = (int32_t)pcpu->pmem[pcpu->pc+1];
        break;
    case Instr_Break:
    default: /* Undefined instructions equal to Break */
        result.length = 1;
        result.opcode = Instr_Break;
        break;
    }
    return result;
}

static inline decode_t fetch_decode(cpu_t *pcpu) {
    return decode(fetch_checked(pcpu), pcpu);
}

/*** Service routines ***/
#define BAIL_ON_ERROR() if (cpu.state != Cpu_Running) break;

#define DISPATCH() do {\
    goto *service_routines[decoded.opcode];   \
   } while(0);
    
/*
#define DISPATCH() \
__asm__ __volatile__("mov    (%0, %1, 8), %%rcx\n" \
                     "jmpq   *%%rcx\n" \
                     :: "r"(&service_routines), "r"((uint64_t)decoded.opcode): "%rcx");\
__asm__ __volatile__("nopl 0x55667788(%%rax)":::"memory");
*/

#define ADVANCE_PC() \
    cpu.pc += decoded.length;\
    cpu.steps++; \
    if (cpu.state != Cpu_Running || cpu.steps >= steplimit) break;

static inline void push(cpu_t *pcpu, uint32_t v) {
    assert(pcpu);
    if (pcpu->sp >= STACK_CAPACITY-1) {
        printf("Stack overflow\n");
        pcpu->state = Cpu_Break;
        return;
    }
    pcpu->stack[++pcpu->sp] = v;
}

static inline uint32_t pop(cpu_t *pcpu) {
    assert(pcpu);
    if (pcpu->sp < 0) {
        printf("Stack underflow\n");
        pcpu->state = Cpu_Break;
        return 0;
    }
    return pcpu->stack[pcpu->sp--];
}


int main(int argc, char **argv) {

    static void* service_routines[] = {
        &&sr_Break, &&sr_Nop, &&sr_Halt, &&sr_Push, &&sr_Print,
        &&sr_Jne, &&sr_Swap, &&sr_Dup, &&sr_Je, &&sr_Inc,
        &&sr_Add, &&sr_Sub, &&sr_Mul, &&sr_Rand, &&sr_Dec,
        &&sr_Drop, &&sr_Over, &&sr_Mod, &&sr_Jump, 
        &&sr_And, &&sr_Or, &&sr_Xor,
        &&sr_SHL, &&sr_SHR,
        &&sr_Rot, NULL /* This NULL seems to be essential to keep GCC from over-optimizing? */
    };
    
    long long steplimit = LLONG_MAX;
    if (argc > 1) {
        char *endptr = NULL;
        steplimit = strtoll(argv[1], &endptr, 10);
        if (errno || (*endptr != '\0')) {
            fprintf(stderr, "Usage: %s [steplimit]\n", argv[0]);
            return 2;
        }
    }
    
    cpu_t cpu = {.pc = 0, .sp = -1, .state = Cpu_Running, 
                 .steps = 0, .stack = {0},
                 .pmem = Program};
    
    uint32_t tmp1 = 0, tmp2 = 0;
    uint64_t tmp3 = 0;
    decode_t decoded = fetch_decode(&cpu);
    DISPATCH();
    do {
        
        sr_Nop:
            /* Do nothing */
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Halt:
            cpu.state = Cpu_Halted;
            ADVANCE_PC();
            /* No need to dispatch after Halt */
        sr_Push:
            push(&cpu, decoded.immediate);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Print:
            tmp1 = pop(&cpu); BAIL_ON_ERROR();
            printf("[%d]\n", tmp1);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Swap:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Dup:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp1);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Over:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp2);
            push(&cpu, tmp1);
            push(&cpu, tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Inc:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1+1);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Add:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 + tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Sub:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 - tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Mod:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp2 == 0) {
                cpu.state = Cpu_Break;
                break;
            }
            push(&cpu, tmp1 % tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Mul:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 * tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Rand:
            tmp1 = rand();
            push(&cpu, tmp1);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Dec:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1-1);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Drop:
            (void)pop(&cpu);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Je:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp1 == 0)
                cpu.pc += decoded.immediate;
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Jne:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp1 != 0)
                cpu.pc += decoded.immediate;
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Jump:
            cpu.pc += decoded.immediate;
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_And:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 & tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Or:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 | tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Xor:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 ^ tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_SHL:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 << tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_SHR:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 >> tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Rot:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            tmp3 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp3);
            push(&cpu, tmp2);
            ADVANCE_PC();
            decoded = fetch_decode(&cpu);
            DISPATCH();
        sr_Break:
            cpu.state = Cpu_Break;
            ADVANCE_PC();
            /* No need to dispatch after Break */
    } while(cpu.state == Cpu_Running);
    
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
    
    return cpu.state == Cpu_Halted ||
           (cpu.state == Cpu_Running &&
            cpu.steps == steplimit)?0:1;
}
