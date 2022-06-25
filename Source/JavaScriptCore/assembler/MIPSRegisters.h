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

#define RegisterNames MIPSRegisters

#define FOR_EACH_REGISTER(macro)                \
    FOR_EACH_GP_REGISTER(macro)                 \
    FOR_EACH_FP_REGISTER(macro)

#define FOR_EACH_GP_REGISTER(macro)             \
    macro(r0,  "zero", 0, 0)                    \
    macro(r1,  "at",   0, 0)                    \
    macro(r2,  "v0",   0, 0)                    \
    macro(r3,  "v1",   0, 0)                    \
    macro(r4,  "a0",   0, 0)                    \
    macro(r5,  "a1",   0, 0)                    \
    macro(r6,  "a2",   0, 0)                    \
    macro(r7,  "a3",   0, 0)                    \
    macro(r8,  "t0",   0, 0)                    \
    macro(r9,  "t1",   0, 0)                    \
    macro(r10, "t2",   0, 0)                    \
    macro(r11, "t3",   0, 0)                    \
    macro(r12, "t4",   0, 0)                    \
    macro(r13, "t5",   0, 0)                    \
    macro(r14, "t6",   0, 0)                    \
    macro(r15, "t7",   0, 0)                    \
    macro(r16, "s0",   0, 1)                    \
    macro(r17, "s1",   0, 1)                    \
    macro(r18, "s2",   0, 0)                    \
    macro(r19, "s3",   0, 0)                    \
    macro(r20, "s4",   0, 0)                    \
    macro(r21, "s5",   0, 0)                    \
    macro(r22, "s6",   0, 0)                    \
    macro(r23, "s7",   0, 0)                    \
    macro(r24, "t8",   0, 0)                    \
    macro(r25, "t9",   0, 0)                    \
    macro(r26, "k0",   0, 0)                    \
    macro(r27, "k1",   0, 0)                    \
    macro(r28, "gp",   0, 0)                    \
    macro(r29, "sp",   0, 0)                    \
    macro(r30, "fp",   0, 0)                    \
    macro(r31, "ra",   0, 0)

#define FOR_EACH_REGISTER_ALIAS(macro)          \
    macro(zero, r0)                             \
    macro(at,   r1)                             \
    macro(v0,   r2)                             \
    macro(v1,   r3)                             \
    macro(a0,   r4)                             \
    macro(a1,   r5)                             \
    macro(a2,   r6)                             \
    macro(a3,   r7)                             \
    macro(t0,   r8)                             \
    macro(t1,   r9)                             \
    macro(t2,   r10)                            \
    macro(t3,   r11)                            \
    macro(t4,   r12)                            \
    macro(t5,   r13)                            \
    macro(t6,   r14)                            \
    macro(t7,   r15)                            \
    macro(s0,   r16)                            \
    macro(s1,   r17)                            \
    macro(s2,   r18)                            \
    macro(s3,   r19)                            \
    macro(s4,   r20)                            \
    macro(s5,   r21)                            \
    macro(s6,   r22)                            \
    macro(s7,   r23)                            \
    macro(t8,   r24)                            \
    macro(t9,   r25)                            \
    macro(k0,   r26)                            \
    macro(k1,   r27)                            \
    macro(gp,   r28)                            \
    macro(sp,   r29)                            \
    macro(fp,   r30)                            \
    macro(ra,   r31)

#define FOR_EACH_SP_REGISTER(macro)             \
    macro(fir,  "fir",  0)                      \
    macro(fccr, "fccr", 25)                     \
    macro(fexr, "fexr", 26)                     \
    macro(fenr, "fenr", 28)                     \
    macro(fcsr, "fcsr", 31)                     \
    macro(pc,   "pc",   32)

#define FOR_EACH_FP_REGISTER(macro)             \
    macro(f0,  "f0", 0, 0)                      \
    macro(f1,  "f1", 0, 0)                      \
    macro(f2,  "f2", 0, 0)                      \
    macro(f3,  "f3", 0, 0)                      \
    macro(f4,  "f4", 0, 0)                      \
    macro(f5,  "f5", 0, 0)                      \
    macro(f6,  "f6", 0, 0)                      \
    macro(f7,  "f7", 0, 0)                      \
    macro(f8,  "f8", 0, 0)                      \
    macro(f9,  "f9", 0, 0)                      \
    macro(f10, "f10", 0, 0)                     \
    macro(f11, "f11", 0, 0)                     \
    macro(f12, "f12", 0, 0)                     \
    macro(f13, "f13", 0, 0)                     \
    macro(f14, "f14", 0, 0)                     \
    macro(f15, "f15", 0, 0)                     \
    macro(f16, "f16", 0, 0)                     \
    macro(f17, "f17", 0, 0)                     \
    macro(f18, "f18", 0, 0)                     \
    macro(f19, "f19", 0, 0)                     \
    macro(f20, "f20", 0, 0)                     \
    macro(f21, "f21", 0, 0)                     \
    macro(f22, "f22", 0, 0)                     \
    macro(f23, "f23", 0, 0)                     \
    macro(f24, "f24", 0, 0)                     \
    macro(f25, "f25", 0, 0)                     \
    macro(f26, "f26", 0, 0)                     \
    macro(f27, "f27", 0, 0)                     \
    macro(f28, "f28", 0, 0)                     \
    macro(f29, "f29", 0, 0)                     \
    macro(f30, "f30", 0, 0)                     \
    macro(f31, "f31", 0, 0)
