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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

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

/* Choose a default program we are about to simulate */
const Instr_t* DefProgram = Primes;

/* Pointer to a loaded program */
Instr_t* LoadedProgram = NULL;

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

cpu_t init_cpu () {
    cpu_t cpu = {.pc = 0, .sp = -1, .state = Cpu_Running,
                 .steps = 0, .stack = {0},
                 .pmem = LoadedProgram ? LoadedProgram : DefProgram};
    return cpu;
}

static const char *steplimit_opt = "--steplimit=";
static const char *inp_prog_opt = "--inp-prog=";

static inline
void report_usage_and_exit(char * exec_name, int ret_code) {
    fprintf(stderr, "Usage: %s %s<num> %s<str>\n", exec_name, steplimit_opt, inp_prog_opt);
    exit (ret_code);
}

long long parse_args(int argc, char** argv) {
    long long steplimit = LLONG_MAX;
    FILE *prog_file = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help"))
            report_usage_and_exit(argv[0], 2);
        else if (!strncmp(argv[i], steplimit_opt, strlen(steplimit_opt))) {
            char *endptr = NULL;
            steplimit = strtoll(argv[i] + strlen(steplimit_opt), &endptr, 10);
            if (errno || (*endptr != '\0')) {
                fprintf(stderr, "Unrecognized steplimit: %s\a", argv[i]);
                report_usage_and_exit(argv[0], 2);
            }
        }
        else if (!strncmp(argv[i], inp_prog_opt, strlen(inp_prog_opt))) {
            prog_file = fopen(argv[i] + strlen(inp_prog_opt), "rb");
            if (errno || prog_file == NULL) {
                fprintf(stderr, "Unrecognized program file: %s\n", argv[i]);
                report_usage_and_exit(argv[0], 2);
            }
        }
        else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            report_usage_and_exit(argv[0], 2);
        }
    }

    unsigned long long filelen = 0;
    if (prog_file != NULL) {
        fseek(prog_file, 0, SEEK_END);  // Jump to the end of the file
        filelen = ftell(prog_file);     // Get the current byte offset in the file
        rewind(prog_file);              // Jump back to the beginning of the file
        if (filelen > PROGRAM_SIZE * sizeof(Instr_t)) {
            fprintf(stderr, "Input program size exceeds allocated memory.\n");
            exit(2);
        }
        LoadedProgram = (Instr_t*) calloc(filelen, sizeof(Instr_t)); // Enough memory for file
        if (LoadedProgram == NULL) {
            fprintf(stderr, "Can't allocate memory for input program.\n");
            exit(2);
        }
        fread(LoadedProgram, filelen, 1, prog_file); // Read in the entire file
        fclose(prog_file);                           // Close the file
    }

    return steplimit;
}

void write_program (Instr_t* program, size_t program_size, const char* out_file) {
    FILE *prog_file = fopen(out_file, "wb");
    if (errno || prog_file == NULL) {
        fprintf(stderr, "Can't open output file.\n");
        report_usage_and_exit("", 2);
    }
    fwrite(program, sizeof(Instr_t), program_size, prog_file);
    fclose(prog_file);
}
