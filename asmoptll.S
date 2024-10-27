
# TODO: кеширование верхних элементов стека
# TODO: самомодифицирующийся код при возвратах и проверках стека

    # VRFY: возможно нативный код не имеет инкремента steps
    # из ADVANCE_PC, тогда он не вполне корректен с точки зрения
    # профилированся скорости

# По-возможности, старайтесь уклониться от исполнения кода:
# невыполнение кода занимает меньше всего времени
.set DBGCNT, 0
.set STEPLIMIT_CHECK, 0
.set MAX_PROGRAM_SIZE_CHECK, 0
.set STATE_RUNNING_CHECK, 0
.set STACK_CHECK, 0

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
#define state           %rdx
#define steplimit       %rcx
#define steps           %r8
#define pc              %r9
#define stack_max       %rbp
#define stack_min       %rbx
#define sp              %rsp

# 0 = Cpu_Running
#define zero            %r10
# 1 = Cpu_Halted
#define one             %r11
# 2 = Cpu_Break
#define two             %r12

# DECODE_T
#define opcode          %r13d
#define opcode_full     %r13
#define immediate       %r14d
#define immediate_full  %r14

.section .text
.macro FETCH_DECODE
FETCH_DECODE_\@:
    FETCH_CHECKED
    DECODE
.endm


    # (PC > 512) then error, exit
    # RET: opcode
.macro FETCH_CHECKED
FETCH_CHECKED_\@:
    .if MAX_PROGRAM_SIZE_CHECK
    movq    $512, opcode_full   # (opcode_full := max_program_size)
    cmp     pc, opcode_full
    jb      handle_pc_out_of_bound  # (pc > max_program_size)
    .endif
    FETCH
.endm

handle_pc_out_of_bound:
    movq    two, state          # = 2 (Cpu_Break)
    movq    zero, opcode_full   # = 0 (Instr_Break)
    lea     sz_pc_out_of_bound(%rip), %rax
    jmp     save_rets_and_exit

    .section .data
sz_pc_out_of_bound:
    .asciz "PC out of bounds"
    .section .text



    # RET: opcode
.macro FETCH
FETCH_\@:
    mov     (prog_mem, pc, 4), opcode
.endm


    # IN: opcode (от FETCH, который всегда предшествует)
    # RET: immediate
.macro DECODE
DECODE_\@:
    mov     4(prog_mem, pc, 4), immediate   # prog_mem[pc+1]
.endm



.macro BAIL_ON_ERROR
    .if STATE_RUNNING_CHECK
    test    state, state
    jne     handle_state_is_not_running
    .endif
.endm

.if STATE_RUNNING_CHECK
handle_state_is_not_running:
    lea     sz_state_is_not_running(%rip), %rax
    mov     two, state # Cpu_Break
    jmp     save_rets_and_exit
.endif


.macro DISPATCH
DISPATCH_\@:
    jmp     *(routines, opcode_full, 8)
.endm


.macro ADVANCE_PC cnt:req
ADVANCE_PC_\@:
    .rept \cnt
    inc    pc
    .endr

    .if STEPLIMIT_CHECK
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
    lea     sz_state_is_not_running(%rip), %rax
    jmp     set_state_break
.endif

.if STEPLIMIT_CHECK
handle_steplimit_reached:
    lea     sz_steplimit_reached(%rip), %rax
    jmp     set_state_break
.endif

.if (STATE_RUNNING_CHECK || STEPLIMIT_CHECK)
set_state_break:
    mov     two, state # Cpu_Break
    jmp     save_rets_and_exit
.endif

    .section .data
sz_state_is_not_running:
    .asciz "state is not running"
sz_steplimit_reached:
    .asciz "steplimit reached"
    .section .text



.macro PUSH_IMM reg
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
    lea     sz_stack_overflow(%rip), %rax
    mov     two, state # Cpu_Break
    jmp     save_rets_and_exit

    .section .data
sz_stack_overflow:
    .asciz "stack overflow"
    .section .text
.endif

.macro VM_PUSH tmpreg args:vararg
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

    // Итерация и push каждого аргумента
    .irp reg, \args
     push   \reg
    .endr
.endm



.macro POP_IMM reg
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
    lea     sz_stack_underflow(%rip), %rax
    mov     two, state # Cpu_Break
    jmp     save_rets_and_exit

    .section .data
sz_stack_underflow:
    .asciz "stack underflow"
    .section .text
.endif

.macro VM_POP tmpreg:req args:vararg
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

    # обработаем varargs
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


    RTN Break
    /* No need to dispatch after Break */
    mov     two, state
    xor     %eax, %eax
    jmp     save_rets_and_exit


    RTN Halt
    /* No need to dispatch after Halt */
    mov     one, state
    xor     %eax, %eax
    jmp     save_rets_and_exit


    RTN Nop
    /* Do nothing */
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Push
    PUSH_IMM immediate_full
    ADVANCE_PC 2
    FETCH_DECODE
    DISPATCH


    RTN Drop
    POP_IMM immediate_full
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Dup
    BAIL_ON_ERROR
    POP_IMM immediate_full
    VM_PUSH opcode_full immediate_full immediate_full
    ADVANCE_PC 1
lab:
    FETCH_DECODE
    DISPATCH


    RTN Swap
    VM_POP opcode_full immediate_full %rax
    BAIL_ON_ERROR
    VM_PUSH opcode_full immediate_full %rax
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Over
    # мы можем использовать opcode_full как tmpreg, так как
    # после FETCH он не используется и будет перезаписан
    # в следующем FETCH
    VM_POP opcode_full immediate_full %rax
    BAIL_ON_ERROR
    VM_PUSH opcode_full %rax immediate_full %rax
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Sub
    VM_POP opcode_full immediate_full %rax
    sub     %rax, immediate_full
    PUSH_IMM immediate_full
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Inc
    POP_IMM immediate_full
    BAIL_ON_ERROR
    inc     immediate_full
    # Тут можно оптимизировать проверки, уже выполенные в POP_IMM
    # а еще лучше - изменять прямо на месте, в памяти
    PUSH_IMM immediate_full
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Add
    POP_IMM immediate_full
    POP_IMM %rax
    BAIL_ON_ERROR
    add     %rax, immediate_full
    PUSH_IMM immediate_full
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH


    RTN Mod
    VM_POP opcode_full %rax immediate_full
    BAIL_ON_ERROR
    test    immediate_full, immediate_full
    jne     1f
    mov     two, state
    mov     sz_divide_zero(%rip), %eax
    jmp     save_rets_and_exit
1:
    # TODO: Здесь можно оптимизировать размещение регистров
    push    %rdx
    xor     %rdx, %rdx
    div     immediate_full  # rdx:rax -> rax, rdx
    xchg    %rdx, immediate_full
    pop     %rdx
    PUSH_IMM immediate_full
    ADVANCE_PC 1
    FETCH_DECODE
    DISPATCH

    .section .data
sz_divide_zero:
    .asciz "divide by zero"
    .section .text


    RTN Jump
    movsx   immediate, immediate_full
    add     immediate_full, pc
    ADVANCE_PC 2
    FETCH_DECODE
    DISPATCH


    RTN Je
    POP_IMM %rax
    BAIL_ON_ERROR
    test    %rax, %rax
    jne     1f
    add     immediate_full, pc
1:
    ADVANCE_PC 2
    FETCH_DECODE
    DISPATCH


    RTN Print
    # Требуется сохранить
    # steps
    # state
    # steplimit
    # two

    # Можно не сохранять (будут перезаписаны)
    # rax
    # opcode_full r13
    # opcode
    POP_IMM %rax
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
    mov     %rax, %rsi
    xor     %rax, %rax
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
    .section .text




#### MAIN ####


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

    xor     %r15, %r15
    xor     steps, steps
    xor     pc, pc
    xor     opcode_full, opcode_full
    xor     immediate_full, immediate_full
    xor     zero, zero
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
    test    %rax, %rax
    jz      1f
    mov     %rax, ret_err_ptr(%rip)
1:
    .if STEPLIMIT_CHECK
    movq    steps, ret_steps(%rip)
    .endif
    movq    state, ret_state(%rip)
    movq    pc, ret_pc(%rip)
    # Save stack pos
    movq    stack_max, ret_sp(%rip)
    sub     sp, ret_sp(%rip)
    shrq    $3, ret_sp(%rip)


    # Копируем стек
    lea     ret_stack(%rip), %rax
    mov     ret_sp(%rip), %rcx
copy_loop:
    popq immediate_full
    movl immediate, (%rax)
    add $8, %rax
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
    gvars cnt_VM_Pop cnt_VM_Push cnt_LPop cnt_LPush cnt_Print cnt_Je cnt_Mod cnt_Add cnt_Sub cnt_Over cnt_Swap cnt_Dup cnt_Drop cnt_Push cnt_Nop cnt_Halt cnt_Break cnt_Inc cnt_Jump

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
