;/*
; Copyright (C) 2014 Apple Inc. All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 1. Redistributions of source code must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
; EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
; PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
; CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
; EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
; OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;*/

EXTERN getHostCallReturnValueWithExecState : near

PUBLIC getHostCallReturnValue
PUBLIC ctiMasmProbeTrampoline

_TEXT   SEGMENT

getHostCallReturnValue PROC
    lea rcx, [rsp - 8]
    ; Allocate space for all 4 parameter registers, and align stack pointer to 16 bytes boundary by allocating another 8 bytes.
    ; The stack alignment is needed to fix a crash in the CRT library on a floating point instruction.
    sub rsp, 40
    call getHostCallReturnValueWithExecState
    add rsp, 40
    ret
getHostCallReturnValue ENDP

; The following constants must match the x86_64 version in MacroAssemblerX86Common.cpp.
PTR_SIZE EQU 8

PROBE_PROBE_FUNCTION_OFFSET EQU (0 * PTR_SIZE)
PROBE_ARG_OFFSET EQU (1 * PTR_SIZE)
PROBE_INIT_STACK_FUNCTION_OFFSET EQU (2 * PTR_SIZE)
PROBE_INIT_STACK_ARG_OFFSET EQU (3 * PTR_SIZE)

PROBE_FIRST_GPR_OFFSET EQU (4 * PTR_SIZE)
PROBE_CPU_EAX_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (0 * PTR_SIZE))
PROBE_CPU_ECX_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (1 * PTR_SIZE))
PROBE_CPU_EDX_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (2 * PTR_SIZE))
PROBE_CPU_EBX_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (3 * PTR_SIZE))
PROBE_CPU_ESP_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (4 * PTR_SIZE))
PROBE_CPU_EBP_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (5 * PTR_SIZE))
PROBE_CPU_ESI_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (6 * PTR_SIZE))
PROBE_CPU_EDI_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (7 * PTR_SIZE))

PROBE_CPU_R8_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (8 * PTR_SIZE))
PROBE_CPU_R9_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (9 * PTR_SIZE))
PROBE_CPU_R10_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (10 * PTR_SIZE))
PROBE_CPU_R11_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (11 * PTR_SIZE))
PROBE_CPU_R12_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (12 * PTR_SIZE))
PROBE_CPU_R13_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (13 * PTR_SIZE))
PROBE_CPU_R14_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (14 * PTR_SIZE))
PROBE_CPU_R15_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (15 * PTR_SIZE))
PROBE_FIRST_SPR_OFFSET EQU (PROBE_FIRST_GPR_OFFSET + (16 * PTR_SIZE))

PROBE_CPU_EIP_OFFSET EQU (PROBE_FIRST_SPR_OFFSET + (0 * PTR_SIZE))
PROBE_CPU_EFLAGS_OFFSET EQU (PROBE_FIRST_SPR_OFFSET + (1 * PTR_SIZE))
PROBE_FIRST_XMM_OFFSET EQU (PROBE_FIRST_SPR_OFFSET + (2 * PTR_SIZE))

XMM_SIZE EQU 8
PROBE_CPU_XMM0_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (0 * XMM_SIZE))
PROBE_CPU_XMM1_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (1 * XMM_SIZE))
PROBE_CPU_XMM2_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (2 * XMM_SIZE))
PROBE_CPU_XMM3_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (3 * XMM_SIZE))
PROBE_CPU_XMM4_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (4 * XMM_SIZE))
PROBE_CPU_XMM5_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (5 * XMM_SIZE))
PROBE_CPU_XMM6_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (6 * XMM_SIZE))
PROBE_CPU_XMM7_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (7 * XMM_SIZE))

PROBE_CPU_XMM8_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (8 * XMM_SIZE))
PROBE_CPU_XMM9_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (9 * XMM_SIZE))
PROBE_CPU_XMM10_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (10 * XMM_SIZE))
PROBE_CPU_XMM11_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (11 * XMM_SIZE))
PROBE_CPU_XMM12_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (12 * XMM_SIZE))
PROBE_CPU_XMM13_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (13 * XMM_SIZE))
PROBE_CPU_XMM14_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (14 * XMM_SIZE))
PROBE_CPU_XMM15_OFFSET EQU (PROBE_FIRST_XMM_OFFSET + (15 * XMM_SIZE))
PROBE_SIZE EQU (PROBE_CPU_XMM15_OFFSET + XMM_SIZE)

PROBE_EXECUTOR_OFFSET EQU PROBE_SIZE ; Stash the executeProbe function pointer at the end of the ProbeContext.

OUT_SIZE EQU (5 * PTR_SIZE)

ctiMasmProbeTrampoline PROC
    pushfq

    ; MacroAssemblerX86Common::probe() has already generated code to store some values.
    ; Together with the rflags pushed above, the top of stack now looks like this:
    ;     rsp[0 * ptrSize]: rflags
    ;     rsp[1 * ptrSize]: return address / saved rip
    ;     rsp[2 * ptrSize]: saved rbx
    ;     rsp[3 * ptrSize]: saved rdx
    ;     rsp[4 * ptrSize]: saved rcx
    ;     rsp[5 * ptrSize]: saved rax
    ;
    ; Incoming registers contain:
    ;     rcx: Probe::executeProbe
    ;     rdx: probe function
    ;     rbx: probe arg
    ;     rax: scratch (was ctiMasmProbeTrampoline)

    mov rax, rsp 
    sub rsp, PROBE_SIZE + OUT_SIZE

    ; The X86_64 ABI specifies that the worse case stack alignment requirement is 32 bytes.
    and rsp, not 01fh
    ; Since sp points to the ProbeContext, we've ensured that it's protected from interrupts before we initialize it.

    mov [PROBE_CPU_EBP_OFFSET + rsp], rbp
    mov rbp, rsp ; Save the ProbeContext*.

    mov [PROBE_EXECUTOR_OFFSET + rbp], rcx
    mov [PROBE_PROBE_FUNCTION_OFFSET + rbp], rdx
    mov [PROBE_ARG_OFFSET + rbp], rbx
    mov [PROBE_CPU_ESI_OFFSET + rbp], rsi
    mov [PROBE_CPU_EDI_OFFSET + rbp], rdi

    mov rcx, [0 * PTR_SIZE + rax]
    mov [PROBE_CPU_EFLAGS_OFFSET + rbp], rcx
    mov rcx, [1 *  PTR_SIZE + rax]
    mov [PROBE_CPU_EIP_OFFSET + rbp], rcx
    mov rcx, [2 * PTR_SIZE + rax]
    mov [PROBE_CPU_EBX_OFFSET + rbp], rcx
    mov rcx, [3 * PTR_SIZE + rax]
    mov [PROBE_CPU_EDX_OFFSET + rbp], rcx
    mov rcx, [4 * PTR_SIZE + rax]
    mov [PROBE_CPU_ECX_OFFSET + rbp], rcx
    mov rcx, [5 * PTR_SIZE + rax]
    mov [PROBE_CPU_EAX_OFFSET + rbp], rcx

    mov rcx, rax
    add rcx, 6 * PTR_SIZE
    mov [PROBE_CPU_ESP_OFFSET + rbp], rcx

    mov [PROBE_CPU_R8_OFFSET + rbp], r8
    mov [PROBE_CPU_R9_OFFSET + rbp], r9
    mov [PROBE_CPU_R10_OFFSET + rbp], r10
    mov [PROBE_CPU_R11_OFFSET + rbp], r11
    mov [PROBE_CPU_R12_OFFSET + rbp], r12
    mov [PROBE_CPU_R13_OFFSET + rbp], r13
    mov [PROBE_CPU_R14_OFFSET + rbp], r14
    mov [PROBE_CPU_R15_OFFSET + rbp], r15

    movq qword ptr [PROBE_CPU_XMM0_OFFSET + rbp], xmm0
    movq qword ptr [PROBE_CPU_XMM1_OFFSET + rbp], xmm1
    movq qword ptr [PROBE_CPU_XMM2_OFFSET + rbp], xmm2
    movq qword ptr [PROBE_CPU_XMM3_OFFSET + rbp], xmm3
    movq qword ptr [PROBE_CPU_XMM4_OFFSET + rbp], xmm4
    movq qword ptr [PROBE_CPU_XMM5_OFFSET + rbp], xmm5
    movq qword ptr [PROBE_CPU_XMM6_OFFSET + rbp], xmm6
    movq qword ptr [PROBE_CPU_XMM7_OFFSET + rbp], xmm7
    movq qword ptr [PROBE_CPU_XMM8_OFFSET + rbp], xmm8
    movq qword ptr [PROBE_CPU_XMM9_OFFSET + rbp], xmm9
    movq qword ptr [PROBE_CPU_XMM10_OFFSET + rbp], xmm10
    movq qword ptr [PROBE_CPU_XMM11_OFFSET + rbp], xmm11
    movq qword ptr [PROBE_CPU_XMM12_OFFSET + rbp], xmm12
    movq qword ptr [PROBE_CPU_XMM13_OFFSET + rbp], xmm13
    movq qword ptr [PROBE_CPU_XMM14_OFFSET + rbp], xmm14
    movq qword ptr [PROBE_CPU_XMM15_OFFSET + rbp], xmm15

    mov rcx, rbp ; the Probe::State* arg.
    sub rsp, 32 ; shadow space
    call qword ptr[PROBE_EXECUTOR_OFFSET + rbp]
    add rsp, 32

    ; Make sure the ProbeContext is entirely below the result stack pointer so
    ; that register values are still preserved when we call the initializeStack
    ; function.
    mov rcx, PROBE_SIZE + OUT_SIZE
    mov rax, rbp
    mov rdx, [PROBE_CPU_ESP_OFFSET + rbp]
    add rax, rcx
    cmp rdx, rax 
    jge ctiMasmProbeTrampolineProbeContextIsSafe

    ; Allocate a safe place on the stack below the result stack pointer to stash the ProbeContext.
    sub rdx, rcx
    and rdx, not 01fh ; Keep the stack pointer 32 bytes aligned.
    xor rax, rax
    mov rsp, rdx

    mov rcx, PROBE_SIZE

    ; Copy the ProbeContext to the safe place.
    ctiMasmProbeTrampolineCopyLoop:
    mov rdx, [rbp + rax]
    mov [rsp + rax], rdx
    add rax, PTR_SIZE
    cmp rcx, rax
    jg ctiMasmProbeTrampolineCopyLoop

    mov rbp, rsp

    ; Call initializeStackFunction if present.
    ctiMasmProbeTrampolineProbeContextIsSafe:
    xor rcx, rcx
    add rcx, [PROBE_INIT_STACK_FUNCTION_OFFSET + rbp]
    je ctiMasmProbeTrampolineRestoreRegisters

    mov rdx, rcx
    mov rcx, rbp ; the Probe::State* arg.
    sub rsp, 32 ; shadow space
    call rdx
    add rsp, 32

    ctiMasmProbeTrampolineRestoreRegisters:

    ; To enable probes to modify register state, we copy all registers
    ; out of the ProbeContext before returning.

    mov rdx, [PROBE_CPU_EDX_OFFSET + rbp]
    mov rbx, [PROBE_CPU_EBX_OFFSET + rbp]
    mov rsi, [PROBE_CPU_ESI_OFFSET + rbp]
    mov rdi, [PROBE_CPU_EDI_OFFSET + rbp]

    mov r8, [PROBE_CPU_R8_OFFSET + rbp]
    mov r9, [PROBE_CPU_R9_OFFSET + rbp]
    mov r10, [PROBE_CPU_R10_OFFSET + rbp]
    mov r11, [PROBE_CPU_R11_OFFSET + rbp]
    mov r12, [PROBE_CPU_R12_OFFSET + rbp]
    mov r13, [PROBE_CPU_R13_OFFSET + rbp]
    mov r14, [PROBE_CPU_R14_OFFSET + rbp]
    mov r15, [PROBE_CPU_R15_OFFSET + rbp]

    movq xmm0, qword ptr[PROBE_CPU_XMM0_OFFSET + rbp]
    movq xmm1, qword ptr[PROBE_CPU_XMM1_OFFSET + rbp]
    movq xmm2, qword ptr[PROBE_CPU_XMM2_OFFSET + rbp]
    movq xmm3, qword ptr[PROBE_CPU_XMM3_OFFSET + rbp]
    movq xmm4, qword ptr[PROBE_CPU_XMM4_OFFSET + rbp]
    movq xmm5, qword ptr[PROBE_CPU_XMM5_OFFSET + rbp]
    movq xmm6, qword ptr[PROBE_CPU_XMM6_OFFSET + rbp]
    movq xmm7, qword ptr[PROBE_CPU_XMM7_OFFSET + rbp]
    movq xmm8, qword ptr[PROBE_CPU_XMM8_OFFSET + rbp]
    movq xmm9, qword ptr[PROBE_CPU_XMM9_OFFSET + rbp]
    movq xmm10, qword ptr[PROBE_CPU_XMM10_OFFSET + rbp]
    movq xmm11, qword ptr[PROBE_CPU_XMM11_OFFSET + rbp]
    movq xmm12, qword ptr[PROBE_CPU_XMM12_OFFSET + rbp]
    movq xmm13, qword ptr[PROBE_CPU_XMM13_OFFSET + rbp]
    movq xmm14, qword ptr[PROBE_CPU_XMM14_OFFSET + rbp]
    movq xmm15, qword ptr[PROBE_CPU_XMM15_OFFSET + rbp]

    ; There are 6 more registers left to restore:
    ;     rax, rcx, rbp, rsp, rip, and rflags.

    ; The restoration process at ctiMasmProbeTrampolineEnd below works by popping
    ; 5 words off the stack into rflags, rax, rcx, rbp, and rip. These 5 words need
    ; to be pushed on top of the final esp value so that just by popping the 5 words,
    ; we'll get the esp that the probe wants to set. Let's call this area (for storing
    ; these 5 words) the restore area.
    mov rcx, [PROBE_CPU_ESP_OFFSET + rbp]
    sub rcx, 5 * PTR_SIZE

    ; rcx now points to the restore area.

    ; Copy remaining restore values from the ProbeContext to the restore area.
    ; Note: We already ensured above that the ProbeContext is in a safe location before
    ; calling the initializeStackFunction. The initializeStackFunction is not allowed to
    ; change the stack pointer again.
    mov rax, [PROBE_CPU_EFLAGS_OFFSET + rbp]
    mov [0 * PTR_SIZE + rcx], rax
    mov rax, [PROBE_CPU_EAX_OFFSET + rbp]
    mov [1 * PTR_SIZE + rcx], rax
    mov rax, [PROBE_CPU_ECX_OFFSET + rbp] 
    mov [2 * PTR_SIZE + rcx], rax
    mov rax, [PROBE_CPU_EBP_OFFSET + rbp]
    mov [3 * PTR_SIZE + rcx], rax
    mov rax, [PROBE_CPU_EIP_OFFSET + rbp]
    mov [4 * PTR_SIZE + rcx], rax
    mov rsp, rcx

    ; Do the remaining restoration by popping off the restore area.
    popfq
    pop rax
    pop rcx
    pop rbp
    ret
ctiMasmProbeTrampoline ENDP

_TEXT   ENDS

END
