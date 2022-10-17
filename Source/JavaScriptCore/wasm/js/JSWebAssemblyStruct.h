/*
 * Copyright (C) 2022-2022 Apple Inc. All rights reserved.
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
#include <wtf/Ref.h>

namespace JSC {

class JSWebAssemblyInstance;

class JSWebAssemblyStruct final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;

    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyStructSpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(FinalObjectType, StructureFlags), info());
    }

    static JSWebAssemblyStruct* tryCreate(JSGlobalObject*, Structure*, JSWebAssemblyInstance*, uint32_t);

    DECLARE_VISIT_CHILDREN;

    uint64_t get(uint32_t) const;
    void set(JSGlobalObject*, uint32_t, JSValue);
    const Wasm::StructType* structType() const { return m_type->as<Wasm::StructType>(); }
    Wasm::FieldType fieldType(uint32_t fieldIndex) const { return structType()->field(fieldIndex); }

    static ptrdiff_t offsetOfPayload() { return OBJECT_OFFSETOF(JSWebAssemblyStruct, m_payload) + FixedVector<uint8_t>::offsetOfStorage(); }

    const uint8_t* fieldPointer(uint32_t fieldIndex) const;
    uint8_t* fieldPointer(uint32_t fieldIndex);

protected:
    JSWebAssemblyStruct(VM&, Structure*, Ref<Wasm::TypeDefinition>&&);
    void finishCreation(VM&);

    // FIXME: It is possible to encode the type information in the structure field of Wasm.Struct and remove this field.
    // https://bugs.webkit.org/show_bug.cgi?id=244838
    Ref<Wasm::TypeDefinition> m_type;

    FixedVector<uint8_t> m_payload;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
