/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebAssemblyGlobalConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyGlobal.h"
#include "WebAssemblyGlobalPrototype.h"

#include "WebAssemblyGlobalConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyGlobalConstructor::s_info = { "Function", &Base::s_info, &constructorGlobalWebAssemblyGlobal, nullptr, CREATE_METHOD_TABLE(WebAssemblyGlobalConstructor) };

/* Source for WebAssemblyGlobalConstructor.lut.h
 @begin constructorGlobalWebAssemblyGlobal
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyGlobal(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* globalDescriptor;
    {
        JSValue argument = callFrame->argument(0);
        if (!argument.isObject())
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global expects its first argument to be an object"_s)));
        globalDescriptor = jsCast<JSObject*>(argument);
    }

    Wasm::GlobalInformation::Mutability mutability;
    {
        Identifier mutableIdent = Identifier::fromString(vm, "mutable");
        JSValue mutableValue = globalDescriptor->get(globalObject, mutableIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        bool mutableBoolean = mutableValue.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (mutableBoolean)
            mutability = Wasm::GlobalInformation::Mutable;
        else
            mutability = Wasm::GlobalInformation::Immutable;
    }

    Wasm::Type type;
    {
        Identifier valueIdent = Identifier::fromString(vm, "value");
        JSValue valueValue = globalDescriptor->get(globalObject, valueIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        String valueString = valueValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (valueString == "i32")
            type = Wasm::Type::I32;
        else if (valueString == "i64")
            type = Wasm::Type::I64;
        else if (valueString == "f32")
            type = Wasm::Type::F32;
        else if (valueString == "f64")
            type = Wasm::Type::F64;
        else
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global expects its 'value' field to be the string 'i32', 'i64', 'f32', or 'f64'"_s)));
    }

    uint64_t initialValue = 0;
    JSValue argument = callFrame->argument(1);
    if (!argument.isUndefined()) {
        switch (type) {
        case Wasm::Type::I32: {
            int32_t value = argument.toInt32(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
            break;
        }
        case Wasm::Type::I64: {
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Global does not accept i64 initial value"_s)));
        }
        case Wasm::Type::F32: {
            float value = argument.toFloat(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
            break;
        }
        case Wasm::Type::F64: {
            double value = argument.toNumber(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = bitwise_cast<uint64_t>(value);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    Ref<Wasm::Global> wasmGlobal = Wasm::Global::create(type, mutability, initialValue);
    RELEASE_AND_RETURN(throwScope, JSValue::encode(JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), WTFMove(wasmGlobal))));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyGlobal(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Global"));
}

WebAssemblyGlobalConstructor* WebAssemblyGlobalConstructor::create(VM& vm, Structure* structure, WebAssemblyGlobalPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyGlobalConstructor>(vm.heap)) WebAssemblyGlobalConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyGlobalConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyGlobalConstructor::finishCreation(VM& vm, WebAssemblyGlobalPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Global"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyGlobalConstructor::WebAssemblyGlobalConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyGlobal, constructJSWebAssemblyGlobal)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

