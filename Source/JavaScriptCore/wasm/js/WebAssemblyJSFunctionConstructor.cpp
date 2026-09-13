/*
 * Copyright (C) 2026 Sergey Rubanov <chi187@gmail.com>. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAssemblyJSFunctionConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "CallData.h"
#include "Error.h"
#include "Identifier.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "WasmTypeDefinition.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyFunctionBase.h"
#include "WebAssemblyJSFunctionPrototype.h"
#include "WebAssemblyWrapperFunction.h"

namespace JSC {

const ClassInfo WebAssemblyJSFunctionConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyJSFunctionConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructWebAssemblyJSFunction);
static JSC_DECLARE_HOST_FUNCTION(callWebAssemblyJSFunctionConstructor);

static bool parseValueType(JSGlobalObject* globalObject, JSValue value, Wasm::Type& type)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String valueString = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (valueString == "i32"_s)
        type = Wasm::Types::I32;
    else if (valueString == "i64"_s)
        type = Wasm::Types::I64;
    else if (valueString == "f32"_s)
        type = Wasm::Types::F32;
    else if (valueString == "f64"_s)
        type = Wasm::Types::F64;
    else if (valueString == "v128"_s)
        type = Wasm::Types::V128;
    else if (valueString == "funcref"_s || valueString == "anyfunc"_s)
        type = Wasm::funcrefType();
    else if (valueString == "externref"_s)
        type = Wasm::externrefType();
    else {
        throwTypeError(globalObject, scope, "WebAssembly.Function expects a valid value type"_s);
        return false;
    }
    return true;
}

static void parseTypeList(JSGlobalObject* globalObject, JSValue list, Vector<Wasm::Type, 16>& types)
{
    forEachInIterable(globalObject, list, [&](VM&, JSGlobalObject* globalObject, JSValue nextType) {
        Wasm::Type type;
        if (!parseValueType(globalObject, nextType, type))
            return;
        types.append(type);
    });
}

JSC_DEFINE_HOST_FUNCTION(constructWebAssemblyJSFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() < 2)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function constructor expects 2 arguments"_s);

    JSValue typeValue = callFrame->uncheckedArgument(0);
    if (!typeValue.isObject())
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function constructor expects a function type object"_s);

    JSObject* typeObject = asObject(typeValue);
    JSValue parametersValue = typeObject->get(globalObject, Identifier::fromString(vm, "parameters"_s));
    RETURN_IF_EXCEPTION(scope, { });
    if (parametersValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function constructor expects a 'parameters' property"_s);

    JSValue resultsValue = typeObject->get(globalObject, Identifier::fromString(vm, "results"_s));
    RETURN_IF_EXCEPTION(scope, { });
    if (resultsValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function constructor expects a 'results' property"_s);

    Vector<Wasm::Type, 16> parameters;
    parseTypeList(globalObject, parametersValue, parameters);
    RETURN_IF_EXCEPTION(scope, { });

    Vector<Wasm::Type, 16> results;
    parseTypeList(globalObject, resultsValue, results);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue functionValue = callFrame->uncheckedArgument(1);
    auto callData = JSC::getCallData(functionValue);
    if (callData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Function constructor expects a function"_s);

    Ref<const Wasm::RTT> rtt = Wasm::TypeInformation::rttForFunction(results, parameters);
    WebAssemblyFunction* wasmFunction = nullptr;
    WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
    if (isWebAssemblyHostFunction(functionValue, wasmFunction, wasmWrapperFunction)) {
        WebAssemblyFunctionBase* existing = wasmFunction ? static_cast<WebAssemblyFunctionBase*>(wasmFunction) : wasmWrapperFunction;
        if (!existing->rtt() || !existing->rtt()->isSubRTT(rtt.get()))
            return throwVMTypeError(globalObject, scope, "WebAssembly.Function signature does not match the provided WebAssembly function"_s);
        return JSValue::encode(existing);
    }

    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyJSFunctionStructure, asObject(callFrame->newTarget()), callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(WebAssemblyWrapperFunction::createFromJSFunction(vm, globalObject, structure, asObject(functionValue), WTF::move(rtt)));
}

JSC_DEFINE_HOST_FUNCTION(callWebAssemblyJSFunctionConstructor, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Function"_s));
}

WebAssemblyJSFunctionConstructor* WebAssemblyJSFunctionConstructor::create(VM& vm, Structure* structure, WebAssemblyJSFunctionPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyJSFunctionConstructor>(vm)) WebAssemblyJSFunctionConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyJSFunctionConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyJSFunctionConstructor::finishCreation(VM& vm, WebAssemblyJSFunctionPrototype* prototype)
{
    constexpr unsigned length = 2;
    Base::finishCreation(vm, length, "Function"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyJSFunctionConstructor::WebAssemblyJSFunctionConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callWebAssemblyJSFunctionConstructor, constructWebAssemblyJSFunction)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
