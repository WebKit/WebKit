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
#include "WebAssemblyRuntimeErrorConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyRuntimeError.h"
#include "WebAssemblyRuntimeErrorPrototype.h"

#include "WebAssemblyRuntimeErrorConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyRuntimeErrorConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyRuntimeError, nullptr, CREATE_METHOD_TABLE(WebAssemblyRuntimeErrorConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyRuntimeError);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyRuntimeError);

/* Source for WebAssemblyRuntimeErrorConstructor.lut.h
 @begin constructorTableWebAssemblyRuntimeError
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyRuntimeError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyRuntimeErrorStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(globalObject, structure, message, options, nullptr, TypeNothing, ErrorType::Error, false)));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyRuntimeError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    Structure* errorStructure = globalObject->webAssemblyRuntimeErrorStructure();
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, options, nullptr, TypeNothing, ErrorType::Error, false));
}

WebAssemblyRuntimeErrorConstructor* WebAssemblyRuntimeErrorConstructor::create(VM& vm, Structure* structure, WebAssemblyRuntimeErrorPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyRuntimeErrorConstructor>(vm)) WebAssemblyRuntimeErrorConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyRuntimeErrorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyRuntimeErrorConstructor::finishCreation(VM& vm, WebAssemblyRuntimeErrorPrototype* prototype)
{
    Base::finishCreation(vm, 1, "RuntimeError"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyRuntimeErrorConstructor::WebAssemblyRuntimeErrorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyRuntimeError, constructJSWebAssemblyRuntimeError)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
