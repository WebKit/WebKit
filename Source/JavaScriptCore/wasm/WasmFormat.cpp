/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "WasmFormat.h"

#if ENABLE(WEBASSEMBLY)

#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyStruct.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/FastMalloc.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

constexpr uintptr_t NullWasmCallee = 0;

Segment::Ptr Segment::create(std::optional<I32InitExpr> offset, uint32_t sizeInBytes, Kind kind)
{
    CheckedUint32 totalBytesChecked = sizeInBytes;
    totalBytesChecked += sizeof(Segment);
    if (totalBytesChecked.hasOverflowed())
        return Ptr(nullptr, &Segment::destroy);
    auto allocated = tryFastCalloc(totalBytesChecked, 1);
    Segment* segment;
    if (!allocated.getValue(segment))
        return Ptr(nullptr, &Segment::destroy);
    ASSERT(kind == Kind::Passive || !!offset);
    segment->kind = kind;
    segment->offsetIfActive = WTFMove(offset);
    segment->sizeInBytes = sizeInBytes;
    return Ptr(segment, &Segment::destroy);
}

void Segment::destroy(Segment *segment)
{
    fastFree(segment);
}

String makeString(const Name& characters)
{
    return WTF::makeString(characters);
}

#if ASSERT_ENABLED
void validateWasmValue(uint64_t wasmValue, Type expectedType)
{
    // FIXME: Add more validations
    auto value = std::bit_cast<JSValue>(wasmValue);
    if (isRefType(expectedType)) {
        if (value.isNull()) {
            ASSERT(expectedType.isNullable());
            return;
        }

        if (!isExternref(expectedType) && !isI31ref(expectedType))
            ASSERT(value.isCell());

        if (isStructref(expectedType))
            ASSERT(jsDynamicCast<JSWebAssemblyStruct*>(value));

        if (isArrayref(expectedType))
            ASSERT(jsDynamicCast<JSWebAssemblyArray*>(value));

        if (isRefWithTypeIndex(expectedType)) {
            RefPtr<const RTT> expectedRTT = Wasm::TypeInformation::getCanonicalRTT(expectedType.index);
            if (expectedRTT->kind() == RTTKind::Function) {
                ASSERT(jsDynamicCast<JSFunction*>(value));
                return;
            }
            auto objectPtr = jsCast<WebAssemblyGCObjectBase*>(value);
            auto objectRTT = objectPtr->rtt();
            ASSERT(objectRTT->isSubRTT(*expectedRTT.get()));
        }
    }
}
#endif

} } // namespace JSC::Wasm

namespace WTF {

void printInternal(PrintStream& out, JSC::Wasm::TableElementType type)
{
    switch (type) {
    case JSC::Wasm::TableElementType::Externref:
        out.print("Externref");
        break;
    case JSC::Wasm::TableElementType::Funcref:
        out.print("Funcref");
        break;
    }
}

} // namespace WTF

#endif // ENABLE(WEBASSEMBLY)
