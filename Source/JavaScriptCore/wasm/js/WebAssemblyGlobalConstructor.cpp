/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyRuntimeError.h"
#include "WasmFormat.h"
#include "WebAssemblyGlobalPrototype.h"

#include "WebAssemblyGlobalConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyGlobalConstructor::s_info = { "Function"_s, &Base::s_info, &constructorGlobalWebAssemblyGlobal, nullptr, CREATE_METHOD_TABLE(WebAssemblyGlobalConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyGlobal);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyGlobal);

/* Source for WebAssemblyGlobalConstructor.lut.h
 @begin constructorGlobalWebAssemblyGlobal
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyGlobal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* webAssemblyGlobalStructure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyGlobalStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(throwScope, { });

    JSObject* globalDescriptor;
    {
        JSValue argument = callFrame->argument(0);
        if (!argument.isObject())
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Global expects its first argument to be an object"_s);
        globalDescriptor = jsCast<JSObject*>(argument);
    }

    Wasm::Mutability mutability;
    {
        Identifier mutableIdent = Identifier::fromString(vm, "mutable"_s);
        JSValue mutableValue = globalDescriptor->get(globalObject, mutableIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        bool mutableBoolean = mutableValue.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (mutableBoolean)
            mutability = Wasm::Mutable;
        else
            mutability = Wasm::Immutable;
    }

    Wasm::Type type;
    {
        Identifier valueIdent = Identifier::fromString(vm, "value"_s);
        JSValue valueValue = globalDescriptor->get(globalObject, valueIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        String valueString = valueValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (valueString == "i32"_s)
            type = Wasm::Types::I32;
        else if (valueString == "i64"_s)
            type = Wasm::Types::I64;
        else if (valueString == "f32"_s)
            type = Wasm::Types::F32;
        else if (valueString == "f64"_s)
            type = Wasm::Types::F64;
        else if (valueString == "anyfunc"_s || valueString == "funcref"_s)
            type = Wasm::funcrefType();
        else if (valueString == "externref"_s)
            type = Wasm::externrefType();
        else
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Global expects its 'value' field to be the string 'i32', 'i64', 'f32', 'f64', 'anyfunc', 'funcref', or 'externref'"_s);
    }

    uint64_t initialValue = 0;
    JSValue argument = callFrame->argument(1);
    switch (type.kind) {
    case Wasm::TypeKind::I32: {
        if (!argument.isUndefined()) {
            int32_t value = argument.toInt32(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
        }
        break;
    }
    case Wasm::TypeKind::I64: {
        if (!argument.isUndefined()) {
            int64_t value = argument.toBigInt64(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = static_cast<uint64_t>(value);
        }
        break;
    }
    case Wasm::TypeKind::F32: {
        if (!argument.isUndefined()) {
            float value = argument.toFloat(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
        }
        break;
    }
    case Wasm::TypeKind::F64: {
        if (!argument.isUndefined()) {
            double value = argument.toNumber(globalObject);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            initialValue = bitwise_cast<uint64_t>(value);
        }
        break;
    }
    case Wasm::TypeKind::V128:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    default: {
        if (Wasm::isFuncref(type)) {
            if (argument.isUndefined())
                argument = defaultValueForReferenceType(type);
            if (!isWebAssemblyHostFunction(argument) && !argument.isNull())
                return throwVMTypeError(globalObject, throwScope, "Funcref must be an exported wasm function"_s);
            initialValue = JSValue::encode(argument);
        } else if (Wasm::isExternref(type)) {
            if (argument.isUndefined())
                argument = defaultValueForReferenceType(type);
            initialValue = JSValue::encode(argument);
        } else if (Wasm::isExternref(type))
            return throwVMTypeError(globalObject, throwScope, "I31ref import from JS currently unsupported"_s);
        else
            RELEASE_ASSERT_NOT_REACHED();
    }
    }

    Ref<Wasm::Global> wasmGlobal = Wasm::Global::create(type, mutability, initialValue);
    JSWebAssemblyGlobal* jsWebAssemblyGlobal = JSWebAssemblyGlobal::tryCreate(globalObject, vm, webAssemblyGlobalStructure, WTFMove(wasmGlobal));
    RETURN_IF_EXCEPTION(throwScope, { });
    ensureStillAliveHere(initialValue); // Ensure this is kept alive while creating JSWebAssemblyGlobal.
    return JSValue::encode(jsWebAssemblyGlobal);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyGlobal, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Global"));
}

WebAssemblyGlobalConstructor* WebAssemblyGlobalConstructor::create(VM& vm, Structure* structure, WebAssemblyGlobalPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyGlobalConstructor>(vm)) WebAssemblyGlobalConstructor(vm, structure);
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

