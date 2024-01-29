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

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "TypeError.h"
#include "WasmFormat.h"
#include "WasmTypeDefinition.h"

namespace JSC {

const ClassInfo JSWebAssemblyArray::s_info = { "WebAssembly.Array"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyArray) };

Structure* JSWebAssemblyArray::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyGCObjectType, StructureFlags), info());
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<uint8_t>&& payload, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload8(WTFMove(payload))
{
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<uint16_t>&& payload, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload16(WTFMove(payload))
{
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<uint32_t>&& payload, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload32(WTFMove(payload))
{
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<uint64_t>&& payload, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload64(WTFMove(payload))
{
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<v128_t>&& payload, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload128(WTFMove(payload))
{
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
    if (isRefType(m_elementType.type.unpacked()) && !isI31ref(m_elementType.type.unpacked())) {
        for (size_t i = 0; i < size; i++)
            set(offset + i, value);
        return;
    }

    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            memset(m_payload8.data() + offset, static_cast<uint8_t>(value), size);
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
    if (isRefType(m_elementType.type.unpacked()) && !isI31ref(m_elementType.type.unpacked())) {
        // If the ranges overlap then copy to a tmp buffer first.
        if (&dst == this && dstOffset <= srcOffset + size && srcOffset <= dstOffset + size) {
            FixedVector<uint64_t> tmpCopy(size);
            std::copy(m_payload64.begin() + srcOffset, m_payload64.begin() + srcOffset + size, tmpCopy.data());
            for (size_t i = 0; i < size; i++)
                dst.set(dstOffset + i, tmpCopy[i]);
        } else {
            for (size_t i = 0; i < size; i++)
                dst.set(dstOffset + i, m_payload64[srcOffset + i]);
        }
        return;
    }

    if (m_elementType.type.is<Wasm::PackedType>()) {
        switch (m_elementType.type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            memmove(dst.m_payload8.data() + dstOffset, m_payload8.data() + srcOffset, size);
            return;
        case Wasm::PackedType::I16:
            std::copy(m_payload16.begin() + srcOffset, m_payload16.begin() + srcOffset + size, dst.m_payload16.begin() + dstOffset);
            return;
        }
    }

    switch (m_elementType.type.as<Wasm::Type>().kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        std::copy(m_payload32.begin() + srcOffset, m_payload32.begin() + srcOffset + size, dst.m_payload32.begin() + dstOffset);
        return;
    case Wasm::TypeKind::V128:
        std::copy(m_payload128.begin() + srcOffset, m_payload128.begin() + srcOffset + size, dst.m_payload128.begin() + dstOffset);
        return;
    default:
        std::copy(m_payload64.begin() + srcOffset, m_payload64.begin() + srcOffset + size, dst.m_payload64.begin() + dstOffset);
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

    if (isRefType(thisObject->elementType().type)) {
        for (unsigned i = 0; i < thisObject->size(); ++i)
            visitor.append(bitwise_cast<WriteBarrier<Unknown>>(thisObject->get(i)));
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyArray);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
