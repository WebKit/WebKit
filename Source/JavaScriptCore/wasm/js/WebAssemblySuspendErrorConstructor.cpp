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
#include "WebAssemblySuspendErrorConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblySuspendError.h"
#include "WebAssemblySuspendErrorPrototype.h"

namespace JSC {

const ClassInfo WebAssemblySuspendErrorConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblySuspendErrorConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblySuspendError);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblySuspendError);

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblySuspendError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblySuspendErrorStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(globalObject, structure, message, options, nullptr, TypeNothing, ErrorType::Error, false)));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblySuspendError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    Structure* errorStructure = globalObject->webAssemblySuspendErrorStructure();
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, options, nullptr, TypeNothing, ErrorType::Error, false));
}

WebAssemblySuspendErrorConstructor* WebAssemblySuspendErrorConstructor::create(VM& vm, Structure* structure, WebAssemblySuspendErrorPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblySuspendErrorConstructor>(vm)) WebAssemblySuspendErrorConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblySuspendErrorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblySuspendErrorConstructor::finishCreation(VM& vm, WebAssemblySuspendErrorPrototype* prototype)
{
    Base::finishCreation(vm, 1, "SuspendError"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblySuspendErrorConstructor::WebAssemblySuspendErrorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblySuspendError, constructJSWebAssemblySuspendError)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
