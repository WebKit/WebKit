/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Felician Marton
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(FILTERS)
#include "FECompositeArithmeticNEON.h"

#if CPU(ARM_NEON) && COMPILER(GCC)

namespace WebCore {

#define ASSTRING(str) #str
#define TOSTRING(value) ASSTRING(value)

#define NL "\n"

#define SOURCE_R     "r0"
#define DEST_R       "r1"
#define END_R        "r2"
#define K_R          "r3"
#define NEXTPIXEL_R  "r12"

#define TEMP_Q       "q0"
#define TEMP_D0      "d0"
#define TEMP_D00     "d0[0]"
#define TEMP_D01     "d0[1]"
#define TEMP_D10     "d1[0]"
#define TEMP_D11     "d1[1]"
#define PIXEL1_Q     "q1"
#define PIXEL1_D0    "d2"
#define PIXEL1_D00   "d2[0]"
#define PIXEL2_Q     "q2"
#define PIXEL2_D0    "d4"
#define PIXEL2_D00   "d4[0]"
#define BYTEMAX_Q    "q3"
#define K1_Q         "q8"
#define K2_Q         "q9"
#define K3_Q         "q10"
#define K4_Q         "q11"

asm ( // NOLINT
".globl " TOSTRING(neonDrawCompositeArithmetic) NL
TOSTRING(neonDrawCompositeArithmetic) ":" NL
    "cmp " END_R ", #0" NL
    "bxeq lr"  NL
    // Set the end of the source register.
    "add " END_R ", " SOURCE_R ", " END_R NL

    "vld1.f32 {" TEMP_Q "}, [" K_R "]" NL
    "ldr " K_R ", [" K_R "]" NL
    "vdup.f32 " K1_Q ", " TEMP_D00 NL
    "vdup.f32 " K2_Q ", " TEMP_D01 NL
    "vdup.f32 " K3_Q ", " TEMP_D10 NL
    "vdup.f32 " K4_Q ", " TEMP_D11 NL

    "vmov.i32 " BYTEMAX_Q ", #0xFF" NL
    "vcvt.f32.u32 " TEMP_Q ", " BYTEMAX_Q NL
    "vmul.f32 " K4_Q ", " K4_Q ", " TEMP_Q NL

    "mov " NEXTPIXEL_R ", #4" NL
    "cmp " K_R ", #0" NL
    "beq .arithmeticK1IsZero" NL

    "vrecpe.f32 " TEMP_Q ", " TEMP_Q NL
    "vmul.f32 " K1_Q ", " K1_Q ", " TEMP_Q NL

".arithmeticK1IsNonZero:" NL

    "vld1.u32 " PIXEL1_D00 ", [ " SOURCE_R "], " NEXTPIXEL_R NL
    "vld1.u32 " PIXEL2_D00 ", [" DEST_R "]" NL

    "vmovl.u8 " PIXEL1_Q ", " PIXEL1_D0 NL
    "vmovl.u16 " PIXEL1_Q ", " PIXEL1_D0 NL
    "vcvt.f32.u32 " PIXEL1_Q ", " PIXEL1_Q NL
    "vmovl.u8 " PIXEL2_Q ", " PIXEL2_D0 NL
    "vmovl.u16 " PIXEL2_Q ", " PIXEL2_D0 NL
    "vcvt.f32.u32 " PIXEL2_Q ", " PIXEL2_Q NL

    "vmul.f32 " TEMP_Q ", " PIXEL1_Q ", " PIXEL2_Q NL
    "vmul.f32 " TEMP_Q ", " TEMP_Q ", " K1_Q NL
    "vmla.f32 " TEMP_Q ", " PIXEL1_Q ", " K2_Q NL
    "vmla.f32 " TEMP_Q ", " PIXEL2_Q ", " K3_Q NL
    "vadd.f32 " TEMP_Q ", " K4_Q NL

    // Convert result to uint so negative values are converted to zero.
    "vcvt.u32.f32 " TEMP_Q ", " TEMP_Q NL
    "vmin.u32 " TEMP_Q ", " TEMP_Q ", " BYTEMAX_Q NL
    "vmovn.u32 " TEMP_D0 ", " TEMP_Q NL
    "vmovn.u16 " TEMP_D0 ", " TEMP_Q NL

    "vst1.u32 " TEMP_D00 ", [" DEST_R "], " NEXTPIXEL_R NL

    "cmp " SOURCE_R ", " END_R NL
    "bcc .arithmeticK1IsNonZero" NL
    "bx lr" NL

".arithmeticK1IsZero:" NL

    "vld1.u32 " PIXEL1_D00 ", [ " SOURCE_R "], " NEXTPIXEL_R NL
    "vld1.u32 " PIXEL2_D00 ", [" DEST_R "]" NL

    "vmovl.u8 " PIXEL1_Q ", " PIXEL1_D0 NL
    "vmovl.u16 " PIXEL1_Q ", " PIXEL1_D0 NL
    "vcvt.f32.u32 " PIXEL1_Q ", " PIXEL1_Q NL
    "vmovl.u8 " PIXEL2_Q ", " PIXEL2_D0 NL
    "vmovl.u16 " PIXEL2_Q ", " PIXEL2_D0 NL
    "vcvt.f32.u32 " PIXEL2_Q ", " PIXEL2_Q NL

    "vmul.f32 " TEMP_Q ", " PIXEL1_Q ", " K2_Q NL
    "vmla.f32 " TEMP_Q ", " PIXEL2_Q ", " K3_Q NL
    "vadd.f32 " TEMP_Q ", " K4_Q NL

    // Convert result to uint so negative values are converted to zero.
    "vcvt.u32.f32 " TEMP_Q ", " TEMP_Q NL
    "vmin.u32 " TEMP_Q ", " TEMP_Q ", " BYTEMAX_Q NL
    "vmovn.u32 " TEMP_D0 ", " TEMP_Q NL
    "vmovn.u16 " TEMP_D0 ", " TEMP_Q NL

    "vst1.u32 " TEMP_D00 ", [" DEST_R "], " NEXTPIXEL_R NL

    "cmp " SOURCE_R ", " END_R NL
    "bcc .arithmeticK1IsZero" NL
    "bx lr" NL
); // NOLINT

} // namespace WebCore

#endif // CPU(ARM_NEON) && COMPILER(GCC)

#endif // ENABLE(FILTERS)

