;/*
; Copyright (C) 2013 Digia Plc. and/or its subsidiary(-ies)
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

PUBLIC callToJavaScript
PUBLIC returnFromJavaScript
PUBLIC getHostCallReturnValue

_TEXT   SEGMENT

callToJavaScript PROC
    ;; This function is believed to be an accurate adaptation of the assembly created by the llint stub of the
    ;; same name with changes for argument register differences.
    mov r10, qword ptr[rsp]
    push rbp
    mov rax, rbp ; Save previous frame pointer
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    push rsi
    push rdi

    ; JIT operations can use up to 6 args (4 in registers and 2 on the stack).
    ; In addition, X86_64 ABI specifies that the worse case stack alignment
    ; requirement is 32 bytes. Based on these factors, we need to pad the stack
    ; an additional 28h bytes.
    sub rsp, 28h

    mov rbp, r9
    sub rbp, 40
    mov qword ptr[rbp + 40], 0
    mov qword ptr[rbp + 32], rdx

    mov rax, qword ptr[rdx]
    mov qword ptr[rbp + 24], rax
    mov qword ptr[rbp + 16], 1
    mov qword ptr[rbp + 8], r10
    mov qword ptr[rbp], rax
    mov rax, rbp

    mov ebx, dword ptr[r8 + 40]
    add rbx, 6
    sal rbx, 3
    sub rbp, rbx
    mov qword ptr[rbp], rax

    mov rax, 5

copyHeaderLoop:
    sub rax, 1
    mov r10, qword ptr[r8 + rax * 8]
    mov qword ptr[rbp + rax * 8 + 16], r10
    test rax, rax
    jnz copyHeaderLoop

    mov ebx, dword ptr[r8 + 24]
    sub rbx, 1
    mov r10d, dword ptr[r8 + 40]
    sub r10, 1
    cmp rbx, r10
    je copyArgs
    mov rax, 0ah

fillExtraArgsLoop:
    sub r10, 1
    mov qword ptr[rbp + r10 * 8 + 56], rax
    cmp rbx, r10
    jne fillExtraArgsLoop

copyArgs:
    mov rax, qword ptr[r8 + 48]

copyArgsLoop:
    test ebx, ebx
    jz copyArgsDone
    sub ebx, 1
    mov r10, qword ptr[rax + rbx * 8]
    mov qword ptr[rbp + rbx * 8 + 56], r10
    jmp copyArgsLoop

copyArgsDone:
    mov qword ptr[rdx], rbp
    mov r14, 0FFFF000000000000h
    mov r15, 0FFFF000000000002h
    call rcx
    cmp qword ptr[rbp + 16], 1
    je calleeFramePopped
    mov rbp, qword ptr[rbp]

calleeFramePopped:
    mov rbx, qword ptr[rbp + 32] ; VM.topCallFrame
    mov r10, qword ptr[rbp + 24]
    mov qword ptr[rbx], r10
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret
callToJavaScript ENDP

callToNativeFunction PROC
    ;; This function is believed to be an accurate adaptation of the assembly created by the llint stub of the
    ;; same name with changes for argument register differences.
    mov r10, qword ptr[rsp]
    push rbp
    mov rax, rbp ; Save previous frame pointer
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    push rsi
    push rdi

    ; JIT operations can use up to 6 args (4 in registers and 2 on the stack).
    ; In addition, X86_64 ABI specifies that the worse case stack alignment
    ; requirement is 32 bytes. Based on these factors, we need to pad the stack
    ; an additional 28h bytes.
    sub rsp, 28h

    mov rbp, r9
    sub rbp, 40
    mov qword ptr[rbp + 40], 0
    mov qword ptr[rbp + 32], rdx

    mov rax, qword ptr[rdx]
    mov qword ptr[rbp + 24], rax
    mov qword ptr[rbp + 16], 1
    mov qword ptr[rbp + 8], r10
    mov qword ptr[rbp], rax
    mov rax, rbp

    mov ebx, dword ptr[r8 + 40]
    add rbx, 6
    sal rbx, 3
    sub rbp, rbx
    mov qword ptr[rbp], rax

    mov rax, 5

copyHeaderLoop:
    sub rax, 1
    mov r10, qword ptr[r8 + rax * 8]
    mov qword ptr[rbp + rax * 8 + 16], r10
    test rax, rax
    jnz copyHeaderLoop

    mov ebx, dword ptr[r8 + 24]
    sub rbx, 1
    mov r10d, dword ptr[r8 + 40]
    sub r10, 1
    cmp rbx, r10
    je copyArgs
    mov rax, 0ah

fillExtraArgsLoop:
    sub r10, 1
    mov qword ptr[rbp + r10 * 8 + 56], rax
    cmp rbx, r10
    jne fillExtraArgsLoop

copyArgs:
    mov rax, qword ptr[r8 + 48]

copyArgsLoop:
    test rbx, rbx
    jz copyArgsDone
    sub rbx, 1
    mov r10, qword ptr[rax + rbx * 8]
    mov qword ptr[rbp + rbx * 8 + 56], r10
    jmp copyArgsLoop

copyArgsDone:
    mov qword ptr[rdx], rbp
    mov r14, 0FFFF000000000000h
    mov r15, 0FFFF000000000002h

    mov rax, rcx
    mov rcx, rbp
    call rax

    cmp qword ptr[rbp + 16], 1
    je calleeFramePopped
    mov rbp, qword ptr[rbp]

calleeFramePopped:
    mov rbx, qword ptr[rbp + 32] ; VM.topCallFrame
    mov r10, qword ptr[rbp + 24]
    mov qword ptr[rbx], r10
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret
callToNativeFunction ENDP

returnFromJavaScript PROC
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret
returnFromJavaScript ENDP
	
getHostCallReturnValue PROC
    mov rbp, [rbp] ; CallFrame
    mov rcx, rbp ; rcx is first argument register on Windows
    jmp getHostCallReturnValueWithExecState
getHostCallReturnValue ENDP

_TEXT   ENDS

END
