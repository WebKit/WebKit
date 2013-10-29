;/*
; * Copyright (C) 2013 Apple, Inc. All rights reserved
; *
; * Redistribution and use in source and binary forms, with or without
; * modification, are permitted provided that the following conditions
; * are met:
; * 1. Redistributions of source code must retain the above copyright
; *    notice, this list of conditions and the following disclaimer.
; * 2. Redistributions in binary form must reproduce the above copyright
; *    notice, this list of conditions and the following disclaimer in the
; *    documentation and/or other materials provided with the distribution.
; *
; * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
; * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
; * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
; * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
; * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
; * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
; * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
; * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
; */

; Tell Windows to trust our error handler. This must be done within an assembly
; module. We cannot do it on-the-fly in our C++ code.
;
; Note also (confirmed by Raymond Chen) that we must use this assembly thunk
; to call our custom exception handler. (See http://jpassing.com/2008/05/20/fun-with-low-level-seh/)

.386
.model FLAT, STDCALL

EXTERN exceptionHandler@16 : near   ; Defined in StructuredExceptionHandlerSupressor.cpp

exceptionHandlerThunk proto
.safeseh exceptionHandlerThunk

.code
exceptionHandlerThunk proc
    jmp exceptionHandler@16
exceptionHandlerThunk endp

END