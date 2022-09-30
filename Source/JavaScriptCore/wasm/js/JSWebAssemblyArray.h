/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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

#include "JSObject.h"
#include "WasmOps.h"

namespace JSC {

class JSWebAssemblyArray final : public JSNonFinalObject {
    friend class LLIntOffsetsExtractor;

public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;

    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyArraySpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSWebAssemblyArray* create(VM& vm, Structure* structure, Wasm::Type elementType, size_t size, FixedVector<uint32_t>&& payload)
    {
        JSWebAssemblyArray* array = new (NotNull, allocateCell<JSWebAssemblyArray>(vm)) JSWebAssemblyArray(vm, structure, elementType, size, WTFMove(payload));
        array->finishCreation(vm);
        return array;
    }

    static JSWebAssemblyArray* create(VM& vm, Structure* structure, Wasm::Type elementType, size_t size, FixedVector<uint64_t>&& payload)
    {
        JSWebAssemblyArray* array = new (NotNull, allocateCell<JSWebAssemblyArray>(vm)) JSWebAssemblyArray(vm, structure, elementType, size, WTFMove(payload));
        array->finishCreation(vm);
        return array;
    }

    DECLARE_VISIT_CHILDREN;

    Wasm::Type elementType() const { return m_elementType; }
    size_t size() const { return m_size; }

    EncodedJSValue get(uint32_t index)
    {
        switch (m_elementType.kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32:
            return static_cast<EncodedJSValue>(m_payload32[index]);
        default:
            return static_cast<EncodedJSValue>(m_payload64[index]);
        }
    }

    void set(VM& vm, uint32_t index, EncodedJSValue value)
    {
        switch (m_elementType.kind) {
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
            WriteBarrier<Unknown>* pointer = bitwise_cast<WriteBarrier<Unknown>*>(m_payload64.data());
            pointer += index;
            pointer->set(vm, this, JSValue::decode(value));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(JSWebAssemblyArray, m_size); }
    static ptrdiff_t offsetOfPayload()
    {
        ASSERT(OBJECT_OFFSETOF(JSWebAssemblyArray, m_payload32) == OBJECT_OFFSETOF(JSWebAssemblyArray, m_payload64));
        return OBJECT_OFFSETOF(JSWebAssemblyArray, m_payload32);
    }

protected:
    JSWebAssemblyArray(VM&, Structure*, Wasm::Type, size_t, FixedVector<uint32_t>&&);
    JSWebAssemblyArray(VM&, Structure*, Wasm::Type, size_t, FixedVector<uint64_t>&&);
    ~JSWebAssemblyArray();

    void finishCreation(VM&);

    Wasm::Type m_elementType;
    size_t m_size;

    // A union is used here to ensure the underlying storage is aligned correctly.
    // The payload member used entirely depends on m_elementType, so no tag is required.
    union {
        FixedVector<uint32_t> m_payload32;
        FixedVector<uint64_t> m_payload64;
    };
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
