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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <wtf/PrintStream.h>

enum class SIMDLaneOperation : uint8_t {
    Const,
    ExtractLane,
    Load,
    Store,
};

#define FOR_EACH_WASM_EXT_SIMD_OP(macro) \
    macro(V128Load,                 0x00, SIMDLaneOperation::Load,                      SIMDLane::v128,     SIMDSignMode::None) \
    macro(V128Store,                0x0b, SIMDLaneOperation::Store,                     SIMDLane::v128,     SIMDSignMode::None) \
    macro(V128Const,                0x0c, SIMDLaneOperation::Const,                     SIMDLane::v128,     SIMDSignMode::None) \
    macro(I8x16ExtractLaneS,        0x15, SIMDLaneOperation::ExtractLane,               SIMDLane::i8x16,    SIMDSignMode::Signed) \
    macro(I8x16ExtractLaneU,        0x16, SIMDLaneOperation::ExtractLane,               SIMDLane::i8x16,    SIMDSignMode::Unsigned) \
    macro(I16x8ExtractLaneS,        0x18, SIMDLaneOperation::ExtractLane,               SIMDLane::i16x8,    SIMDSignMode::Signed) \
    macro(I16x8ExtractLaneU,        0x19, SIMDLaneOperation::ExtractLane,               SIMDLane::i16x8,    SIMDSignMode::Unsigned) \
    macro(I32x4ExtractLane,         0x1b, SIMDLaneOperation::ExtractLane,               SIMDLane::i32x4,    SIMDSignMode::None) \
    macro(I64x2ExtractLane,         0x1d, SIMDLaneOperation::ExtractLane,               SIMDLane::i64x2,    SIMDSignMode::None) \
    macro(F32x4ExtractLane,         0x1f, SIMDLaneOperation::ExtractLane,               SIMDLane::f32x4,    SIMDSignMode::None) \
    macro(F64x2ExtractLane,         0x21, SIMDLaneOperation::ExtractLane,               SIMDLane::f64x2,    SIMDSignMode::None) \

static void dumpSIMDLaneOperation(PrintStream& out, SIMDLaneOperation op)
{
    switch (op) {
    case SIMDLaneOperation::Const: out.print("Const"); break;
    case SIMDLaneOperation::ExtractLane: out.print("ExtractLane"); break;
    case SIMDLaneOperation::Load: out.print("Load"); break;
    case SIMDLaneOperation::Store: out.print("Store"); break;
    }
}
MAKE_PRINT_ADAPTOR(SIMDLaneOperationDump, SIMDLaneOperation, dumpSIMDLaneOperation);

#endif // ENABLE(WEBASSEMBLY)
