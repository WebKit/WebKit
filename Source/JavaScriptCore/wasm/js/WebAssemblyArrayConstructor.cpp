/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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
#include "WebAssemblyArrayConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyArray.h"
#include "WebAssemblyArrayPrototype.h"

#include "WebAssemblyArrayConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyArrayConstructor::s_info = { "Function"_s, &Base::s_info, &constructorTableWebAssemblyArray, nullptr, CREATE_METHOD_TABLE(WebAssemblyArrayConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyArray);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyArray);

/* Source for WebAssemblyArrayConstructor.lut.h
 @begin constructorTableWebAssemblyArray
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyArray, (JSGlobalObject* globalObject, CallFrame*))
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return throwVMTypeError(globalObject, scope, "WebAssembly.Array constructor should not be exposed currently"_s);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyArray, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Array"));
}

WebAssemblyArrayConstructor* WebAssemblyArrayConstructor::create(VM& vm, Structure* structure, WebAssemblyArrayPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyArrayConstructor>(vm)) WebAssemblyArrayConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyArrayConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyArrayConstructor::finishCreation(VM& vm, WebAssemblyArrayPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Array"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

WebAssemblyArrayConstructor::WebAssemblyArrayConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyArray, constructJSWebAssemblyArray)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
