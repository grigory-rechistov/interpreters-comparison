/*  threaded-subroutined.c - a threaded interpreter for a stack virtual machine.
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
#include <math.h>

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
    case Instr_SQRT:
    case Instr_Pick:
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

typedef void (*service_routine_t)(cpu_t *pcpu, decode_t* pdecode);

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

static inline uint32_t pick(cpu_t *pcpu, int32_t pos) {
    assert(pcpu);
    if (pcpu->sp - 1 < pos) {
        printf("Out of bound picking\n");
        pcpu->state = Cpu_Break;
        return 0;
    }
    return pcpu->stack[pcpu->sp - pos];
}

cpu_t pcpu;
decode_t pdecoded;

sr_Nop:
void sr_Nop(cpu_t *pcpu, decode_t *pdecoded) {
    /* Do nothing */
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Halt:
void sr_Halt(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->state = Cpu_Halted;
    ADVANCE_PC();
    return;
}

sr_Push:
void sr_Push(cpu_t *pcpu, decode_t *pdecoded) {
    push(pcpu, pdecoded->immediate);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Print:
void sr_Print(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    printf("[%d]\n", tmp1);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Swap:
void sr_Swap(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Dup:
void sr_Dup(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp1);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Over:
void sr_Over(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp2);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Inc:
void sr_Inc(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1+1);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Add:
void sr_Add(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 + tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Sub:
void sr_Sub(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 - tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Mod:
void sr_Mod(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp2 == 0) {
        pcpu->state = Cpu_Break;
        return;
    }
    push(pcpu, tmp1 % tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Mul:
void sr_Mul(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 * tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Rand:
void sr_Rand(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = rand();
    push(pcpu, tmp1);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Dec:
void sr_Dec(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1-1);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Drop:
void sr_Drop(cpu_t *pcpu, decode_t *pdecoded) {
    (void)pop(pcpu);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Je:
void sr_Je(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp1 == 0)
        pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Jne:
void sr_Jne(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp1 != 0)
        pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_And:
void sr_And(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 & tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Or:
void sr_Or(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 | tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Xor:
void sr_Xor(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 ^ tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_SHL:
void sr_SHL(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 << tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_SHR:
void sr_SHR(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 >> tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Rot:
void sr_Rot(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    uint32_t tmp3 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp3);
    push(pcpu, tmp2);
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Jump:
void sr_Jump(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_SQRT:
void sr_SQRT(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, sqrt(tmp1));
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Pick:
void sr_Pick(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, pick(pcpu, tmp1));
    ADVANCE_PC();
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
}

sr_Break:
void sr_Break(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->state = Cpu_Break;
    ADVANCE_PC();
    /* No need to dispatch after Break */
    return;
}


int main(int argc, char **argv) {

    static void* service_routines[] = {
        &&sr_Break, &&sr_Nop, &&sr_Halt, &&sr_Push, &&sr_Print,
        &&sr_Jne, &&sr_Swap, &&sr_Dup, &&sr_Je, &&sr_Inc,
        &&sr_Add, &&sr_Sub, &&sr_Mul, &&sr_Rand, &&sr_Dec,
        &&sr_Drop, &&sr_Over, &&sr_Mod, &&sr_Jump, 
        &&sr_And, &&sr_Or, &&sr_Xor,
        &&sr_SHL, &&sr_SHR,
        &&sr_SQRT, &&sr_Rot, &&sr_Pick, NULL /* This NULL seems to be essential to keep GCC from over-optimizing? */
    };

    long long steplimit = parse_args(argc, argv);
    pcpu = init_cpu();

    uint32_t tmp1 = 0, tmp2 = 0, tmp3 = 0;
    pdecoded = fetch_decode(&pcpu);
    DISPATCH();
    
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