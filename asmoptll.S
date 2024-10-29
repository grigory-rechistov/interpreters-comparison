
# TODO: самомодифицирующийся код при возвратах и проверках стека

    # VRFY: возможно нативный код не имеет инкремента steps
    # из ADVANCE_PC, тогда он не вполне корректен с точки зрения
    # профилированся скорости

# По-возможности, старайтесь уклониться от исполнения кода:
# невыполнение кода занимает меньше всего времени
.set DBGCNT, 0
.set STEPCNT, 0
.set STEPLIMIT_CHECK, 0
.set MAX_PROGRAM_SIZE_CHECK, 0
.set STATE_RUNNING_CHECK, 0
.set STACK_CHECK, 1

# Оптимизации
.set OPT_CACHED, 0
.set OPT_ON_SITE, 0 # 3.590s
.set OPT_ON_SITE, 1 # 3.298s

.set OPT_CACHED, 1  # 3.004s
.set OPT_CACHED, 2  # 2.847s

# Инструкции
#define Instr_Break   0x0000 /* Abnormal end */
#define Instr_Nop     0x0001
#define Instr_Halt    0x0002 /* Normal program end */
#define Instr_Push    0x0003 /* imm */
#define Instr_Print   0x0004
#define Instr_JNE     0x0005 /* imm */
#define Instr_Swap    0x0006
#define Instr_Dup     0x0007
#define Instr_JE      0x0008 /* imm */
#define Instr_Inc     0x0009
#define Instr_Add     0x000a
#define Instr_Sub     0x000b
#define Instr_Mul     0x000c
#define Instr_Rand    0x000d
#define Instr_Dec     0x000e
#define Instr_Drop    0x000f
#define Instr_Over    0x0010
#define Instr_Mod     0x0011
#define Instr_Jump    0x0012 /* imm */
#define Instr_And     0x0013
#define Instr_Or      0x0014
#define Instr_Xor     0x0015
#define Instr_SHL     0x0016
#define Instr_SHR     0x0017
#define Instr_SQRT    0x0018
#define Instr_Rot     0x0019
#define Instr_Pick    0x001a

# CPU_T
#define routines        %rdi
#define prog_mem        %rsi
#define steplimit       %rcx
#define steps           %r8
#define pc              %r9
#define stack_max       %rbp
#define stack_min       %rbx
#define sp              %rsp

#define top             %rax
# 0 = Cpu_Running
#define subtop          %r10
# 1 = Cpu_Halted
#define one             %r11
# 2 = Cpu_Break
#define two             %r12

# DECODE_T
#define opcode32        %edx
#define opcode64        %rdx
#define immed32         %r14d
#define immed64         %r14
#define state           %r15

# ACCUMULATOR
# define acc            %r13


.section .text
.macro FETCH_DECODE
    FETCH_CHECKED
    DECODE
.endm

.macro FETCH_CHECKED
    .if MAX_PROGRAM_SIZE_CHECK
    movq    $512, opcode64   # (opcode64 := max_program_size)
    cmp     pc, opcode64
    jb      handle_pc_out_of_bound  # (pc > max_program_size)
    .endif
    FETCH
.endm

handle_pc_out_of_bound:
    movq    two, state           # = 2 (Cpu_Break)
    xor     opcode64, opcode64   # = 0 (Instr_Break)
    lea     sz_pc_out_of_bound(%rip), acc
    jmp     save_rets_and_exit

    .section .data
sz_pc_out_of_bound:
    .asciz "PC out of bounds"
    .section .text

.macro FETCH
    mov     (prog_mem, pc, 4), opcode32     # prog_mem[pc]
.endm

.macro DECODE
    mov     4(prog_mem, pc, 4), immed32     # prog_mem[pc+1]
.endm



.macro BAIL_ON_ERROR
    .if STATE_RUNNING_CHECK
    test    state, state
    jne     handle_state_is_not_running
    .endif
.endm

.if STATE_RUNNING_CHECK
handle_state_is_not_running:
    mov     two, state # Cpu_Break
    lea     sz_state_is_not_running(%rip), acc
    jmp     save_rets_and_exit
.endif


.macro DISPATCH
# DISPATCH_\@:
    jmp     *(routines, opcode64, 8)
.endm


.macro ADVANCE_PC cnt:req
# ADVANCE_PC_\@:
    .rept \cnt
    inc    pc
    .endr

    .if (STEPLIMIT_CHECK || STEPCNT)
    # Аксакалы верят что если разнести инкремент и проверку, то
    # это позволит процессору заняться в промежутке чем-то еще
    inc     steps
    .endif

    .if STATE_RUNNING_CHECK
    test    state, state        # Cpu_Running(0) != state
    jne     handle_state_not_running
    .endif

    .if STEPLIMIT_CHECK
    cmp     steps, steplimit    # steps >= steplimit
    jl      handle_steplimit_reached
    .endif
.endm

.if STATE_RUNNING_CHECK
handle_state_not_running:
    lea     sz_state_is_not_running(%rip), acc
    jmp     set_state_break
.endif

.if STEPLIMIT_CHECK
handle_steplimit_reached:
    lea     sz_steplimit_reached(%rip), acc
    jmp     set_state_break
.endif

.if (STATE_RUNNING_CHECK || STEPLIMIT_CHECK)
set_state_break:
    mov     two, state # Cpu_Break
    lea     sz_system_break(%rip), acc
    jmp     save_rets_and_exit
.endif

    .section .data
sz_state_is_not_running:
    .asciz "state is not running"
sz_steplimit_reached:
    .asciz "steplimit reached"
    .section .text



.macro PUSH_IMM reg
# PUSH_IMM_\@:
    .if DBGCNT
    incq    cnt_LPush(%rip)
    .endif

    .if STACK_CHECK
    cmp     sp, stack_min
    jae     handle_overflow
    .endif

    push    \reg
.endm

.if STACK_CHECK
handle_overflow:
    mov     two, state # Cpu_Break
    lea     sz_stack_overflow(%rip), acc
    jmp     save_rets_and_exit

    .section .data
sz_stack_overflow:
    .asciz "stack overflow"
    .section .text
.endif

.macro VM_PUSH tmpreg args:vararg
# VM_PUSH_\@:
    .if DBGCNT
    incq    cnt_VM_Push(%rip)
    .endif

    # подсчитаем количество макро-аргументов
    .set num_args, 0
    .irp arg, \args
     .set num_args, num_args + 1
    .endr

    .if STACK_CHECK
    # смещение для LEA
    .set offset, -8 * num_args
    lea     offset(sp), \tmpreg
    # проверим не выходим ли за минимум
    cmp     \tmpreg, stack_min
    jae     handle_overflow
    .endif

    # push каждого аргумента
    .irp reg, \args
     push   \reg
    .endr
.endm



.macro POP_IMM reg
# POP_IMM_\@:
    .if DBGCNT
    incq    cnt_LPop(%rip)
    .endif

    .if STACK_CHECK
    cmp     sp, stack_max
    jb      handle_underflow
    .endif

    pop     \reg
.endm

.if STACK_CHECK
handle_underflow:
    mov     two, state # Cpu_Break
    lea     sz_stack_underflow(%rip), acc
    jmp     save_rets_and_exit

    .section .data
sz_stack_underflow:
    .asciz "stack underflow"
    .section .text
.endif

.macro VM_POP tmpreg:req args:vararg
# VM_POP_\@:
    .if DBGCNT
    incq    cnt_VM_Pop(%rip)
    .endif

    # подсчитаем количество макро-аргументов
    .set num_args, 0
    .irp arg, \args
        .set num_args, num_args + 1
    .endr

    .if STACK_CHECK
    # вычислим смещение для LEA
    .set offset, 8*num_args
    lea offset(sp), \tmpreg
    # проверим не выходим ли за максимум
    cmp     \tmpreg, stack_max
    jb      handle_underflow
    .endif

    # pop каждого аргумента
    .irp reg, \args
    pop     \reg
    .endr
.endm


#### ROUTINES ####


.macro RTN name
    .global srv_\name
    .type srv_\name, @function
srv_\name:
    .if DBGCNT
    incq    cnt_\name(%rip)
    .endif
.endm


    RTN Break   ## <- NB! Not used
    # No need to dispatch after Break
    mov     two, state
    lea     sz_system_break(%rip), acc
    jmp     save_rets_and_exit


    RTN Halt
    # No need to dispatch after Halt
    mov     one, state
    lea     sz_system_halted(%rip), acc
    jmp     save_rets_and_exit


    RTN Nop     ## <- NB! Not used
    # Do nothing
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Push
    .if OPT_CACHED == 2
      PUSH_IMM  subtop
      movq      top, subtop
      movq      immed64, top
    .endif
    .if OPT_CACHED == 1
      PUSH_IMM  top
      movq      immed64, top
    .endif
    .if OPT_CACHED == 0
      PUSH_IMM  immed64
    .endif
    ADVANCE_PC 2
    FETCH_DECODE
    DISPATCH


    RTN Drop
    .if OPT_CACHED == 2
      movq      subtop, top
      POP_IMM   subtop
    .endif
    .if OPT_CACHED == 1
      POP_IMM   top
    .endif
    .if OPT_CACHED == 0
      POP_IMM   immed64
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Dup     ## <- NB! Not used
    .if OPT_CACHED == 2
      PUSH_IMM  subtop
      movq      top, subtop
    .endif
    .if OPT_CACHED == 1
      PUSH_IMM   top
      BAIL_ON_ERROR
    .endif
    .if OPT_CACHED == 0
      .if OPT_ON_SITE == 1
        # Возможно этот способ медленее, чем вариант
        # из else, но т.к. эта RTN не используется,
        # то это никак не проверить - пусть будет так
        mov (sp), immed64
        PUSH_IMM immed64
      .else
        POP_IMM immed64
        BAIL_ON_ERROR
        VM_PUSH opcode64 immed64 immed64
      .endif
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Swap
    .if OPT_CACHED == 2
      xchg   top, subtop
    .endif
    .if OPT_CACHED == 1
      #xchg   top, (sp)
      POP_IMM   opcode64
      xchg  opcode64, top
      push  opcode64
    .endif
    .if OPT_CACHED == 0
      .if OPT_ON_SITE == 1
        # Этот способ серьезно замедляет работу, поэтому
        # он закомментирован, и вместо него сюда
        # скопирован и используется вариант из else
        # mov     (sp), acc
        # xchg    8(sp), acc
        # xchg    (sp), acc
        VM_POP opcode64 immed64 acc
        BAIL_ON_ERROR
        VM_PUSH opcode64 immed64 acc
      .else
        VM_POP opcode64 immed64 acc
        BAIL_ON_ERROR
        VM_PUSH opcode64 immed64 acc
      .endif
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Over
    .if OPT_CACHED == 2
      xchg  top, subtop
      PUSH_IMM  top
    .endif
    .if OPT_CACHED == 1
      PUSH_IMM  top
      movq  8(sp), top
    .endif
    .if OPT_CACHED == 0
      .if OPT_ON_SITE == 1
         movq       8(sp), acc
         PUSH_IMM   acc
      .else
        # мы можем использовать opcode64 как tmpreg, так как
        # после FETCH он не используется и будет перезаписан
        # в следующем FETCH
        VM_POP opcode64 immed64 acc
        BAIL_ON_ERROR
        VM_PUSH opcode64 acc immed64 acc
      .endif
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Sub
    .if OPT_CACHED == 2
      subq      subtop, top
      POP_IMM   subtop
    .endif
    .if OPT_CACHED == 1
      POP_IMM    opcode64
      subq       opcode64, top
    .endif
    .if OPT_CACHED == 0
      .if OPT_ON_SITE == 1
        POP_IMM  immed64
        subq     immed64, (sp)
      .else
        VM_POP   opcode64 immed64 acc
        sub      acc, immed64
        PUSH_IMM immed64
      .endif
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Inc
    .if OPT_CACHED == 2
      inc   top
    .endif
    .if OPT_CACHED == 1
      inc   top
    .endif
    .if OPT_CACHED == 0
      .if OPT_ON_SITE == 1
        incq    (sp)
      .else
        POP_IMM immed64
        BAIL_ON_ERROR
        inc     immed64
        # Тут можно оптимизировать проверки, уже выполненные в POP_IMM
        # а еще лучше - изменять прямо на месте, в памяти
        PUSH_IMM immed64
      .endif
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Mod
    .if OPT_CACHED == 2
      # Так как мы для top выбрали RAX то не требуется
      # делать mov top, %rax для подготовки к делению
      test    subtop, subtop
      je      handle_divide_zero
      xor     %rdx, %rdx        # rdx = opcode64
      div     subtop            # rdx:rax / operand -> rax, rdx
      movq    %rdx, top
      POP_IMM subtop
    .endif

    .if OPT_CACHED == 1
      # Так как мы для top выбрали RAX то не требуется
      # делать mov top, %rax для подготовки к делению
      POP_IMM immed64
      BAIL_ON_ERROR
      test    immed64, immed64
      je      handle_divide_zero
      xor     %rdx, %rdx          # rdx = opcode64
      div     immed64      # rdx:rax / operand -> rax, rdx
      movq    %rdx, top
    .endif

    .if OPT_CACHED == 0
      VM_POP opcode64 %rax immed64
      BAIL_ON_ERROR
      test    immed64, immed64
      je      handle_divide_zero
      xor     %rdx, %rdx          # rdx = opcode64
      div     immed64      # rdx:rax / operand  -> rax, rdx
      PUSH_IMM %rdx
    .endif
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH

handle_divide_zero:
    mov     two, state
    lea     sz_divide_zero(%rip), acc
    jmp     save_rets_and_exit
end_handle_divide_zero:

    .section .data
sz_divide_zero:
    .asciz "divide by zero"
    .section .text


    RTN Jump
    movsx   immed32, immed64
    add     immed64, pc
    ADVANCE_PC 2
    FETCH_DECODE
    DISPATCH


    RTN Je

    .if OPT_CACHED == 2
      movq    top, acc
      movq    subtop, top
      POP_IMM subtop
      BAIL_ON_ERROR
      test    acc, acc
      je      3f
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
3:
      add     immed64, pc
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
    .endif

    .if OPT_CACHED == 1
      movq    top, acc
      POP_IMM top
      BAIL_ON_ERROR
      test    acc, acc
      je      2f
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
2:
      add     immed64, pc
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
    .endif

    .if OPT_CACHED == 0
      POP_IMM acc
      BAIL_ON_ERROR
      test    acc, acc
      je      1f
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
1:
      add     immed64, pc
      ADVANCE_PC 2
      FETCH_DECODE
      DISPATCH
    .endif


    RTN Print
    .if OPT_CACHED == 2
      movq  top, acc
      movq    subtop, top
      POP_IMM subtop
    .endif
    .if OPT_CACHED == 1
      movq  top, acc
      POP_IMM top
    .endif
    .if OPT_CACHED == 0
      POP_IMM acc
    .endif
    BAIL_ON_ERROR
    push    %rdi
    push    %rsi
    push    %rcx
    push    %rdx
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    lea     sz_fmt_str(%rip), %rdi
    movq    acc, %rsi
    xor     acc, acc
test:
    call    printf
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rdx
    pop     %rcx
    pop     %rsi
    pop     %rdi

    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    .section .data
sz_fmt_str:
    .asciz "[%d]\n"
    .quad 0



#### MAIN ####


    .section .text

    .global asm_main
    .type asm_main, @function
    # %rdi routines
    # %rsi prog_mem
    # %rdx state
    # %rcx steplimit
    # %r8  -
    # %r9  -
asm_main:
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, old_rsp(%rip)

    mov     %rdx, state

    xor     %r15, %r15
    xor     steps, steps
    xor     pc, pc
    xor     opcode64, opcode64
    xor     immed64, immed64
    xor     top, top
    xor     subtop, subtop
    xor     one, one
    inc     one
    mov     one, two
    inc     two
    mov     sp, stack_max
    mov     sp, stack_min
    sub     $0x100, stack_min  # STACK_CAPACITY = 32

    FETCH_DECODE
    DISPATCH

save_rets_and_exit: # <----
    test    acc, acc
    jz      1f
    mov     acc, ret_err_ptr(%rip)
1:
    .if (STEPLIMIT_CHECK || STEPCNT)
    movq    steps, ret_steps(%rip)
    .endif
    movq    state, ret_state(%rip)
    movq    pc, ret_pc(%rip)
    # Save stack pos
    movq    stack_max, ret_sp(%rip)
    sub     sp, ret_sp(%rip)
    shrq    $3, ret_sp(%rip)


    # Копируем стек
    lea     ret_stack(%rip), acc
    mov     ret_sp(%rip), %rcx
copy_loop:
    popq immed64
    movl immed32, (acc)
    add $8, acc
    loop copy_loop

    # Теперь можно восстановить RSP
    movq    old_rsp(%rip), %rsp
    # Востанавливаем все остальное
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret


#### DATA ####

    .section .data

.macro var name
\name:
    .quad 0
.endm

.macro vars names:vararg
    .irp name, \names
        var \name
    .endr
.endm

.macro gvar name
    .global \name
\name:
    .quad 0
.endm

.macro gvars names:vararg
    .irp name, \names
        gvar \name
    .endr
.endm

    vars old_rsp

    gvars ret_steps ret_state ret_pc ret_sp
    gvars cnt_VM_Pop cnt_VM_Push cnt_LPop cnt_LPush cnt_Print cnt_Je cnt_Mod cnt_Sub cnt_Over cnt_Swap cnt_Dup cnt_Drop cnt_Push cnt_Nop cnt_Halt cnt_Break cnt_Inc cnt_Jump

sz_system_break:
    .asciz "system break."

sz_system_halted:
    .asciz "system halted."

no_err_msg:
    .asciz "no errors."

    .global ret_err_ptr
ret_err_ptr:
    .quad no_err_msg

    .global ret_stack
ret_stack:
    .rept 32
    .word 0
    .endr
