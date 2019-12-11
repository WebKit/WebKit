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

#define RegisterNames ARMRegisters

#define FOR_EACH_REGISTER(macro)                \
    FOR_EACH_GP_REGISTER(macro)                 \
    FOR_EACH_FP_REGISTER(macro)

#define FOR_EACH_FP_REGISTER(macro)             \
    FOR_EACH_FP_DOUBLE_REGISTER(macro)

#if !PLATFORM(IOS_FAMILY)
#define FOR_EACH_GP_REGISTER(macro)             \
    macro(r0,  "r0",  0, 0)                     \
    macro(r1,  "r1",  0, 0)                     \
    macro(r2,  "r2",  0, 0)                     \
    macro(r3,  "r3",  0, 0)                     \
    macro(r4,  "r4",  0, 1)                     \
    macro(r5,  "r5",  0, 1)                     \
    macro(r6,  "r6",  0, 1)                     \
    macro(r7,  "r7",  0, 0)                     \
    macro(r8,  "r8",  0, 1)                     \
    macro(r9,  "r9",  0, 1)                     \
    macro(r10, "r10", 0, 1)                     \
    macro(r11, "r11", 0, 1)                     \
    macro(r12, "ip",  0, 0)                     \
    macro(r13, "sp",  0, 0)                     \
    macro(r14, "lr",  1, 0)                     \
    macro(r15, "pc",  1, 0)
#else
#define FOR_EACH_GP_REGISTER(macro)             \
    macro(r0,  "r0",  0, 0)                     \
    macro(r1,  "r1",  0, 0)                     \
    macro(r2,  "r2",  0, 0)                     \
    macro(r3,  "r3",  0, 0)                     \
    macro(r4,  "r4",  0, 1)                     \
    macro(r5,  "r5",  0, 1)                     \
    macro(r6,  "r6",  0, 1)                     \
    macro(r7,  "r7",  0, 0)                     \
    macro(r8,  "r8",  0, 1)                     \
    macro(r9,  "r9",  0, 0)                     \
    macro(r10, "r10", 0, 1)                     \
    macro(r11, "r11", 0, 1)                     \
    macro(r12, "ip",  0, 0)                     \
    macro(r13, "sp",  0, 0)                     \
    macro(r14, "lr",  1, 0)                     \
    macro(r15, "pc",  1, 0)
#endif

#define FOR_EACH_REGISTER_ALIAS(macro)          \
    macro(fp, "fp", r7)                         \
    macro(sb, "sb", r9)                         \
    macro(sl, "sl", r10)                        \
    macro(ip, "ip", r12)                        \
    macro(sp, "sp", r13)                        \
    macro(lr, "lr", r14)                        \
    macro(pc, "pc", r15)

#define FOR_EACH_SP_REGISTER(macro)             \
    macro(apsr,  "apsr")                        \
    macro(fpscr, "fpscr")

#define FOR_EACH_FP_SINGLE_REGISTER(macro)      \
    macro(s0,  "s0",  0, 0)                     \
    macro(s1,  "s1",  0, 0)                     \
    macro(s2,  "s2",  0, 0)                     \
    macro(s3,  "s3",  0, 0)                     \
    macro(s4,  "s4",  0, 0)                     \
    macro(s5,  "s5",  0, 0)                     \
    macro(s6,  "s6",  0, 0)                     \
    macro(s7,  "s7",  0, 0)                     \
    macro(s8,  "s8",  0, 0)                     \
    macro(s9,  "s9",  0, 0)                     \
    macro(s10, "s10", 0, 0)                     \
    macro(s11, "s11", 0, 0)                     \
    macro(s12, "s12", 0, 0)                     \
    macro(s13, "s13", 0, 0)                     \
    macro(s14, "s14", 0, 0)                     \
    macro(s15, "s15", 0, 0)                     \
    macro(s16, "s16", 0, 0)                     \
    macro(s17, "s17", 0, 0)                     \
    macro(s18, "s18", 0, 0)                     \
    macro(s19, "s19", 0, 0)                     \
    macro(s20, "s20", 0, 0)                     \
    macro(s21, "s21", 0, 0)                     \
    macro(s22, "s22", 0, 0)                     \
    macro(s23, "s23", 0, 0)                     \
    macro(s24, "s24", 0, 0)                     \
    macro(s25, "s25", 0, 0)                     \
    macro(s26, "s26", 0, 0)                     \
    macro(s27, "s27", 0, 0)                     \
    macro(s28, "s28", 0, 0)                     \
    macro(s29, "s29", 0, 0)                     \
    macro(s30, "s30", 0, 0)                     \
    macro(s31, "s31", 0, 0)

#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
#define FOR_EACH_FP_DOUBLE_REGISTER(macro)      \
    macro(d0,  "d0",  0, 0)                     \
    macro(d1,  "d1",  0, 0)                     \
    macro(d2,  "d2",  0, 0)                     \
    macro(d3,  "d3",  0, 0)                     \
    macro(d4,  "d4",  0, 0)                     \
    macro(d5,  "d5",  0, 0)                     \
    macro(d6,  "d6",  0, 0)                     \
    macro(d7,  "d7",  0, 0)                     \
    macro(d8,  "d8",  0, 0)                     \
    macro(d9,  "d9",  0, 0)                     \
    macro(d10, "d10", 0, 0)                     \
    macro(d11, "d11", 0, 0)                     \
    macro(d12, "d12", 0, 0)                     \
    macro(d13, "d13", 0, 0)                     \
    macro(d14, "d14", 0, 0)                     \
    macro(d15, "d15", 0, 0)                     \
    macro(d16, "d16", 0, 0)                     \
    macro(d17, "d17", 0, 0)                     \
    macro(d18, "d18", 0, 0)                     \
    macro(d19, "d19", 0, 0)                     \
    macro(d20, "d20", 0, 0)                     \
    macro(d21, "d21", 0, 0)                     \
    macro(d22, "d22", 0, 0)                     \
    macro(d23, "d23", 0, 0)                     \
    macro(d24, "d24", 0, 0)                     \
    macro(d25, "d25", 0, 0)                     \
    macro(d26, "d26", 0, 0)                     \
    macro(d27, "d27", 0, 0)                     \
    macro(d28, "d28", 0, 0)                     \
    macro(d29, "d29", 0, 0)                     \
    macro(d30, "d30", 0, 0)                     \
    macro(d31, "d31", 0, 0)
#else
#define FOR_EACH_FP_DOUBLE_REGISTER(macro)      \
    macro(d0,  "d0",  0, 0)                     \
    macro(d1,  "d1",  0, 0)                     \
    macro(d2,  "d2",  0, 0)                     \
    macro(d3,  "d3",  0, 0)                     \
    macro(d4,  "d4",  0, 0)                     \
    macro(d5,  "d5",  0, 0)                     \
    macro(d6,  "d6",  0, 0)                     \
    macro(d7,  "d7",  0, 0)                     \
    macro(d8,  "d8",  0, 0)                     \
    macro(d9,  "d9",  0, 0)                     \
    macro(d10, "d10", 0, 0)                     \
    macro(d11, "d11", 0, 0)                     \
    macro(d12, "d12", 0, 0)                     \
    macro(d13, "d13", 0, 0)                     \
    macro(d14, "d14", 0, 0)                     \
    macro(d15, "d15", 0, 0)
#endif

#if CPU(ARM_NEON)
#define FOR_EACH_FP_QUAD_REGISTER(macro)        \
    macro(q0, "q0", 0, 0)                       \
    macro(q1, "q1", 0, 0)                       \
    macro(q2, "q2", 0, 0)                       \
    macro(q3, "q3", 0, 0)                       \
    macro(q4, "q4", 0, 0)                       \
    macro(q5, "q5", 0, 0)                       \
    macro(q6, "q6", 0, 0)                       \
    macro(q7, "q7", 0, 0)                       \
    macro(q8, "q8", 0, 0)                       \
    macro(q9, "q9", 0, 0)                       \
    macro(q10, "q10", 0, 0)                     \
    macro(q11, "q11", 0, 0)                     \
    macro(q12, "q12", 0, 0)                     \
    macro(q13, "q13", 0, 0)                     \
    macro(q14, "q14", 0, 0)                     \
    macro(q15, "q15", 0, 0)
#endif
