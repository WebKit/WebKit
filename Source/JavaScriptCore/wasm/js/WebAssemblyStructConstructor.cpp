/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebAssemblyStructConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyStruct.h"
#include "WebAssemblyStructPrototype.h"

#include "WebAssemblyStructConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyStructConstructor::s_info = { "Function"_s, &Base::s_info, &constructorTableWebAssemblyStruct, nullptr, CREATE_METHOD_TABLE(WebAssemblyStructConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyStruct);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyStruct);

/* Source for WebAssemblyStructConstructor.lut.h
 @begin constructorTableWebAssemblyStruct
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyStruct, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    UNUSED_PARAM(callFrame);
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "WebAssembly.Struct is not accessible from JS"_s);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyStruct, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "WebAssembly.Struct is not accessible from JS"_s);
}

WebAssemblyStructConstructor* WebAssemblyStructConstructor::create(VM& vm, Structure* structure, WebAssemblyStructPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyStructConstructor>(vm)) WebAssemblyStructConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyStructConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyStructConstructor::finishCreation(VM& vm, WebAssemblyStructPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Struct"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyStructConstructor::WebAssemblyStructConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyStruct, constructJSWebAssemblyStruct)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
