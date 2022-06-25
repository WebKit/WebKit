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

// More on the RISC-V calling convention and registers:
// https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
// https://riscv.org/wp-content/uploads/2017/05/riscv-spec-v2.2.pdf (Chapter 20)

#define RegisterNames RISCV64Registers

#define FOR_EACH_GP_REGISTER(macro)    \
    macro(x0, "x0", 1, 0)              \
    macro(x1, "x1", 1, 0)              \
    macro(x2, "x2", 1, 1)              \
    macro(x3, "x3", 1, 0)              \
    macro(x4, "x4", 1, 0)              \
    macro(x5, "x5", 0, 0)              \
    macro(x6, "x6", 0, 0)              \
    macro(x7, "x7", 0, 0)              \
    macro(x8, "x8", 1, 1)              \
    macro(x9, "x9", 0, 1)              \
    macro(x10, "x10", 0, 0)            \
    macro(x11, "x11", 0, 0)            \
    macro(x12, "x12", 0, 0)            \
    macro(x13, "x13", 0, 0)            \
    macro(x14, "x14", 0, 0)            \
    macro(x15, "x15", 0, 0)            \
    macro(x16, "x16", 0, 0)            \
    macro(x17, "x17", 0, 0)            \
    macro(x18, "x18", 0, 1)            \
    macro(x19, "x19", 0, 1)            \
    macro(x20, "x20", 0, 1)            \
    macro(x21, "x21", 0, 1)            \
    macro(x22, "x22", 0, 1)            \
    macro(x23, "x23", 0, 1)            \
    macro(x24, "x24", 0, 1)            \
    macro(x25, "x25", 0, 1)            \
    macro(x26, "x26", 0, 1)            \
    macro(x27, "x27", 0, 1)            \
    macro(x28, "x28", 0, 0)            \
    macro(x29, "x29", 0, 0)            \
/* MacroAssembler scratch registers */ \
    macro(x30, "x30", 0, 0)            \
    macro(x31, "x31", 0, 0)

#define FOR_EACH_REGISTER_ALIAS(macro) \
    macro(zero, "zero", x0)            \
    macro(ra, "ra", x1)                \
    macro(sp, "sp", x2)                \
    macro(gp, "gp", x3)                \
    macro(tp, "tp", x4)                \
    macro(fp, "fp", x8)

#define FOR_EACH_SP_REGISTER(macro) \
    macro(pc, "pc")

#define FOR_EACH_FP_REGISTER(macro) \
    macro(f0, "f0", 0, 0)           \
    macro(f1, "f1", 0, 0)           \
    macro(f2, "f2", 0, 0)           \
    macro(f3, "f3", 0, 0)           \
    macro(f4, "f4", 0, 0)           \
    macro(f5, "f5", 0, 0)           \
    macro(f6, "f6", 0, 0)           \
    macro(f7, "f7", 0, 0)           \
    macro(f8, "f8", 0, 1)           \
    macro(f9, "f9", 0, 1)           \
    macro(f10, "f10", 0, 0)         \
    macro(f11, "f11", 0, 0)         \
    macro(f12, "f12", 0, 0)         \
    macro(f13, "f13", 0, 0)         \
    macro(f14, "f14", 0, 0)         \
    macro(f15, "f15", 0, 0)         \
    macro(f16, "f16", 0, 0)         \
    macro(f17, "f17", 0, 0)         \
    macro(f18, "f18", 0, 1)         \
    macro(f19, "f19", 0, 1)         \
    macro(f20, "f20", 0, 1)         \
    macro(f21, "f21", 0, 1)         \
    macro(f22, "f22", 0, 1)         \
    macro(f23, "f23", 0, 1)         \
    macro(f24, "f24", 0, 1)         \
    macro(f25, "f25", 0, 1)         \
    macro(f26, "f26", 0, 1)         \
    macro(f27, "f27", 0, 1)         \
    macro(f28, "f28", 0, 0)         \
    macro(f29, "f29", 0, 0)         \
    macro(f30, "f30", 0, 0)         \
    macro(f31, "f31", 0, 0)
