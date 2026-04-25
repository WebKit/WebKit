/*
 * Copyright (C) 2021 Igalia S.L.
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

#if CPU(RISCV64)

// More on the RISC-V calling convention and registers:
// https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
// https://riscv.org/wp-content/uploads/2017/05/riscv-spec-v2.2.pdf (Chapter 20)

#define RegisterNames RISCV64Registers

#define FOR_EACH_GP_REGISTER(macro)    \
    macro(x0, "x0"_s, 1, 0)              \
    macro(x1, "x1"_s, 1, 0)              \
    macro(x2, "x2"_s, 1, 1)              \
    macro(x3, "x3"_s, 1, 0)              \
    macro(x4, "x4"_s, 1, 0)              \
    macro(x5, "x5"_s, 0, 0)              \
    macro(x6, "x6"_s, 0, 0)              \
    macro(x7, "x7"_s, 0, 0)              \
    macro(x8, "x8"_s, 1, 1)              \
    macro(x9, "x9"_s, 0, 1)              \
    macro(x10, "x10"_s, 0, 0)            \
    macro(x11, "x11"_s, 0, 0)            \
    macro(x12, "x12"_s, 0, 0)            \
    macro(x13, "x13"_s, 0, 0)            \
    macro(x14, "x14"_s, 0, 0)            \
    macro(x15, "x15"_s, 0, 0)            \
    macro(x16, "x16"_s, 0, 0)            \
    macro(x17, "x17"_s, 0, 0)            \
    macro(x18, "x18"_s, 0, 1)            \
    macro(x19, "x19"_s, 0, 1)            \
    macro(x20, "x20"_s, 0, 1)            \
    macro(x21, "x21"_s, 0, 1)            \
    macro(x22, "x22"_s, 0, 1)            \
    macro(x23, "x23"_s, 0, 1)            \
    macro(x24, "x24"_s, 0, 1)            \
    macro(x25, "x25"_s, 0, 1)            \
    macro(x26, "x26"_s, 0, 1)            \
    macro(x27, "x27"_s, 0, 1)            \
    macro(x28, "x28"_s, 0, 0)            \
    macro(x29, "x29"_s, 0, 0)            \
/* MacroAssembler scratch registers */ \
    macro(x30, "x30"_s, 0, 0)            \
    macro(x31, "x31"_s, 0, 0)

#define FOR_EACH_REGISTER_ALIAS(macro) \
    macro(zero, "zero"_s, x0)            \
    macro(ra, "ra"_s, x1)                \
    macro(sp, "sp"_s, x2)                \
    macro(gp, "gp"_s, x3)                \
    macro(tp, "tp"_s, x4)                \
    macro(fp, "fp"_s, x8)

#define FOR_EACH_SP_REGISTER(macro) \
    macro(pc, "pc"_s)

#define FOR_EACH_FP_REGISTER(macro) \
    macro(f0, "f0"_s, 0, 0)           \
    macro(f1, "f1"_s, 0, 0)           \
    macro(f2, "f2"_s, 0, 0)           \
    macro(f3, "f3"_s, 0, 0)           \
    macro(f4, "f4"_s, 0, 0)           \
    macro(f5, "f5"_s, 0, 0)           \
    macro(f6, "f6"_s, 0, 0)           \
    macro(f7, "f7"_s, 0, 0)           \
    macro(f8, "f8"_s, 0, 1)           \
    macro(f9, "f9"_s, 0, 1)           \
    macro(f10, "f10"_s, 0, 0)         \
    macro(f11, "f11"_s, 0, 0)         \
    macro(f12, "f12"_s, 0, 0)         \
    macro(f13, "f13"_s, 0, 0)         \
    macro(f14, "f14"_s, 0, 0)         \
    macro(f15, "f15"_s, 0, 0)         \
    macro(f16, "f16"_s, 0, 0)         \
    macro(f17, "f17"_s, 0, 0)         \
    macro(f18, "f18"_s, 0, 1)         \
    macro(f19, "f19"_s, 0, 1)         \
    macro(f20, "f20"_s, 0, 1)         \
    macro(f21, "f21"_s, 0, 1)         \
    macro(f22, "f22"_s, 0, 1)         \
    macro(f23, "f23"_s, 0, 1)         \
    macro(f24, "f24"_s, 0, 1)         \
    macro(f25, "f25"_s, 0, 1)         \
    macro(f26, "f26"_s, 0, 1)         \
    macro(f27, "f27"_s, 0, 1)         \
    macro(f28, "f28"_s, 0, 0)         \
    macro(f29, "f29"_s, 0, 0)         \
    macro(f30, "f30"_s, 0, 0)         \
    macro(f31, "f31"_s, 0, 0)

#endif // CPU(RISCV64)
