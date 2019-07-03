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

#define RegisterNames ARM64Registers

#define FOR_EACH_REGISTER(macro)                \
    FOR_EACH_GP_REGISTER(macro)                 \
    FOR_EACH_FP_REGISTER(macro)

// We don't include LR in the set of callee-save registers even though it technically belongs
// there. This is because we use this set to describe the set of registers that need to be saved
// beyond what you would save by the platform-agnostic "preserve return address" and "restore
// return address" operations in CCallHelpers.

#if !PLATFORM(IOS_FAMILY)
#define FOR_EACH_GP_REGISTER(macro)                             \
    /* Parameter/result registers. */                           \
    macro(x0,  "x0",  0, 0)                                     \
    macro(x1,  "x1",  0, 0)                                     \
    macro(x2,  "x2",  0, 0)                                     \
    macro(x3,  "x3",  0, 0)                                     \
    macro(x4,  "x4",  0, 0)                                     \
    macro(x5,  "x5",  0, 0)                                     \
    macro(x6,  "x6",  0, 0)                                     \
    macro(x7,  "x7",  0, 0)                                     \
    /* Indirect result location register. */                    \
    macro(x8,  "x8",  0, 0)                                     \
    /* Temporary registers. */                                  \
    macro(x9,  "x9",  0, 0)                                     \
    macro(x10, "x10", 0, 0)                                     \
    macro(x11, "x11", 0, 0)                                     \
    macro(x12, "x12", 0, 0)                                     \
    macro(x13, "x13", 0, 0)                                     \
    macro(x14, "x14", 0, 0)                                     \
    macro(x15, "x15", 0, 0)                                     \
    /* Intra-procedure-call scratch registers (temporary). */   \
    macro(x16, "x16", 0, 0)                                     \
    macro(x17, "x17", 0, 0)                                     \
    /* Platform Register (temporary). */                        \
    macro(x18, "x18", 0, 0)                                     \
    /* Callee-saved. */                                         \
    macro(x19, "x19", 0, 1)                                     \
    macro(x20, "x20", 0, 1)                                     \
    macro(x21, "x21", 0, 1)                                     \
    macro(x22, "x22", 0, 1)                                     \
    macro(x23, "x23", 0, 1)                                     \
    macro(x24, "x24", 0, 1)                                     \
    macro(x25, "x25", 0, 1)                                     \
    macro(x26, "x26", 0, 1)                                     \
    macro(x27, "x27", 0, 1)                                     \
    macro(x28, "x28", 0, 1)                                     \
    /* Special. */                                              \
    macro(fp,  "fp",  0, 1)                                     \
    macro(lr,  "lr",  1, 0)                                     \
    macro(sp,  "sp",  0, 0)
#else
#define FOR_EACH_GP_REGISTER(macro)                             \
    /* Parameter/result registers. */                           \
    macro(x0,  "x0",  0, 0)                                     \
    macro(x1,  "x1",  0, 0)                                     \
    macro(x2,  "x2",  0, 0)                                     \
    macro(x3,  "x3",  0, 0)                                     \
    macro(x4,  "x4",  0, 0)                                     \
    macro(x5,  "x5",  0, 0)                                     \
    macro(x6,  "x6",  0, 0)                                     \
    macro(x7,  "x7",  0, 0)                                     \
    /* Indirect result location register. */                    \
    macro(x8,  "x8",  0, 0)                                     \
    /* Temporary registers. */                                  \
    macro(x9,  "x9",  0, 0)                                     \
    macro(x10, "x10", 0, 0)                                     \
    macro(x11, "x11", 0, 0)                                     \
    macro(x12, "x12", 0, 0)                                     \
    macro(x13, "x13", 0, 0)                                     \
    macro(x14, "x14", 0, 0)                                     \
    macro(x15, "x15", 0, 0)                                     \
    /* Intra-procedure-call scratch registers (temporary). */   \
    macro(x16, "x16", 0, 0)                                     \
    macro(x17, "x17", 0, 0)                                     \
    /* Platform Register (temporary). */                        \
    macro(x18, "x18", 1, 0)                                     \
    /* Callee-saved. */                                         \
    macro(x19, "x19", 0, 1)                                     \
    macro(x20, "x20", 0, 1)                                     \
    macro(x21, "x21", 0, 1)                                     \
    macro(x22, "x22", 0, 1)                                     \
    macro(x23, "x23", 0, 1)                                     \
    macro(x24, "x24", 0, 1)                                     \
    macro(x25, "x25", 0, 1)                                     \
    macro(x26, "x26", 0, 1)                                     \
    macro(x27, "x27", 0, 1)                                     \
    macro(x28, "x28", 0, 1)                                     \
    /* Special. */                                              \
    macro(fp,  "fp",  0, 1)                                     \
    macro(lr,  "lr",  1, 0)                                     \
    macro(sp,  "sp",  0, 0)
#endif

#define FOR_EACH_REGISTER_ALIAS(macro)          \
    macro(ip0, "ip0", x16)                      \
    macro(ip1, "ip1", x17)                      \
    macro(x29, "x29", fp)                       \
    macro(x30, "x30", lr)                       \
    macro(zr,  "zr",  0x3f)

#define FOR_EACH_SP_REGISTER(macro)             \
    macro(pc,   "pc")                           \
    macro(nzcv, "nzcv")                         \
    macro(fpsr, "fpsr")

#define FOR_EACH_FP_REGISTER(macro)             \
    /* Parameter/result registers. */           \
    macro(q0,  "q0",  0, 0)                     \
    macro(q1,  "q1",  0, 0)                     \
    macro(q2,  "q2",  0, 0)                     \
    macro(q3,  "q3",  0, 0)                     \
    macro(q4,  "q4",  0, 0)                     \
    macro(q5,  "q5",  0, 0)                     \
    macro(q6,  "q6",  0, 0)                     \
    macro(q7,  "q7",  0, 0)                     \
    /* Callee-saved (up to 64-bits only!). */   \
    macro(q8,  "q8",  0, 1)                     \
    macro(q9,  "q9",  0, 1)                     \
    macro(q10, "q10", 0, 1)                     \
    macro(q11, "q11", 0, 1)                     \
    macro(q12, "q12", 0, 1)                     \
    macro(q13, "q13", 0, 1)                     \
    macro(q14, "q14", 0, 1)                     \
    macro(q15, "q15", 0, 1)                     \
    /* Temporary registers. */                  \
    macro(q16, "q16", 0, 0)                     \
    macro(q17, "q17", 0, 0)                     \
    macro(q18, "q18", 0, 0)                     \
    macro(q19, "q19", 0, 0)                     \
    macro(q20, "q20", 0, 0)                     \
    macro(q21, "q21", 0, 0)                     \
    macro(q22, "q22", 0, 0)                     \
    macro(q23, "q23", 0, 0)                     \
    macro(q24, "q24", 0, 0)                     \
    macro(q25, "q25", 0, 0)                     \
    macro(q26, "q26", 0, 0)                     \
    macro(q27, "q27", 0, 0)                     \
    macro(q28, "q28", 0, 0)                     \
    macro(q29, "q29", 0, 0)                     \
    macro(q30, "q30", 0, 0)                     \
    macro(q31, "q31", 0, 0)

