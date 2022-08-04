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

enum class SimdLaneOperation : uint8_t {
    Const,
    ExtractLane,
    Load,
    Store,
};

#define FOR_EACH_WASM_EXT_SIMD_OP(macro) \
    macro(V128Load,                 0x00, SimdLaneOperation::Load,                      SimdLane::v128,     SimdSignMode::None) \
    macro(V128Store,                0x0b, SimdLaneOperation::Store,                     SimdLane::v128,     SimdSignMode::None) \
    macro(V128Const,                0x0c, SimdLaneOperation::Const,                     SimdLane::v128,     SimdSignMode::None) \
    macro(I8x16ExtractLaneS,        0x15, SimdLaneOperation::ExtractLane,               SimdLane::i8x16,    SimdSignMode::Signed) \
    macro(I8x16ExtractLaneU,        0x16, SimdLaneOperation::ExtractLane,               SimdLane::i8x16,    SimdSignMode::Unsigned) \
    macro(I16x8ExtractLaneS,        0x18, SimdLaneOperation::ExtractLane,               SimdLane::i16x8,    SimdSignMode::Signed) \
    macro(I16x8ExtractLaneU,        0x19, SimdLaneOperation::ExtractLane,               SimdLane::i16x8,    SimdSignMode::Unsigned) \
    macro(I32x4ExtractLane,         0x1b, SimdLaneOperation::ExtractLane,               SimdLane::i32x4,    SimdSignMode::None) \
    macro(I64x2ExtractLane,         0x1d, SimdLaneOperation::ExtractLane,               SimdLane::i64x2,    SimdSignMode::None) \
    macro(F32x4ExtractLane,         0x1f, SimdLaneOperation::ExtractLane,               SimdLane::f32x4,    SimdSignMode::None) \
    macro(F64x2ExtractLane,         0x21, SimdLaneOperation::ExtractLane,               SimdLane::f64x2,    SimdSignMode::None) \

static void dumpSimdLaneOperation(PrintStream& out, SimdLaneOperation op)
{
    switch (op) {
    case SimdLaneOperation::Const: out.print("Const"); break;
    case SimdLaneOperation::ExtractLane: out.print("ExtractLane"); break;
    case SimdLaneOperation::Load: out.print("Load"); break;
    case SimdLaneOperation::Store: out.print("Store"); break;
    }
}
MAKE_PRINT_ADAPTOR(SimdLaneOperationDump, SimdLaneOperation, dumpSimdLaneOperation);

#endif // ENABLE(WEBASSEMBLY)
