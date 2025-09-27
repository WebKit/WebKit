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
#include "WebAssemblySuspendingConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "JSCInlines.h"
#include "JSWebAssemblySuspendingFunction.h"

namespace JSC {

const ClassInfo WebAssemblySuspendingConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblySuspendingConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblySuspending);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblySuspending);

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblySuspending, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* wrappedFunction = nullptr;
    if (!(callFrame->argumentCount() == 1 && (wrappedFunction = jsDynamicCast<JSFunction*>(callFrame->argument(0).getObject())))) {
        throwVMError(globalObject, scope, "new WebAssembly.Suspending() expects a function"_s);
        return { };
    }

    auto* suspending = JSWebAssemblySuspendingFunction::create(vm, globalObject, wrappedFunction);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(suspending);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblySuspending, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Suspending"_s));
}

WebAssemblySuspendingConstructor* WebAssemblySuspendingConstructor::create(VM& vm, Structure* structure, WebAssemblySuspendingPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblySuspendingConstructor>(vm)) WebAssemblySuspendingConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblySuspendingConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblySuspendingConstructor::finishCreation(VM& vm, WebAssemblySuspendingPrototype* prototype)
{
    constexpr unsigned length = 1;
    Base::finishCreation(vm, length, "WebAssembly.Suspending"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblySuspendingConstructor::WebAssemblySuspendingConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblySuspending, constructJSWebAssemblySuspending)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
