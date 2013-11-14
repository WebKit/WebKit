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
    push rbp
    mov rbp, rax ; Save previous frame pointer
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx

    ; JIT operations can use up to 6 args (4 in registers and 2 on the stack).
    ; In addition, X86_64 ABI specifies that the worse case stack alignment
    ; requirement is 32 bytes. Based on these factors, we need to pad the stack
    ; an additional 28h bytes.
    sub rsp, 28h
    mov rbp, rdx
    mov r11, qword ptr[rbp] ; Put the previous frame pointer in the sentinel call frame above us
    mov qword ptr[r11], rax
    mov r14, 0FFFF000000000000h
    mov r15, 0FFFF000000000002h
    call rcx
    add rsp, 28h
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret
callToJavaScript ENDP

returnFromJavaScript PROC
    add rsp, 28h
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret
returnFromJavaScript ENDP
	
getHostCallReturnValue PROC
    sub r13, 40
    mov r13, rdi
    jmp getHostCallReturnValueWithExecState
getHostCallReturnValue ENDP

_TEXT   ENDS

END
