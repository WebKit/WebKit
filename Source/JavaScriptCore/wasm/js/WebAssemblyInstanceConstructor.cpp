/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "WebAssemblyInstanceConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyInstanceConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyInstance, nullptr, CREATE_METHOD_TABLE(WebAssemblyInstanceConstructor) };

/* Source for WebAssemblyInstanceConstructor.lut.h
 @begin constructorTableWebAssemblyInstance
 @end
 */

using Wasm::Plan;

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyInstance(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // If moduleObject is not a WebAssembly.Module instance, a TypeError is thrown.
    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, scope, createTypeError(globalObject, "first argument to WebAssembly.Instance must be a WebAssembly.Module"_s, defaultSourceAppender, runtimeTypeForValue(vm, callFrame->argument(0)))));

    // If the importObject parameter is not undefined and Type(importObject) is not Object, a TypeError is thrown.
    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (!importArgument.isUndefined() && !importObject)
        return JSValue::encode(throwException(globalObject, scope, createTypeError(globalObject, "second argument to WebAssembly.Instance must be undefined or an Object"_s, defaultSourceAppender, runtimeTypeForValue(vm, importArgument))));

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* instanceStructure = newTarget == callFrame->jsCallee()
        ? globalObject->webAssemblyInstanceStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->webAssemblyInstanceStructure());
    RETURN_IF_EXCEPTION(scope, { });

    JSWebAssemblyInstance* instance = JSWebAssemblyInstance::tryCreate(vm, globalObject, JSWebAssemblyInstance::createPrivateModuleKey(), module, importObject, instanceStructure, Ref<Wasm::Module>(module->module()), Wasm::CreationMode::FromJS);
    RETURN_IF_EXCEPTION(scope, { });

    instance->finalizeCreation(vm, globalObject, module->module().compileSync(&vm.wasmContext, instance->memoryMode()), importObject, Wasm::CreationMode::FromJS);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(instance);
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyInstance(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Instance"));
}

WebAssemblyInstanceConstructor* WebAssemblyInstanceConstructor::create(VM& vm, Structure* structure, WebAssemblyInstancePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyInstanceConstructor>(vm.heap)) WebAssemblyInstanceConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyInstanceConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyInstanceConstructor::finishCreation(VM& vm, WebAssemblyInstancePrototype* prototype)
{
    Base::finishCreation(vm, "Instance"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

WebAssemblyInstanceConstructor::WebAssemblyInstanceConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyInstance, constructJSWebAssemblyInstance)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

