/*  translated-inline.c - a binary translation engine which
    inlines code for individual target instructions,
    for a stack virtual machine.
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

#define _GNU_SOURCE 1

#ifndef __x86_64__
/* The program generates machine code, only specific platforms are supported */
#error This program is designed to compile only on Intel64/AMD64 platform.
#error Sorry.
#endif

/* For dynamic function size detection */
#include <dlfcn.h>
#include <elf.h>

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

/* Entry points corresponding to guest instruction boundaries. Made global to
   be reachable from generated code. */
void* entrypoints[PROGRAM_SIZE] = {0};

/* Area for generated code. It is put into the .text section to be reachable
   from the rest of the code (relative branch to fit in 32 bits) */
/* For explanation of '#' character,
   see https://gcc.gnu.org/ml/gcc-help/2010-09/msg00088.html */
char gen_code[JIT_CODE_SIZE] /*__attribute__ ((section (".text#")))*/
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
    __asm__ __volatile__ ("jmp *%0\n"::"r"(addr):);
}

static void do_exit() {
    longjmp(return_buf, 1);
    __asm__ __volatile__("ret\n"); /* Dummy return to not confuse r2 */
}

static void exit_generated_code() {
    __asm__ __volatile__ ("call *%0\n"::"r"(&do_exit):"memory");
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

/* The program we are about to simulate */
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

const char prologue_marked[] = {0xf2, 0x66, 0x67, 0x0f, 0x1f, 0x44, 0x12, 0x34};
const char epilogue_marked[] = {0xf2, 0x66, 0x67, 0x0f, 0x1f, 0x44, 0xdf, 0xca};

/* From AMD64 ABI: "%rbp, %rbx and %r12 through %r15 belong to the calling
   function and the called function is required to preserve their values".
   As we do not know exactly what registers compiler will use, this complete
   list should have been saved in this artificial prologue, and then restored
   in an artificial epilogue.
   As code inspection shows, currently GCC compiler uses at most RBX alone.
   ICC is more aggressive with its registers use; this code will not work
   correctly with such a naive approach. */
#define MARKED_PROLOGUE() do { \
        __asm__ __volatile__ (".byte 0xf2, 0x66, 0x67, 0x0f, 0x1f, 0x44, 0x12, 0x34 \n" \
                                 :::"rsp", "memory"); \
        } while (0);

/* Restore registers saved in SAVE_CALLEE_SAVED, then compensate for RSP change
   induced by compiler, push a new return address, and finally "return". */
#define REWRITE_AND_RETURN(newpc, entrypoints) do { \
        if (newpc > PROGRAM_SIZE) exit_generated_code(); \
        __asm__ __volatile__ ("pop %%rbx\n" \
                                 "add $8, %%rsp\n" \
                                 "jmp *%0\n" \
                                 ".byte 0xf2, 0x66, 0x67, 0x0f, 0x1f, 0x44, 0xdf, 0xca \n" \
                                  :: \
                                  "r"(entrypoints[(newpc)]) \
                                  :"rbx", "rsp", "memory"); \
        } while (0);

#define MARKED_RETURN() do { \
        __asm__ __volatile__ (".byte 0xf2, 0x66, 0x67, 0x0f, 0x1f, 0x44, 0xdf, 0xca \n" \
                                 :: \
                                 :"rbx", "rsp", "memory"); \
        } while (0);

typedef void (*service_routine_t)();

void sr_Nop() {
    MARKED_PROLOGUE();
    /* Do nothing */
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Halt() {
    MARKED_PROLOGUE();
    pcpu->state = Cpu_Halted;
    ADVANCE_PC(1);
    exit_generated_code();
    MARKED_RETURN();
}

void sr_Push(int32_t immediate) {
    MARKED_PROLOGUE();
    push(pcpu, immediate);
    ADVANCE_PC(2);
    MARKED_RETURN();
}

void sr_Print() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    printf("[%d]\n", tmp1);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Swap() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Dup() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1);
    push(pcpu, tmp1);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Over() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp2);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Inc() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1+1);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Add() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 + tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}
    
void sr_Sub() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 - tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Mod() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    if (tmp2 == 0) {
        pcpu->state = Cpu_Break;
        exit_generated_code();
    }
    push(pcpu, tmp1 % tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Mul() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    push(pcpu, tmp1 * tmp2);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Rand() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = rand();
    push(pcpu, tmp1);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Dec() {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    push(pcpu, tmp1-1);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Drop() {
    MARKED_PROLOGUE();
    (void)pop(pcpu);
    ADVANCE_PC(1);
    MARKED_RETURN();
}

void sr_Je(int32_t immediate) {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);    
    if (tmp1 == 0)
        pcpu->pc += immediate;
    ADVANCE_PC(2);
    if (tmp1 == 0)
        REWRITE_AND_RETURN(pcpu->pc, entrypoints); /* Non-sequential PC change */
    MARKED_RETURN();
}

void sr_Jne(int32_t immediate) {
    MARKED_PROLOGUE();
    uint32_t tmp1 = pop(pcpu);
    if (tmp1 != 0)
        pcpu->pc += immediate;
    ADVANCE_PC(2);
    if (tmp1 != 0)
        REWRITE_AND_RETURN(pcpu->pc, entrypoints); /* Non-sequential PC change */
    MARKED_RETURN();
}

void sr_Jump(int32_t immediate) {
    MARKED_PROLOGUE();
    pcpu->pc += immediate;
    ADVANCE_PC(2);
    REWRITE_AND_RETURN(pcpu->pc, entrypoints); /* Non-sequential PC change */
    MARKED_RETURN();
}

void sr_Break() {
    MARKED_PROLOGUE();
    pcpu->state = Cpu_Break;
    ADVANCE_PC(1);
    exit_generated_code();
    MARKED_RETURN();
}

const service_routine_t service_routines[] = {
        &sr_Break, &sr_Nop, &sr_Halt, &sr_Push, &sr_Print,
        &sr_Jne, &sr_Swap, &sr_Dup, &sr_Je, &sr_Inc,
        &sr_Add, &sr_Sub, &sr_Mul, &sr_Rand, &sr_Dec,
        &sr_Drop, &sr_Over, &sr_Mod, &sr_Jump
    };

/* Find subarray in array and return pointer to its start */
static const char* find_subarray(const char* array,
                              const char *subarray,
                              int sublen) {
    const char *p = array;
    int i = 0;
    const int search_limit = 1000;
    while (i < search_limit) {
        if (!memcmp(p, subarray, sublen)) {
            return p;
        }
        p++; i++;
    }
    return NULL;
}

typedef struct {
    const char *body; /* start of block to be copied */
    size_t size; /* full size */
    intptr_t start_marker; /* offset of prologue_marked */
    intptr_t end_marker; /* offset of epilogue_marked */
} capsule_t;

uint64_t  get_function_size(const capsule_t *start) {
    return start->end_marker;
//    return 0x1000;
    //    Dl_info dlip;
//    Elf64_Sym* sym = 0;
//    int rv = dladdr1(start, &dlip, (void**)&sym, RTLD_DL_SYMENT);
//
//    assert((rv && sym) && "Function size is known");
//    return sym->st_size;
}

/*static*/ void translate_program(const Instr_t *prog,
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
    
    /* Make capsules from service routines */
    const int sr_amount
        = sizeof(service_routines)/ sizeof(service_routines[0]);
    capsule_t *capsules = calloc(sr_amount, sizeof(capsule_t));
    assert(capsules);
    for (int i=0; i<sr_amount; i++) {
        const char *start_capsule = find_subarray((const char*)service_routines[i], prologue_marked, sizeof(prologue_marked));
        const char *end_capsule   = find_subarray((const char*)service_routines[i], epilogue_marked, sizeof(epilogue_marked));
        assert(start_capsule && "Malformed capsule prologue");
        assert(end_capsule && "Malformed capsule epilogue");
        capsules[i].body = (char*)service_routines[i];
        capsules[i].start_marker = start_capsule - capsules[i].body;
        capsules[i].end_marker = end_capsule - capsules[i].body;
        capsules[i].size = get_function_size(&capsules[i]);
        printf("Debug: fcn %d, body %p, size %lu,"
               " start_marker %lu, end_marker %lu\n",
               i, capsules[i].body, capsules[i].size, capsules[i].start_marker,
               capsules[i].end_marker);
    }

    
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
        
        int sr_length = capsules[decoded.opcode].size;
        const char *sr = capsules[decoded.opcode].body
                          + capsules[decoded.opcode].start_marker;

        assert(cur + sr_length - out_code < JIT_CODE_SIZE);
        memcpy(cur, sr, sr_length);
        i += decoded.length;
        cur += sr_length;

        /* TODO insert jmp to the end of the */

//        assert(cur + jmp_template_size - out_code < JIT_CODE_SIZE);
//        memcpy(cur, jmp_template_code, jmp_template_size);
//        intptr_t offset = (intptr_t)
//        if (offset != (intptr_t)(int32_t)offset) {
//            fprintf(stderr, "Offset to service routine for opcode %d"
//            " does not fit in 32 bits. Cannot generate code for it, sorry",
//            decoded.opcode);
//            exit(2);
//        }
//        uint32_t offset32 = (uint32_t)offset;
//        /* Patch template with correct offset */
//        memcpy(cur + 1, &offset, 4);
//        i += decoded.length;
//        cur += jmp_template_size;
    }
    printf("Debug: total generated code size %lu\n", cur - out_code);
    __asm__ __volatile__ ("int3\n");
}

int main(int argc, char **argv) {    
    if (argc > 1) {
        char *endptr = NULL;
        steplimit = strtoll(argv[1], &endptr, 10);
        if (errno || (*endptr != '\0')) {
            fprintf(stderr, "Usage: %s [steplimit]\n", argv[0]);
            exit(2);
        }
    }
    
    cpu_t cpu = {.pc = 0, .sp = -1, .state = Cpu_Running, 
                 .steps = 0, .stack = {0},
                 .pmem = Primes};
    pcpu = &cpu;
    
    /* Code section is protected from writes by default, un-protect it */
    if (mprotect(gen_code, JIT_CODE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        perror("mprotect");
        exit(2);
    }
    /* Pre-populate resulting code buffer with INT3 (machine code 0xCC).
       This will help to catch jumps to wrong locations */
    memset(gen_code, 0xcc, JIT_CODE_SIZE);
    
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
    
    return cpu.state == Cpu_Halted ||
           (cpu.state == Cpu_Running &&
            cpu.steps == steplimit)?0:1;
}
