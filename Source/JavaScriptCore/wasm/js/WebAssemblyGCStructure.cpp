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
#include "WebAssemblyGCStructure.h"

#include "JSCInlines.h"
#include "WasmFormat.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC {

static inline Wasm::TypeHash typeHash(const Wasm::TypeDefinition& typeDef)
{
    return Wasm::TypeHash { const_cast<Wasm::TypeDefinition&>(typeDef) };
}

WebAssemblyGCStructureTypeDependencies::WebAssemblyGCStructureTypeDependencies(Ref<const Wasm::TypeDefinition>&& unexpandedType)
{
    WorkList work;
    SUPPRESS_UNCHECKED_ARG work.append(unexpandedType->expand());
    while (!work.isEmpty())
        SUPPRESS_UNCHECKED_ARG process(work.takeLast(), work);
    m_typeDefinitions.add(typeHash(unexpandedType));
}

void WebAssemblyGCStructureTypeDependencies::process(const Wasm::TypeDefinition& typeDef, WorkList& work)
{
    if (m_typeDefinitions.contains(typeHash(typeDef)))
        return;
    m_typeDefinitions.add(typeHash(typeDef));
    if (typeDef.is<Wasm::StructType>()) {
        SUPPRESS_UNCHECKED_LOCAL auto* structType = typeDef.as<Wasm::StructType>();
        for (unsigned i = 0; i < structType->fieldCount(); ++i)
            process(structType->field(i), work);
    } else if (typeDef.is<Wasm::ArrayType>())
        process(typeDef.as<Wasm::ArrayType>()->elementType(), work);
}

void WebAssemblyGCStructureTypeDependencies::process(Wasm::FieldType fieldType, WorkList& work)
{
    if (fieldType.type.is<Wasm::Type>()) {
        Wasm::Type type = fieldType.type.as<Wasm::Type>();
        if (isRefWithTypeIndex(type)) {
            SUPPRESS_UNCHECKED_LOCAL const auto& typeDef = Wasm::TypeInformation::get(type.index);
            work.append(typeDef);
        }
    }
}

WebAssemblyGCStructure::WebAssemblyGCStructure(VM& vm, JSGlobalObject* globalObject, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
    : Structure(vm, StructureVariant::WebAssemblyGC, globalObject, typeInfo, classInfo)
    , m_rtt(WTF::move(rtt))
    , m_type(WTF::move(type))
    , m_typeDependencies(WebAssemblyGCStructureTypeDependencies { WTF::move(unexpandedType) })
{
}

WebAssemblyGCStructure::WebAssemblyGCStructure(VM& vm, WebAssemblyGCStructure* previous)
    : Structure(vm, StructureVariant::WebAssemblyGC, previous)
    , m_rtt(previous->m_rtt)
    , m_type(previous->m_type)
    , m_typeDependencies(previous->m_typeDependencies)
{
}


WebAssemblyGCStructure* WebAssemblyGCStructure::create(VM& vm, JSGlobalObject* globalObject, const TypeInfo& typeInfo, const ClassInfo* classInfo, Ref<const Wasm::TypeDefinition>&& unexpandedType, Ref<const Wasm::TypeDefinition>&& type, Ref<const Wasm::RTT>&& rtt)
{
    ASSERT(vm.structureStructure);
    WebAssemblyGCStructure* newStructure = new (NotNull, allocateCell<WebAssemblyGCStructure>(vm)) WebAssemblyGCStructure(vm, globalObject, typeInfo, classInfo, WTF::move(unexpandedType), WTF::move(type), WTF::move(rtt));
    newStructure->finishCreation(vm);
    ASSERT(newStructure->type() == StructureType);
    return newStructure;
}

} // namespace JSC

#endif
