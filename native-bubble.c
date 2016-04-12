/*  native-bubble.c - a native C-code implementation for algorithm used
 *  for the Bubble program executed inside stack virtual machine.
 *  Copyright (c) 2015, 2016 Alexandra Tsvetkova. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of interpreters-comparison nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"

#define SWAP(A, B) { int t = A; A = B; B = t; }

extern const Instr_t Bubble[];

int main()
{
    /* This program cannot be used for performance comparisons if the target
     * program chosen for simulation is different from Bubble.
     * Warn and refuse to work instead of giving user bogus numbers
     */
    if (Program != Bubble) {
        fprintf(stderr,
            "This executable can only execute Newton, but another target"
            " program was chosen. The results would be incomparable."
            " Please fix your code\n");
        return 1;
    }
    int n = 24;
    //printf("Enter array size ");
    //scanf("%d", &n);

    int* a = (int*)malloc(n*sizeof(int));
    int i, j;


    for (i = n - 1; i >= 0; i--)
        a[i]=rand();
    for (i = n - 1; i >= 0; i--)
    {
        for (j = 0; j < i; j++)
        {
            if (a[j] < a[j + 1])
                SWAP( a[j], a[j + 1] );
        }
    }
    for (i = n - 1; i >= 0; i--)
        printf("\na[%d] = %d", i, a[i]);
    printf("\n");
    return 0;
}
