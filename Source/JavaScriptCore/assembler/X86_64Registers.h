/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#define RegisterNames X86Registers

#if !OS(WINDOWS)

#define FOR_EACH_GP_REGISTER(macro)             \
    macro(eax, "rax", 0, 0)                     \
    macro(ecx, "rcx", 0, 0)                     \
    macro(edx, "rdx", 0, 0)                     \
    macro(ebx, "rbx", 0, 1)                     \
    macro(esp, "rsp", 0, 0)                     \
    macro(ebp, "rbp", 0, 1)                     \
    macro(esi, "rsi", 0, 0)                     \
    macro(edi, "rdi", 0, 0)                     \
    macro(r8,  "r8",  0, 0)                     \
    macro(r9,  "r9",  0, 0)                     \
    macro(r10, "r10", 0, 0)                     \
    macro(r11, "r11", 0, 0)                     \
    macro(r12, "r12", 0, 1)                     \
    macro(r13, "r13", 0, 1)                     \
    macro(r14, "r14", 0, 1)                     \
    macro(r15, "r15", 0, 1)

#else // OS(WINDOWS)

#define FOR_EACH_GP_REGISTER(macro)             \
    macro(eax, "rax", 0, 0)                     \
    macro(ecx, "rcx", 0, 0)                     \
    macro(edx, "rdx", 0, 0)                     \
    macro(ebx, "rbx", 0, 1)                     \
    macro(esp, "rsp", 0, 0)                     \
    macro(ebp, "rbp", 0, 1)                     \
    macro(esi, "rsi", 0, 1)                     \
    macro(edi, "rdi", 0, 1)                     \
    macro(r8,  "r8",  0, 0)                     \
    macro(r9,  "r9",  0, 0)                     \
    macro(r10, "r10", 0, 0)                     \
    macro(r11, "r11", 0, 0)                     \
    macro(r12, "r12", 0, 1)                     \
    macro(r13, "r13", 0, 1)                     \
    macro(r14, "r14", 0, 1)                     \
    macro(r15, "r15", 0, 1)

#endif // !OS(WINDOWS)

#define FOR_EACH_FP_REGISTER(macro)             \
    macro(xmm0,  "xmm0",  0, 0)                  \
    macro(xmm1,  "xmm1",  0, 0)                  \
    macro(xmm2,  "xmm2",  0, 0)                  \
    macro(xmm3,  "xmm3",  0, 0)                  \
    macro(xmm4,  "xmm4",  0, 0)                  \
    macro(xmm5,  "xmm5",  0, 0)                  \
    macro(xmm6,  "xmm6",  0, 0)                  \
    macro(xmm7,  "xmm7",  0, 0)                  \
    macro(xmm8,  "xmm8",  0, 0)                  \
    macro(xmm9,  "xmm9",  0, 0)                  \
    macro(xmm10, "xmm10", 0, 0)                  \
    macro(xmm11, "xmm11", 0, 0)                  \
    macro(xmm12, "xmm12", 0, 0)                  \
    macro(xmm13, "xmm13", 0, 0)                  \
    macro(xmm14, "xmm14", 0, 0)                  \
    macro(xmm15, "xmm15", 0, 0)

#define FOR_EACH_SP_REGISTER(macro)             \
    macro(eip,    "eip",    0, 0)               \
    macro(eflags, "eflags", 0, 0)
