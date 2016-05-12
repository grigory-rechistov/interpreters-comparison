/*  inline_translated.c - a inline binary translation sample engine
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
#include <math.h>

#include "common.h"
#include "inline_data.h"

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

/* Supporting functions for in-lined service routines */

static void enter_generated_code(void* addr) {
    __asm__ __volatile__ ( "jmp *%0"::"r"(addr):);
}

static void exit_generated_code() {
    longjmp(return_buf, 1);
}

static inline void push(cpu_t *pcpu, uint32_t v) {
    assert(pcpu);
    if (pcpu->sp >= STACK_CAPACITY-1) {
        printf("Stack overflow\n");
        pcpu->state = Cpu_Break;
        exit_generated_code();
    }
    pcpu->stack[++pcpu->sp] = v;
}

static void inline_translate_program(const Instr_t *prog,
                           char *out_code, void **entrypoints, int len) {
    assert(prog);
    assert(out_code);
    assert(entrypoints);

    /* An IA-32 instruction "CALL rel32" is used as a trampoline to invoke
       service routines. A template for it is "call .+0x00000005" */
    const char call_template_code[] = { 0xe8, 0x00, 0x00, 0x00, 0x00 };
    const int call_template_size = sizeof(call_template_code);

    int i = 0; /* Address of current guest instruction */
    char* cur = out_code; /* Where to put new code */

    while (i < len) {
        decode_t decoded = decode_at_address(prog, i);
        entrypoints[i] = (void*) cur;

        int addr_exit1 = 0;
        int addr_exit2 = 0;
        int addr_exit3 = 0;
        int addr_puts1 = 0;
        int addr_puts2 = 0;
        int addr_push1 = 0;
        int addr_push2 = 0;
        int addr_push3 = 0;
        int addr_abort = 0;
        int addr_printf = 0;
        int addr_rand = 0;
        int addr_str1 = (intptr_t)&str_printf;
        int addr_str2 = (intptr_t)&str_pop;
        int addr_str3 = (intptr_t)&str_push;
        int len = 0;
        int imm = 0;

        switch(decoded.opcode) {
        case Instr_Nop:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x2b;
            len = sizeof(bin_sr_Nop);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Nop, len);
            memcpy(cur+0x2c,&addr_exit1, 4);
            break;
        case Instr_Halt:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x11;
            len = sizeof(bin_sr_Halt);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Halt, len);
            memcpy(cur+0x12,&addr_exit1, 4);
            break;
        case Instr_Print:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x57;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x6f;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x61;
            addr_printf = (intptr_t)&printf - (intptr_t)cur - call_template_size - (intptr_t)0x24;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x74;
            len = sizeof(bin_sr_Print);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Print, len);
            memcpy(cur+0x58,&addr_exit1, 4);
            memcpy(cur+0x70,&addr_exit2,4);
            memcpy(cur+0x25,&addr_printf,4);
            memcpy(cur+0x62,&addr_puts1,4);
            memcpy(cur+0x75,&addr_abort,4);
            memcpy(cur+0x1a,&addr_str1,4);
            memcpy(cur+0x5d,&addr_str2,4);
            break;
        case Instr_Swap:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x8a;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x91;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0xa9;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x7c;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x9b;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0xae;
            len = sizeof(bin_sr_Swap);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Swap, len);
            memcpy(cur+0x7d,&addr_puts1,4);
            memcpy(cur+0x9c,&addr_puts2,4);
            memcpy(cur+0xaf,&addr_abort,4);
            memcpy(cur+0x8b,&addr_exit1,4);
            memcpy(cur+0x92,&addr_exit2,4);
            memcpy(cur+0xaa,&addr_exit3,4);
            memcpy(cur+0x78,&addr_str2,4);
            memcpy(cur+0x97,&addr_str3,4);
            break;
        case Instr_Dup:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x79;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x80;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x98;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x6b;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x8a;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x9d;
            len = sizeof(bin_sr_Dup);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Dup, len);
            memcpy(cur+0x7a,&addr_exit1, 4);
            memcpy(cur+0x81,&addr_exit2,4);
            memcpy(cur+0x99,&addr_exit3,4);
            memcpy(cur+0x6c,&addr_puts1,4);
            memcpy(cur+0x8b,&addr_puts2,4);
            memcpy(cur+0x9e,&addr_abort,4);
            memcpy(cur+0x86,&addr_str2,4);
            memcpy(cur+0x67,&addr_str3,4);
            break;
        case Instr_Inc:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x5f;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x77;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x8f;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x69;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x81;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x94;
            len = sizeof(bin_sr_Inc);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Inc, len);
            memcpy(cur+0x60,&addr_exit1, 4);
            memcpy(cur+0x78,&addr_exit2,4);
            memcpy(cur+0x90,&addr_exit3,4);
            memcpy(cur+0x6a,&addr_puts1,4);
            memcpy(cur+0x82,&addr_puts2,4);
            memcpy(cur+0x95,&addr_abort,4);
            memcpy(cur+0x7d,&addr_str2,4);
            memcpy(cur+0x65,&addr_str3,4);
            break;
        case Instr_Add:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x7c;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x83;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0xa0;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x6e;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x92;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x88;
            len = sizeof(bin_sr_Add);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Add, len);
            memcpy(cur+0x7d,&addr_exit1, 4);
            memcpy(cur+0x84,&addr_exit2,4);
            memcpy(cur+0xa1,&addr_exit3,4);
            memcpy(cur+0x6f,&addr_puts1,4);
            memcpy(cur+0x93,&addr_puts2,4);
            memcpy(cur+0x89,&addr_abort,4);
            memcpy(cur+0x6a,&addr_str2,4);
            memcpy(cur+0x8e,&addr_str3,4);
            break;
        case Instr_Sub:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x7c;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x83;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0xa0;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x6e;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x92;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x88;
            len = sizeof(bin_sr_Sub);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Sub, len);
            memcpy(cur+0x7d,&addr_exit1, 4);
            memcpy(cur+0x84,&addr_exit2,4);
            memcpy(cur+0xa1,&addr_exit3,4);
            memcpy(cur+0x6f,&addr_puts1,4);
            memcpy(cur+0x93,&addr_puts2,4);
            memcpy(cur+0x89,&addr_abort,4);
            memcpy(cur+0x6a,&addr_str2,4);
            memcpy(cur+0x8e,&addr_str3,4);
            break;
        case Instr_Mul:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x7d;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x84;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0xa1;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x6f;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x93;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x89;
            len = sizeof(bin_sr_Mul);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Mul, len);
            memcpy(cur+0x7e,&addr_exit1, 4);
            memcpy(cur+0x85,&addr_exit2,4);
            memcpy(cur+0xa2,&addr_exit3,4);
            memcpy(cur+0x70,&addr_puts1,4);
            memcpy(cur+0x94,&addr_puts2,4);
            memcpy(cur+0x8a,&addr_abort,4);
            memcpy(cur+0x6b,&addr_str2,4);
            memcpy(cur+0x8f,&addr_str3,4);
            break;
        case Instr_Rand:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x50;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x68;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x5a;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x6d;
            addr_rand = (intptr_t)&rand - (intptr_t)cur - call_template_size;
            len = sizeof(bin_sr_Rand);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Rand, len);
            memcpy(cur+0x51,&addr_exit1, 4);
            memcpy(cur+0x69,&addr_exit2,4);
            memcpy(cur+0x5b,&addr_puts1,4);
            memcpy(cur+0x6e,&addr_abort,4);
            memcpy(cur+0x56,&addr_str3,4);
            memcpy(cur+0x01,&addr_rand,4);
            break;
        case Instr_Dec:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x5f;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x77;
            addr_exit3 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x8f;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x69;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x81;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x94;
            len = sizeof(bin_sr_Dec);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Dec, len);
            memcpy(cur+0x60,&addr_exit1, 4);
            memcpy(cur+0x78,&addr_exit2,4);
            memcpy(cur+0x90,&addr_exit3,4);
            memcpy(cur+0x6a,&addr_puts1,4);
            memcpy(cur+0x82,&addr_puts2,4);
            memcpy(cur+0x95,&addr_abort,4);
            memcpy(cur+0x7d,&addr_str2,4);
            memcpy(cur+0x65,&addr_str3,4);
            break;
        case Instr_Drop:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x42;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x5a;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x4c;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x5f;
            len = sizeof(bin_sr_Drop);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Drop, len);
            memcpy(cur+0x43,&addr_exit1, 4);
            memcpy(cur+0x5b,&addr_exit2,4);
            memcpy(cur+0x4d,&addr_puts1,4);
            memcpy(cur+0x60,&addr_abort,4);
            memcpy(cur+0x48,&addr_str2,4);
            break;
        case Instr_Over:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x91;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x98;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x83;
            addr_push1 = (intptr_t)&push - (intptr_t)cur - call_template_size - (intptr_t)0x3b;
            addr_push2 = (intptr_t)&push - (intptr_t)cur - call_template_size - (intptr_t)0x45;
            addr_push3 = (intptr_t)&push - (intptr_t)cur - call_template_size - (intptr_t)0x50;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x9d;
            len = sizeof(bin_sr_Over);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Over, len);
            memcpy(cur+0x3c,&addr_push1, 4);
            memcpy(cur+0x46,&addr_push2,4);
            memcpy(cur+0x51,&addr_push3,4);
            memcpy(cur+0x84,&addr_puts1,4);
            memcpy(cur+0x9e,&addr_abort,4);
            memcpy(cur+0x92,&addr_exit1,4);
            memcpy(cur+0x99,&addr_exit2,4);
            memcpy(cur+0x7f,&addr_str2,4);
            break;
        case Instr_Mod:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x87;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x9f;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x79;
            addr_puts2 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x91;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0xa4;
            len = sizeof(bin_sr_Mod);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Mod, len);
            memcpy(cur+0x88,&addr_exit1, 4);
            memcpy(cur+0xa0,&addr_exit2,4);
            memcpy(cur+0x7a,&addr_puts1,4);
            memcpy(cur+0x92,&addr_puts2,4);
            memcpy(cur+0xa5,&addr_abort,4);
            memcpy(cur+0x8d,&addr_str2,4);
            memcpy(cur+0x75,&addr_str3,4);
            break;
        case Instr_Push:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x4e;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x66;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x58;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x6b;
            imm = decoded.immediate;
            len = sizeof(bin_sr_Push);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Push, len);
            memcpy(cur+0x4f,&addr_exit1, 4);
            memcpy(cur+0x67,&addr_exit2,4);
            memcpy(cur+0x59,&addr_puts1,4);
            memcpy(cur+0x6c,&addr_abort,4);
            memcpy(cur+0x54,&addr_str3,4);
            memcpy(cur+0x23,&imm,4);
            break;
        case Instr_JNE:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x56;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x6e;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x60;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x73;
            imm = decoded.immediate + 2;
            len = sizeof(bin_sr_Jne);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Jne, len);
            memcpy(cur+0x57,&addr_exit1, 4);
            memcpy(cur+0x6f,&addr_exit2,4);
            memcpy(cur+0x61,&addr_puts1,4);
            memcpy(cur+0x74,&addr_abort,4);
            memcpy(cur+0x5c,&addr_str2,4);
            memcpy(cur+0x4b,&imm,4);
            break;
        case Instr_JE:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x56;
            addr_exit2 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x6e;
            addr_puts1 = (intptr_t)&puts - (intptr_t)cur - call_template_size - (intptr_t)0x60;
            addr_abort = (intptr_t)&abort - (intptr_t)cur - call_template_size - (intptr_t)0x73;
            imm = decoded.immediate + 2;
            len = sizeof(bin_sr_Je);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Je, len);
            memcpy(cur+0x57,&addr_exit1, 4);
            memcpy(cur+0x6f,&addr_exit2,4);
            memcpy(cur+0x61,&addr_puts1,4);
            memcpy(cur+0x74,&addr_abort,4);
            memcpy(cur+0x5c,&addr_str2,4);
            memcpy(cur+0x4b,&imm,4);
            break;
        case Instr_Jump:
            addr_exit1 = (intptr_t)&exit_generated_code - (intptr_t)cur - call_template_size
                                - (intptr_t)0x0e;
            len = sizeof(bin_sr_Jump);
            assert(cur + len - out_code < JIT_CODE_SIZE);
            memcpy(cur, bin_sr_Jump, len);
            memcpy(cur+0x0f,&addr_exit1, 4);
            imm = decoded.immediate + 2;
            memcpy(cur+0x03,&imm, 4);
            break;
        case Instr_Break:
        default:
            break;
        }

        i += decoded.length;
        cur += len;
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

    inline_translate_program(cpu.pmem, gen_code, entrypoints, PROGRAM_SIZE);

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