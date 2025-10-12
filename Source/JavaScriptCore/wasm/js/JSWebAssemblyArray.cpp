/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyArray.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyInstance.h"
#include "TypeError.h"
#include "WasmFormat.h"
#include "WasmTypeDefinition.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyArray::s_info = { "WebAssembly.Array"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyArray) };

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, WebAssemblyGCStructure* structure, unsigned size)
    : Base(vm, structure)
    , m_size(size)
{
    if (elementsAreRefTypes())
        std::fill(span<uint64_t>().begin(), span<uint64_t>().end(), JSValue::encode(jsNull()));
    else
        zeroSpan(bytes());
}

JSWebAssemblyArray* JSWebAssemblyArray::tryCreate(VM& vm, WebAssemblyGCStructure* structure, unsigned size)
{
    Wasm::FieldType fieldType = elementType(structure);
    std::optional<unsigned> allocationSize = allocationSizeInBytes(fieldType, size);
    if (!allocationSize) [[unlikely]]
        return nullptr;

    auto* array = new (NotNull, allocateCell<JSWebAssemblyArray>(vm, allocationSize.value())) JSWebAssemblyArray(vm, structure, size);
    array->finishCreation(vm);
    return array;
}

void JSWebAssemblyArray::fill(VM& vm, uint32_t offset, uint64_t value, uint32_t size)
{
    // Handle ref types separately to ensure write barriers are in effect.
    if (elementsAreRefTypes()) {
        // FIXME: We should have a GCSafeMemfill.
        for (size_t i = 0; i < size; i++)
            set(vm, offset + i, value);
        return;
    }

    visitSpanNonVector([&](auto span) ALWAYS_INLINE_LAMBDA {
        span = span.subspan(offset, size);
        std::fill(span.begin(), span.end(), value);
    });
}

void JSWebAssemblyArray::fill(VM&, uint32_t offset, v128_t value, uint32_t size)
{
    ASSERT(elementType().type.unpacked().isV128());
    auto payload = span<v128_t>().subspan(offset, size);
    std::fill(payload.begin(), payload.end(), value);
}

void JSWebAssemblyArray::copy(VM& vm, JSWebAssemblyArray& dst, uint32_t dstOffset, uint32_t srcOffset, uint32_t size)
{
    // Handle ref types separately to ensure write barriers are in effect.
    if (elementsAreRefTypes()) {
        gcSafeMemmove(dst.span<uint64_t>().subspan(dstOffset).data(), span<uint64_t>().subspan(srcOffset).data(), size * sizeof(JSValue));
        vm.writeBarrier(&dst);
        return;
    }

    visitSpan([&]<typename T>(std::span<T> srcSpan) ALWAYS_INLINE_LAMBDA {
        srcSpan = srcSpan.subspan(srcOffset, size);
        auto dstSpan = dst.span<T>().subspan(dstOffset, size);
        memmoveSpan(dstSpan, srcSpan);
    });
}

template<typename Visitor>
void JSWebAssemblyArray::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyArray* thisObject = jsCast<JSWebAssemblyArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    if (thisObject->elementsAreRefTypes()) {
        auto span = thisObject->refTypeSpan();
#if ASSERT_ENABLED
        if (!thisObject->m_isUnpopulated) {
            for (uint64_t value : span)
                validateWasmValue(value, thisObject->elementType().type.unpacked());
        }
#endif
        visitor.appendValues(std::bit_cast<WriteBarrier<Unknown>*>(span.data()), span.size());
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyArray);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
