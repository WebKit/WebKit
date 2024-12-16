/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023-2024 Loongson Technology. All rights reserved.
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

#if CPU(LOONGARCH64)

// More on the LOONGARCH calling convention and registers:
// https://loongson.github.io/LoongArch-Documentation/LoongArch-ELF-ABI-EN.html

#define RegisterNames LOONGARCH64Registers

#define FOR_EACH_GP_REGISTER(macro)    \
    macro(r0, "r0"_s, 1, 0)              \
    macro(r1, "r1"_s, 1, 0)              \
    macro(r2, "r2"_s, 1, 0)              \
    macro(r3, "r3"_s, 1, 1)              \
    /* Parameter/result registers. */  \
    macro(r4, "r4"_s, 0, 0)              \
    macro(r5, "r5"_s, 0, 0)              \
    macro(r6, "r6"_s, 0, 0)              \
    macro(r7, "r7"_s, 0, 0)              \
    macro(r8, "r8"_s, 0, 0)              \
    macro(r9, "r9"_s, 0, 0)              \
    macro(r10, "r10"_s, 0, 0)            \
    macro(r11, "r11"_s, 0, 0)            \
    /* Temporary registers. */         \
    macro(r12, "r12"_s, 0, 0)            \
    macro(r13, "r13"_s, 0, 0)            \
    macro(r14, "r14"_s, 0, 0)            \
    macro(r15, "r15"_s, 0, 0)            \
    macro(r16, "r16"_s, 0, 0)            \
    macro(r17, "r17"_s, 0, 0)            \
    macro(r18, "r18"_s, 0, 0)            \
    macro(r19, "r19"_s, 0, 0)            \
    macro(r20, "r20"_s, 0, 0)            \
    macro(r21, "r21"_s, 1, 0)            \
    /* Callee-saved. */                \
    macro(r22, "r22"_s, 1, 1)            \
    macro(r23, "r23"_s, 0, 1)            \
    macro(r24, "r24"_s, 0, 1)            \
    macro(r25, "r25"_s, 0, 1)            \
    macro(r26, "r26"_s, 0, 1)            \
    macro(r27, "r27"_s, 0, 1)            \
    macro(r28, "r28"_s, 0, 1)            \
    macro(r29, "r29"_s, 0, 1)            \
    macro(r30, "r30"_s, 0, 1)            \
    macro(r31, "r31"_s, 0, 1)

#define FOR_EACH_REGISTER_ALIAS(macro) \
    macro(zero, "zero"_s, r0)            \
    macro(ra, "ra"_s, r1)                \
    macro(tp, "tp"_s, r2)                \
    macro(sp, "sp"_s, r3)                \
    macro(rx, "rx"_s, r21)               \
    macro(gp, "gp"_s, r31)               \
    macro(fp, "fp"_s, r22)

#define FOR_EACH_SP_REGISTER(macro)    \
    macro(pc, "pc"_s)

#define FOR_EACH_FP_REGISTER(macro)    \
    /* Parameter/result registers. */  \
    macro(f0, "f0"_s, 0, 0)              \
    macro(f1, "f1"_s, 0, 0)              \
    macro(f2, "f2"_s, 0, 0)              \
    macro(f3, "f3"_s, 0, 0)              \
    macro(f4, "f4"_s, 0, 0)              \
    macro(f5, "f5"_s, 0, 0)              \
    macro(f6, "f6"_s, 0, 0)              \
    macro(f7, "f7"_s, 0, 0)              \
    /* Temporary registers. */         \
    macro(f8, "f8"_s, 0, 0)              \
    macro(f9, "f9"_s, 0, 0)              \
    macro(f10, "f10"_s, 0, 0)            \
    macro(f11, "f11"_s, 0, 0)            \
    macro(f12, "f12"_s, 0, 0)            \
    macro(f13, "f13"_s, 0, 0)            \
    macro(f14, "f14"_s, 0, 0)            \
    macro(f15, "f15"_s, 0, 0)            \
    macro(f16, "f16"_s, 0, 0)            \
    macro(f17, "f17"_s, 0, 0)            \
    macro(f18, "f18"_s, 0, 0)            \
    macro(f19, "f19"_s, 0, 0)            \
    macro(f20, "f20"_s, 0, 0)            \
    macro(f21, "f21"_s, 0, 0)            \
    macro(f22, "f22"_s, 0, 0)            \
    macro(f23, "f23"_s, 0, 0)            \
    /* Callee-saved. */                \
    macro(f24, "f24"_s, 0, 1)            \
    macro(f25, "f25"_s, 0, 1)            \
    macro(f26, "f26"_s, 0, 1)            \
    macro(f27, "f27"_s, 0, 1)            \
    macro(f28, "f28"_s, 0, 1)            \
    macro(f29, "f29"_s, 0, 1)            \
    macro(f30, "f30"_s, 0, 1)            \
    macro(f31, "f31"_s, 0, 1)

#define FOR_EACH_CF_REGISTER(macro)    \
    macro(fcc0, "fcc0"_s, 0, 0)          \
    macro(fcc1, "fcc1"_s, 0, 0)          \
    macro(fcc2, "fcc2"_s, 0, 0)          \
    macro(fcc3, "fcc3"_s, 0, 0)          \
    macro(fcc4, "fcc4"_s, 0, 0)          \
    macro(fcc5, "fcc5"_s, 0, 0)          \
    macro(fcc6, "fcc6"_s, 0, 0)          \
    macro(fcc7, "fcc7"_s, 0, 0)

#endif // CPU(LOONGARCH64)
