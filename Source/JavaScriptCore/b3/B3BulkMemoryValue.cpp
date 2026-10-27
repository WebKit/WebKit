/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "B3BulkMemoryValue.h"

#if ENABLE(B3_JIT)

#include "B3ProcedureInlines.h"
#include "B3SIMDValue.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

BulkMemoryValue::~BulkMemoryValue() = default;

Width BulkMemoryValue::widestFillStoreWidth(const Procedure& proc) const
{
    // Only 64-bit stores are known to be single-copy atomic per word on every target.
    if (fillWidth() == Width128 || (!isGCSafe() && proc.usesSIMD()))
        return Width128;
    return Width64;
}

Value* BulkMemoryValue::splatFillPattern(Procedure& proc, Width storeWidth, NOESCAPE const ScopedLambda<void(Value*)>& insert) const
{
    Width elementWidth = fillWidth();
    ASSERT(storeWidth >= elementWidth);
    Value* pattern = child(1);
    if (storeWidth == elementWidth)
        return pattern;

    auto add = [&](Value* value) {
        insert(value);
        return value;
    };

    ASSERT(elementWidth <= Width64);
    uint64_t elementMask = mask(elementWidth);
    uint64_t splatMultiplier = UINT64_MAX / elementMask;

    if (pattern->hasInt()) {
        uint64_t splat = (static_cast<uint64_t>(pattern->asInt()) & elementMask) * splatMultiplier;
        if (storeWidth == Width128)
            return add(proc.addConstant(origin(), V128, v128_t { splat, splat }));
        return add(proc.addConstant(origin(), storeWidth == Width64 ? Int64 : Int32, splat));
    }

    if (storeWidth == Width128) {
        SIMDLane lane = SIMDLane::i64x2;
        switch (elementWidth) {
        case Width8:
            lane = SIMDLane::i8x16;
            break;
        case Width16:
            lane = SIMDLane::i16x8;
            break;
        case Width32:
            lane = SIMDLane::i32x4;
            break;
        default:
            break;
        }
        return add(proc.add<SIMDValue>(origin(), VectorSplat, V128, lane, SIMDSignMode::None, pattern));
    }

    ASSERT(pattern->type() == Int32);
    Value* element = pattern;
    if (elementWidth < Width32)
        element = add(proc.add<Value>(BitAnd, origin(), pattern, add(proc.addIntConstant(origin(), Int32, elementMask))));
    if (storeWidth == Width64)
        return add(proc.add<Value>(Mul, origin(), add(proc.add<Value>(ZExt32, origin(), element)), add(proc.addIntConstant(origin(), Int64, splatMultiplier))));
    return add(proc.add<Value>(Mul, origin(), element, add(proc.addIntConstant(origin(), Int32, static_cast<int32_t>(splatMultiplier)))));
}

void BulkMemoryValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma, "readRange = ", readRange(), comma, "writeRange = ", writeRange(), comma, "gcSafe = ", isGCSafe());
}

BulkMemoryValue::BulkMemoryValue(Kind kind, Origin origin, Value* child0, Value* child1, Value* child2, bool isGCSafe)
    : Value(CheckedOpcode, kind, Void, Three, origin, child0, child1, child2)
    , m_isGCSafe(isGCSafe)
{
    ASSERT(!isGCSafe || kind == MemoryMove || kind == MemoryFill64);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

