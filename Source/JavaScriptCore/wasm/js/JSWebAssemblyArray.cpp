/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "TypeError.h"
#include "WasmFormat.h"
#include "WasmTypeDefinition.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyArray::s_info = { "WebAssembly.Array"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyArray) };

Structure* JSWebAssemblyArray::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyGCObjectType, StructureFlags), info());
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, RefPtr<const Wasm::RTT>&& rtt)
    : Base(vm, structure, WTFMove(rtt))
    , m_elementType(elementType)
    , m_size(size)
{
    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            new (&m_payload8) FixedVector<uint8_t>(m_size);
            m_payload8.fill(0);
            return;
        case Wasm::PackedType::I16:
            new (&m_payload16) FixedVector<uint16_t>(m_size);
            m_payload16.fill(0);
            return;
        }
        return;
    }

    switch (m_elementType.type.as<Wasm::Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        new (&m_payload32) FixedVector<uint32_t>(m_size);
        m_payload32.fill(0);
        return;
    case Wasm::TypeKind::V128:
        new (&m_payload128) FixedVector<v128_t>(m_size);
        m_payload128.fill(v128_t { });
        return;
    default:
        new (&m_payload64) FixedVector<uint64_t>(m_size);
        if (elementsAreRefTypes())
            m_payload64.fill(JSValue::encode(jsNull()));
        else
            m_payload64.fill(0);
        return;
    }
}

JSWebAssemblyArray::~JSWebAssemblyArray()
{
    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            m_payload8.~FixedVector<uint8_t>();
            break;
        case Wasm::PackedType::I16:
            m_payload16.~FixedVector<uint16_t>();
            break;
        }
        return;
    }

    switch (m_elementType.type.as<Wasm::Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        m_payload32.~FixedVector<uint32_t>();
        break;
    case Wasm::TypeKind::V128:
        m_payload128.~FixedVector<v128_t>();
        break;
    default:
        m_payload64.~FixedVector<uint64_t>();
        break;
    }
}

void JSWebAssemblyArray::fill(uint32_t offset, uint64_t value, uint32_t size)
{
    // Handle ref types separately to ensure write barriers are in effect.
    if (elementsAreRefTypes()) {
        for (size_t i = 0; i < size; i++)
            set(offset + i, value);
        return;
    }

    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            memsetSpan(m_payload8.mutableSpan().subspan(offset, size), static_cast<uint8_t>(value));
            return;
        case Wasm::PackedType::I16:
            std::fill(m_payload16.begin() + offset, m_payload16.begin() + offset + size, static_cast<uint16_t>(value));
            return;
        }
    }

    switch (m_elementType.type.as<Wasm::Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        std::fill(m_payload32.begin() + offset, m_payload32.begin() + offset + size, static_cast<uint32_t>(value));
        return;
    case Wasm::TypeKind::V128:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    default:
        std::fill(m_payload64.begin() + offset, m_payload64.begin() + offset + size, value);
        return;
    }
}

void JSWebAssemblyArray::fill(uint32_t offset, v128_t value, uint32_t size)
{
    ASSERT(m_elementType.type.unpacked().isV128());
    std::fill(m_payload128.begin() + offset, m_payload128.begin() + offset + size, value);
}

void JSWebAssemblyArray::copy(JSWebAssemblyArray& dst, uint32_t dstOffset, uint32_t srcOffset, uint32_t size)
{
    // Handle ref types separately to ensure write barriers are in effect.
    if (elementsAreRefTypes()) {
        gcSafeMemmove(dst.m_payload64.mutableSpan().subspan(dstOffset).data(), m_payload64.span().subspan(srcOffset).data(), size * sizeof(JSValue));
        vm().writeBarrier(this);
        return;
    }

    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            memmove(dst.m_payload8.mutableSpan().subspan(dstOffset).data(), m_payload8.span().subspan(srcOffset).data(), size * sizeof(uint8_t));
            return;
        case Wasm::PackedType::I16:
            memmove(dst.m_payload16.mutableSpan().subspan(dstOffset).data(), m_payload16.span().subspan(srcOffset).data(), size * sizeof(uint16_t));
            return;
        }
    }

    switch (m_elementType.type.as<Wasm::Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        memmove(dst.m_payload32.mutableSpan().subspan(dstOffset).data(), m_payload32.span().subspan(srcOffset).data(), size * sizeof(uint32_t));
        return;
    case Wasm::TypeKind::V128:
        memmove(dst.m_payload128.mutableSpan().subspan(dstOffset).data(), m_payload128.span().subspan(srcOffset).data(), size * sizeof(v128_t));
        return;
    default:
        memmove(dst.m_payload64.mutableSpan().subspan(dstOffset).data(), m_payload64.span().subspan(srcOffset).data(), size * sizeof(uint64_t));
        return;
    }
}

void JSWebAssemblyArray::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyArray*>(cell)->JSWebAssemblyArray::~JSWebAssemblyArray();
}

template<typename Visitor>
void JSWebAssemblyArray::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyArray* thisObject = jsCast<JSWebAssemblyArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    if (thisObject->elementsAreRefTypes())
        visitor.appendValues(std::bit_cast<WriteBarrier<Unknown>*>(thisObject->reftypeData()), thisObject->size());
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyArray);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
