/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebAssemblyExceptionConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "IteratorOperations.h"
#include "JITOpaqueByproducts.h"
#include "JSCInlines.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyTag.h"
#include "WebAssemblyExceptionPrototype.h"

#include "WebAssemblyExceptionConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyExceptionConstructor::s_info = { "Function"_s, &Base::s_info, &constructorTableWebAssemblyException, nullptr, CREATE_METHOD_TABLE(WebAssemblyExceptionConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyException);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyException);

/* Source for WebAssemblyExceptionConstructor.lut.h
 @begin constructorTableWebAssemblyException
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyException, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue tagValue = callFrame->argument(0);
    JSValue tagParameters = callFrame->argument(1);

    auto tag = jsDynamicCast<JSWebAssemblyTag*>(tagValue);
    if (!tag)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Exception constructor expects the first argument to be a WebAssembly.Tag"_s);

    const auto& tagFunctionType = tag->type();
    MarkedArgumentBuffer values;
    values.ensureCapacity(tagFunctionType.argumentCount());
    forEachInIterable(globalObject, tagParameters, [&] (VM&, JSGlobalObject*, JSValue nextValue) {
        values.append(nextValue);
        if (UNLIKELY(values.hasOverflowed()))
            throwOutOfMemoryError(globalObject, scope);
    });
    RETURN_IF_EXCEPTION(scope, { });

    if (values.size() != tagFunctionType.argumentCount())
        return throwVMTypeError(globalObject, scope, "WebAssembly.Exception constructor expects the number of paremeters in WebAssembly.Tag to match the tags parameter count."_s);

    // Any GC'd values in here will be marked by the MarkedArugementBuffer until stored in the Exception.
    FixedVector<uint64_t> payload(values.size());
    for (unsigned i = 0; i < values.size(); ++i) {
        payload[i] = fromJSValue(globalObject, tagFunctionType.argumentType(i), values.at(i));
        RETURN_IF_EXCEPTION(scope, { });
    }

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyExceptionStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(JSWebAssemblyException::create(vm, structure, tag->tag(), WTFMove(payload))));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyException, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Exception"));
}

WebAssemblyExceptionConstructor* WebAssemblyExceptionConstructor::create(VM& vm, Structure* structure, WebAssemblyExceptionPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyExceptionConstructor>(vm)) WebAssemblyExceptionConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyExceptionConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyExceptionConstructor::finishCreation(VM& vm, WebAssemblyExceptionPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Exception"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyExceptionConstructor::WebAssemblyExceptionConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyException, constructJSWebAssemblyException)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
