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

#include <wtf/Platform.h>

#define RegisterNames X86Registers

#define FOR_EACH_REGISTER(macro)                \
    FOR_EACH_GP_REGISTER(macro)                 \
    FOR_EACH_FP_REGISTER(macro)

#define FOR_EACH_GP_REGISTER(macro)             \
    macro(eax, "eax", 0, 0)                     \
    macro(ecx, "ecx", 0, 0)                     \
    macro(edx, "edx", 0, 0)                     \
    macro(ebx, "ebx", 0, 1)                     \
    macro(esp, "esp", 0, 0)                     \
    macro(ebp, "ebp", 0, 1)                     \
    macro(esi, "esi", 0, 1)                     \
    macro(edi, "edi", 0, 1)

#define FOR_EACH_FP_REGISTER(macro)             \
    macro(xmm0, "xmm0", 0, 0)                   \
    macro(xmm1, "xmm1", 0, 0)                   \
    macro(xmm2, "xmm2", 0, 0)                   \
    macro(xmm3, "xmm3", 0, 0)                   \
    macro(xmm4, "xmm4", 0, 0)                   \
    macro(xmm5, "xmm5", 0, 0)                   \
    macro(xmm6, "xmm6", 0, 0)                   \
    macro(xmm7, "xmm7", 0, 0)

#define FOR_EACH_SP_REGISTER(macro)             \
    macro(eip,    "eip",    0, 0)               \
    macro(eflags, "eflags", 0, 0)

