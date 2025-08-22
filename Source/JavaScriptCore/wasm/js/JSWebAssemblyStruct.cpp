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

#include "config.h"
#include "JSWebAssemblyStruct.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "WasmFormat.h"
#include "WasmModuleInformation.h"
#include <wtf/ScopedPrintStream.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo JSWebAssemblyStruct::s_info = { "WebAssembly.Struct"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyStruct) };

JSWebAssemblyStruct::JSWebAssemblyStruct(VM& vm, WebAssemblyGCStructure* structure)
    : Base(vm, structure)
    , TrailingArrayType(structure->typeDefinition().as<Wasm::StructType>()->instancePayloadSize())
{
    // Make sure if another object is allocated while initializing the struct we don't crash the GC. It's *VERY* important this happens before finishCreation since that executes our mutator fence.
    memsetSpan(span(), 0);
}

JSWebAssemblyStruct* JSWebAssemblyStruct::tryCreate(VM& vm, WebAssemblyGCStructure* structure)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* structType = structure->typeDefinition().as<Wasm::StructType>();
    auto* cell = tryAllocateCell<JSWebAssemblyStruct>(vm, TrailingArrayType::allocationSize(structType->instancePayloadSize()));
    if (!cell) [[unlikely]]
        return nullptr;

    auto* structValue = new (NotNull, cell) JSWebAssemblyStruct(vm, structure);
    structValue->finishCreation(vm);
    return structValue;
}

JSWebAssemblyStruct* JSWebAssemblyStruct::create(VM& vm, WebAssemblyGCStructure* structure)
{
    auto* result = JSWebAssemblyStruct::tryCreate(vm, structure);
    RELEASE_ASSERT(result);
    return result;
}

uint64_t JSWebAssemblyStruct::get(uint32_t fieldIndex) const
{
    using Wasm::TypeKind;

    const uint8_t* targetPointer = fieldPointer(fieldIndex);

    if (fieldType(fieldIndex).type.is<Wasm::PackedType>()) {
        switch (fieldType(fieldIndex).type.as<Wasm::PackedType>()) {
        case Wasm::PackedType::I8:
            return *std::bit_cast<uint8_t*>(targetPointer);
        case Wasm::PackedType::I16:
            return *std::bit_cast<uint16_t*>(targetPointer);
        }
    }
    ASSERT(fieldType(fieldIndex).type.is<Wasm::Type>());

    switch (fieldType(fieldIndex).type.as<Wasm::Type>().kind) {
    case TypeKind::I32:
    case TypeKind::F32:
        return *std::bit_cast<uint32_t*>(targetPointer);
    case TypeKind::I64:
    case TypeKind::F64:
        return *std::bit_cast<const uint64_t*>(targetPointer);
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull:
        return JSValue::encode(std::bit_cast<WriteBarrierBase<Unknown>*>(targetPointer)->get());
    case TypeKind::V128:
        // V128 is not supported in IPInt.
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
            *std::bit_cast<uint8_t*>(targetPointer) = static_cast<uint8_t>(argument);
            return;
        case Wasm::PackedType::I16:
            *std::bit_cast<uint16_t*>(targetPointer) = static_cast<uint16_t>(argument);
            return;
        }
    }
    ASSERT(fieldType(fieldIndex).type.is<Wasm::Type>());

    switch (fieldType(fieldIndex).type.as<Wasm::Type>().kind) {
    case TypeKind::I32:
    case TypeKind::F32: {
        *std::bit_cast<uint32_t*>(targetPointer) = static_cast<uint32_t>(argument);
        return;
    }
    case TypeKind::I64:
    case TypeKind::F64: {
        *std::bit_cast<uint64_t*>(targetPointer) = argument;
        return;
    }
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        std::bit_cast<WriteBarrierBase<Unknown>*>(targetPointer)->set(vm(), this, JSValue::decode(static_cast<EncodedJSValue>(argument)));
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
    case TypeKind::Exnref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
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
    *std::bit_cast<v128_t*>(targetPointer) = argument;
}

template<typename Visitor>
void JSWebAssemblyStruct::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* wasmStruct = jsCast<JSWebAssemblyStruct*>(cell);
    if (!wasmStruct->structType().hasRefFieldTypes()) {
#if ASSERT_ENABLED
        for (unsigned i = 0; i < wasmStruct->structType().fieldCount(); ++i)
            ASSERT(!isRefType(wasmStruct->fieldType(i).type));
#endif
        return;
    }

    for (unsigned i = 0; i < wasmStruct->structType().fieldCount(); ++i) {
        auto fieldType = wasmStruct->fieldType(i).type;
        if (isRefType(fieldType)) {
            auto* writeBarrier = std::bit_cast<WriteBarrier<Unknown>*>(wasmStruct->fieldPointer(i));
            validateWasmValue(JSValue::encode(writeBarrier->get()), fieldType.unpacked());
            visitor.append(*std::bit_cast<WriteBarrier<Unknown>*>(wasmStruct->fieldPointer(i)));
        }
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyStruct);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
