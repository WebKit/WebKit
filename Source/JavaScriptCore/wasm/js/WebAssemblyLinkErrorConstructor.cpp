/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WebAssemblyLinkErrorConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyLinkError.h"
#include "WebAssemblyLinkErrorPrototype.h"

#include "WebAssemblyLinkErrorConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyLinkErrorConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyLinkError, nullptr, CREATE_METHOD_TABLE(WebAssemblyLinkErrorConstructor) };

/* Source for WebAssemblyLinkErrorConstructor.lut.h
 @begin constructorTableWebAssemblyLinkError
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyLinkError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->webAssemblyLinkErrorStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->webAssemblyLinkErrorStructure());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(JSWebAssemblyLinkError::create(globalObject, vm, structure, message)));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyLinkError(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSValue message = callFrame->argument(0);
    Structure* errorStructure = globalObject->webAssemblyLinkErrorStructure();
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, nullptr, TypeNothing, false));
}

WebAssemblyLinkErrorConstructor* WebAssemblyLinkErrorConstructor::create(VM& vm, Structure* structure, WebAssemblyLinkErrorPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyLinkErrorConstructor>(vm.heap)) WebAssemblyLinkErrorConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyLinkErrorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyLinkErrorConstructor::finishCreation(VM& vm, WebAssemblyLinkErrorPrototype* prototype)
{
    Base::finishCreation(vm, 1, "LinkError"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyLinkErrorConstructor::WebAssemblyLinkErrorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyLinkError, constructJSWebAssemblyLinkError)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
