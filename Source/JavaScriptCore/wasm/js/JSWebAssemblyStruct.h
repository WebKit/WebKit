/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyGCObjectBase.h"
#include <wtf/Ref.h>
#include <wtf/TrailingArray.h>

namespace JSC {

class JSWebAssemblyInstance;

class alignas(sizeof(uint64_t)) JSWebAssemblyStruct final : public WebAssemblyGCObjectBase, private TrailingArray<JSWebAssemblyStruct, uint8_t> {
public:
    using Base = WebAssemblyGCObjectBase;
    using TrailingArrayType = TrailingArray<JSWebAssemblyStruct, uint8_t>;
    friend TrailingArrayType;
    static_assert(StructureFlags == WebAssemblyGCObjectBase::StructureFlags, "WebAssemblyGCObjectBase must have the same StructureFlags as us");

    template<typename CellType, SubspaceAccess mode>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.heap.variableSizedCellSpace;
    }

    DECLARE_INFO;

    static inline WebAssemblyGCStructure* createStructure(VM&, JSGlobalObject*, Ref<const Wasm::TypeDefinition>&&, Ref<const Wasm::RTT>&&);
    static JSWebAssemblyStruct* tryCreate(VM&, WebAssemblyGCStructure*);
    static JSWebAssemblyStruct* create(VM&, WebAssemblyGCStructure*);

    DECLARE_VISIT_CHILDREN;

    uint64_t get(uint32_t) const;
    void set(uint32_t, uint64_t);
    void set(uint32_t, v128_t);
    const Wasm::TypeDefinition& typeDefinition() const { return gcStructure()->typeDefinition(); }
    const Wasm::StructType& structType() const { return *typeDefinition().as<Wasm::StructType>(); }
    Wasm::FieldType fieldType(uint32_t fieldIndex) const { return structType().field(fieldIndex); }

    uint8_t* fieldPointer(uint32_t fieldIndex) { return &at(structType().offsetOfFieldInPayload(fieldIndex)); }
    const uint8_t* fieldPointer(uint32_t fieldIndex) const { return const_cast<JSWebAssemblyStruct*>(this)->fieldPointer(fieldIndex); }

    using TrailingArrayType::offsetOfData;
    using TrailingArrayType::offsetOfSize;
    using TrailingArrayType::allocationSize;

protected:
    JSWebAssemblyStruct(VM&, WebAssemblyGCStructure*);
    DECLARE_DEFAULT_FINISH_CREATION;
};

WebAssemblyGCStructure* JSWebAssemblyStruct::createStructure(VM& vm, JSGlobalObject* globalObject, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
{
    RELEASE_ASSERT(rtt->kind() == Wasm::RTTKind::Struct);
    RELEASE_ASSERT(type->is<Wasm::StructType>());
    return WebAssemblyGCStructure::create(vm, globalObject, TypeInfo(WebAssemblyGCObjectType, StructureFlags), info(), WTFMove(type), WTFMove(rtt));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
