/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SIMDInfo.h"

namespace WTF {

void printInternal(PrintStream& out, JSC::SIMDLane lane)
{
    switch (lane) {
    case JSC::SIMDLane::i8x16:
        out.print("i8x16");
        break;
    case JSC::SIMDLane::i16x8:
        out.print("i16x8");
        break;
    case JSC::SIMDLane::i32x4:
        out.print("i32x4");
        break;
    case JSC::SIMDLane::f32x4:
        out.print("f32x4");
        break;
    case JSC::SIMDLane::i64x2:
        out.print("i64x2");
        break;
    case JSC::SIMDLane::f64x2:
        out.print("f64x2");
        break;
    case JSC::SIMDLane::v128:
        out.print("v128");
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void printInternal(PrintStream& out, JSC::SIMDSignMode mode)
{
    switch (mode) {
    case JSC::SIMDSignMode::None:
        out.print("SignMode::None");
        break;
    case JSC::SIMDSignMode::Signed:
        out.print("SignMode::Signed");
        break;
    case JSC::SIMDSignMode::Unsigned:
        out.print("SignMode::Unsigned");
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void printInternal(PrintStream& out, JSC::v128_t v)
{
    out.print("{ ", v.u32x4[0], ", ", v.u32x4[1], ", ", v.u32x4[2], ", ", v.u32x4[3], " }");
}

} // namespace WTF
