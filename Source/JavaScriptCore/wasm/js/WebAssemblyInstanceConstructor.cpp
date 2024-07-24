/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "WebAssemblyInstanceConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "WebAssemblyInstancePrototype.h"

namespace JSC {

const ClassInfo WebAssemblyInstanceConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyInstanceConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyInstance);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyInstance);

using Wasm::Plan;

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyInstance, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // If moduleObject is not a WebAssembly.Module instance, a TypeError is thrown.
    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, scope, createTypeError(globalObject, "first argument to WebAssembly.Instance must be a WebAssembly.Module"_s, defaultSourceAppender, runtimeTypeForValue(callFrame->argument(0)))));

    // If the importObject parameter is not undefined and Type(importObject) is not Object, a TypeError is thrown.
    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject)
        return JSValue::encode(throwException(globalObject, scope, createTypeError(globalObject, "second argument to WebAssembly.Instance must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(importArgument))));

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* instanceStructure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyInstanceStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::tryCreate(vm, instanceStructure, globalObject, JSWebAssemblyInstance::createPrivateModuleKey(), module, importObject, Wasm::CreationMode::FromJS);
    RETURN_IF_EXCEPTION(scope, { });

    instance->initializeImports(globalObject, importObject, Wasm::CreationMode::FromJS);
    RETURN_IF_EXCEPTION(scope, { });

    instance->finalizeCreation(vm, globalObject, module->module().compileSync(vm, instance->memoryMode()), Wasm::CreationMode::FromJS);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(instance);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyInstance, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Instance"_s));
}

WebAssemblyInstanceConstructor* WebAssemblyInstanceConstructor::create(VM& vm, Structure* structure, WebAssemblyInstancePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyInstanceConstructor>(vm)) WebAssemblyInstanceConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyInstanceConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyInstanceConstructor::finishCreation(VM& vm, WebAssemblyInstancePrototype* prototype)
{
    Base::finishCreation(vm, 1, "Instance"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyInstanceConstructor::WebAssemblyInstanceConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyInstance, constructJSWebAssemblyInstance)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

