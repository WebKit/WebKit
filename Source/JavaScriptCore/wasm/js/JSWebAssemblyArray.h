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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include "WebAssemblyGCObjectBase.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class JSWebAssemblyArray final : public WebAssemblyGCObjectBase {
    friend class LLIntOffsetsExtractor;

public:
    using Base = WebAssemblyGCObjectBase;
    static constexpr bool needsDestruction = true;

    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyArraySpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    template <typename ElementType>
    static JSWebAssemblyArray* tryCreate(VM& vm, Structure* structure, Wasm::FieldType elementType, size_t size, FixedVector<ElementType>&& payload, RefPtr<const Wasm::RTT> rtt)
    {
        void* buffer = tryAllocateCell<JSWebAssemblyArray>(vm);
        if (UNLIKELY(!buffer))
            return nullptr;
        JSWebAssemblyArray* array = new (NotNull, buffer) JSWebAssemblyArray(vm, structure, elementType, size, WTFMove(payload), rtt);
        array->finishCreation(vm);
        return array;
    }

    DECLARE_VISIT_CHILDREN;

    Wasm::FieldType elementType() const { return m_elementType; }
    size_t size() const { return m_size; }

    uint8_t* data()
    {
        if (m_elementType.type.is<Wasm::PackedType>()) {
            switch (m_elementType.type.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                return m_payload8.mutableSpan().data();
            case Wasm::PackedType::I16:
                return reinterpret_cast<uint8_t*>(m_payload16.mutableSpan().data());
            }
        }
        ASSERT(m_elementType.type.is<Wasm::Type>());
        switch (m_elementType.type.as<Wasm::Type>().kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32:
            return reinterpret_cast<uint8_t*>(m_payload32.mutableSpan().data());
        case Wasm::TypeKind::V128:
            return reinterpret_cast<uint8_t*>(m_payload128.mutableSpan().data());
        default:
            return reinterpret_cast<uint8_t*>(m_payload64.mutableSpan().data());
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    };

    uint64_t* reftypeData()
    {
        RELEASE_ASSERT(m_elementType.type.unpacked().isRef() || m_elementType.type.unpacked().isRefNull());
        return m_payload64.mutableSpan().data();
    }

    uint64_t get(uint32_t index)
    {
        if (m_elementType.type.is<Wasm::PackedType>()) {
            switch (m_elementType.type.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                return static_cast<uint64_t>(m_payload8[index]);
            case Wasm::PackedType::I16:
                return static_cast<uint64_t>(m_payload16[index]);
            }
        }
        // m_element_type must be a type, so we can get its kind
        ASSERT(m_elementType.type.is<Wasm::Type>());
        switch (m_elementType.type.as<Wasm::Type>().kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32:
            return static_cast<uint64_t>(m_payload32[index]);
        case Wasm::TypeKind::V128:
            // V128 is not supported in LLInt.
            RELEASE_ASSERT_NOT_REACHED();
        default:
            return static_cast<uint64_t>(m_payload64[index]);
        }
    }

    void set(uint32_t index, uint64_t value)
    {
        if (m_elementType.type.is<Wasm::PackedType>()) {
            switch (m_elementType.type.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                m_payload8[index] = static_cast<uint8_t>(value);
                break;
            case Wasm::PackedType::I16:
                m_payload16[index] = static_cast<uint16_t>(value);
                break;
            }
            return;
        }

        ASSERT(m_elementType.type.is<Wasm::Type>());

        switch (m_elementType.type.as<Wasm::Type>().kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32:
            m_payload32[index] = static_cast<uint32_t>(value);
            break;
        case Wasm::TypeKind::I64:
        case Wasm::TypeKind::F64:
            m_payload64[index] = static_cast<uint64_t>(value);
            break;
        case Wasm::TypeKind::Externref:
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Ref:
        case Wasm::TypeKind::RefNull: {
            WriteBarrier<Unknown>* pointer = std::bit_cast<WriteBarrier<Unknown>*>(m_payload64.mutableSpan().data());
            pointer += index;
            pointer->set(vm(), this, JSValue::decode(static_cast<EncodedJSValue>(value)));
            break;
        }
        case Wasm::TypeKind::V128:
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    void set(uint32_t index, v128_t value)
    {
        ASSERT(m_elementType.type.is<Wasm::Type>());
        ASSERT(m_elementType.type.as<Wasm::Type>().kind == Wasm::TypeKind::V128);
        m_payload128[index] = value;
    }

    void fill(uint32_t, uint64_t, uint32_t);
    void fill(uint32_t, v128_t, uint32_t);
    void copy(JSWebAssemblyArray&, uint32_t, uint32_t, uint32_t);

    static constexpr ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(JSWebAssemblyArray, m_size); }
    static constexpr ptrdiff_t offsetOfPayload() { return OBJECT_OFFSETOF(JSWebAssemblyArray, m_payload8) + FixedVector<uint8_t>::offsetOfStorage(); }
    static ptrdiff_t offsetOfElements(Wasm::StorageType type)
    {
        if (type.is<Wasm::PackedType>()) {
            switch (type.as<Wasm::PackedType>()) {
            case Wasm::PackedType::I8:
                return FixedVector<uint8_t>::Storage::offsetOfData();
            case Wasm::PackedType::I16:
                return FixedVector<uint16_t>::Storage::offsetOfData();
            }
        }

        ASSERT(type.is<Wasm::Type>());

        switch (type.as<Wasm::Type>().kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32:
            return FixedVector<uint32_t>::Storage::offsetOfData();
        case Wasm::TypeKind::I64:
        case Wasm::TypeKind::F64:
        case Wasm::TypeKind::Ref:
        case Wasm::TypeKind::RefNull:
            return FixedVector<uint64_t>::Storage::offsetOfData();
        case Wasm::TypeKind::V128:
            return FixedVector<v128_t>::Storage::offsetOfData();
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        return 0;
    }

protected:
    JSWebAssemblyArray(VM&, Structure*, Wasm::FieldType, size_t, FixedVector<uint8_t>&&, RefPtr<const Wasm::RTT>);
    JSWebAssemblyArray(VM&, Structure*, Wasm::FieldType, size_t, FixedVector<uint16_t>&&, RefPtr<const Wasm::RTT>);
    JSWebAssemblyArray(VM&, Structure*, Wasm::FieldType, size_t, FixedVector<uint32_t>&&, RefPtr<const Wasm::RTT>);
    JSWebAssemblyArray(VM&, Structure*, Wasm::FieldType, size_t, FixedVector<uint64_t>&&, RefPtr<const Wasm::RTT>);
    JSWebAssemblyArray(VM&, Structure*, Wasm::FieldType, size_t, FixedVector<v128_t>&&, RefPtr<const Wasm::RTT>);
    ~JSWebAssemblyArray();

    DECLARE_DEFAULT_FINISH_CREATION;

    Wasm::FieldType m_elementType;
    size_t m_size;

    // A union is used here to ensure the underlying storage is aligned correctly.
    // The payload member used entirely depends on m_elementType, so no tag is required.
    union {
        FixedVector<uint8_t>  m_payload8;
        FixedVector<uint16_t> m_payload16;
        FixedVector<uint32_t> m_payload32;
        FixedVector<uint64_t> m_payload64;
        FixedVector<v128_t>   m_payload128;
    };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
