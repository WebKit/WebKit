/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyStruct.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "WasmFormat.h"
#include "WasmModuleInformation.h"
#include <wtf/MallocPtr.h>

namespace JSC {

const ClassInfo JSWebAssemblyStruct::s_info = { "WebAssembly.Struct"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyStruct) };

Structure* JSWebAssemblyStruct::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyGCObjectType, StructureFlags), info());
}

JSWebAssemblyStruct::JSWebAssemblyStruct(VM& vm, Structure* structure, Ref<const Wasm::TypeDefinition>&& type, RefPtr<const Wasm::RTT> rtt)
    : Base(vm, structure, rtt)
    , m_type(WTFMove(type))
    , m_payload(structType()->instancePayloadSize(), 0)
{
}

JSWebAssemblyStruct* JSWebAssemblyStruct::create(JSGlobalObject* globalObject, Structure* structure, JSWebAssemblyInstance* instance, uint32_t typeIndex, RefPtr<const Wasm::RTT> rtt)
{
    VM& vm = globalObject->vm();

    Ref<const Wasm::TypeDefinition> type = instance->instance().module().moduleInformation().typeSignatures[typeIndex]->expand();

    auto* structValue = new (NotNull, allocateCell<JSWebAssemblyStruct>(vm)) JSWebAssemblyStruct(vm, structure, Ref { type }, rtt);
    structValue->finishCreation(vm);
    return structValue;
}

const uint8_t* JSWebAssemblyStruct::fieldPointer(uint32_t fieldIndex) const
{
    return m_payload.data() + structType()->offsetOfFieldInternal(fieldIndex);
}

uint8_t* JSWebAssemblyStruct::fieldPointer(uint32_t fieldIndex)
{
    return const_cast<uint8_t*>(const_cast<const JSWebAssemblyStruct*>(this)->fieldPointer(fieldIndex));
}

uint64_t JSWebAssemblyStruct::get(uint32_t fieldIndex) const
{
    using Wasm::TypeKind;

    const uint8_t* targetPointer = fieldPointer(fieldIndex);

    if (fieldType(fieldIndex).type.is<Wasm::PackedType>()) {
        switch (fieldType(fieldIndex).type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            return *bitwise_cast<uint8_t*>(targetPointer);
        case Wasm::PackedType::I16:
            return *bitwise_cast<uint16_t*>(targetPointer);
        }
    }
    ASSERT(fieldType(fieldIndex).type.is<Wasm::Type>());

    switch (fieldType(fieldIndex).type.as<Wasm::Type>().kind) {
    case TypeKind::I32:
    case TypeKind::F32:
        return *bitwise_cast<uint32_t*>(targetPointer);
    case TypeKind::I64:
    case TypeKind::F64:
        return *bitwise_cast<const uint64_t*>(targetPointer);
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        return JSValue::encode(bitwise_cast<WriteBarrierBase<Unknown>*>(targetPointer)->get());
    case TypeKind::V128:
        // V128 is not supported in LLInt.
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

void JSWebAssemblyStruct::set(uint32_t fieldIndex, uint64_t argument)
{
    using Wasm::TypeKind;

    uint8_t* targetPointer = fieldPointer(fieldIndex);

    if (fieldType(fieldIndex).type.is<Wasm::PackedType>()) {
        switch (fieldType(fieldIndex).type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            *bitwise_cast<uint8_t*>(targetPointer) = static_cast<uint8_t>(argument);
            return;
        case Wasm::PackedType::I16:
            *bitwise_cast<uint16_t*>(targetPointer) = static_cast<uint16_t>(argument);
            return;
        }
    }
    ASSERT(fieldType(fieldIndex).type.is<Wasm::Type>());

    switch (fieldType(fieldIndex).type.as<Wasm::Type>().kind) {
    case TypeKind::I32:
    case TypeKind::F32: {
        *bitwise_cast<uint32_t*>(targetPointer) = static_cast<uint32_t>(argument);
        return;
    }
    case TypeKind::I64:
    case TypeKind::F64: {
        *bitwise_cast<uint64_t*>(targetPointer) = argument;
        return;
    }
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        bitwise_cast<WriteBarrierBase<Unknown>*>(targetPointer)->set(vm(), this, JSValue::decode(static_cast<EncodedJSValue>(argument)));
        return;
    }
    case TypeKind::V128:
    case TypeKind::Func:
    case TypeKind::Struct:
    case TypeKind::Array:
    case TypeKind::Void:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Rec:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
    case TypeKind::I31ref: {
        break;
    }
    }

    ASSERT_NOT_REACHED();
}

void JSWebAssemblyStruct::set(uint32_t fieldIndex, v128_t argument)
{
    uint8_t* targetPointer = fieldPointer(fieldIndex);
    ASSERT(fieldType(fieldIndex).type.is<Wasm::Type>());
    ASSERT(fieldType(fieldIndex).type.as<Wasm::Type>().kind == Wasm::TypeKind::V128);
    *bitwise_cast<v128_t*>(targetPointer) = argument;
}

template<typename Visitor>
void JSWebAssemblyStruct::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* wasmStruct = jsCast<JSWebAssemblyStruct*>(cell);
    for (unsigned i = 0; i < wasmStruct->structType()->fieldCount(); ++i) {
        if (isRefType(wasmStruct->fieldType(i).type))
            visitor.append(*bitwise_cast<WriteBarrier<Unknown>*>(wasmStruct->fieldPointer(i)));
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyStruct);

void JSWebAssemblyStruct::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyStruct*>(cell)->JSWebAssemblyStruct::~JSWebAssemblyStruct();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
