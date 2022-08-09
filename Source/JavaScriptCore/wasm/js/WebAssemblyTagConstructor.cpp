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
#include "WebAssemblyTagConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "IteratorOperations.h"
#include "JSGlobalObject.h"
#include "JSWebAssemblyTag.h"
#include "WebAssemblyTagPrototype.h"

namespace JSC {

const ClassInfo WebAssemblyTagConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyTagConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyTag);
static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyTag);

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyTag, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (callFrame->argumentCount() < 1)
        return throwVMTypeError(globalObject, scope, "WebAssembly.Tag constructor expects the tag type as the first argument."_s);

    JSValue tagTypeValue = callFrame->argument(0);
    JSValue signatureObject = tagTypeValue.get(globalObject, Identifier::fromString(vm, "parameters"_s));
    RETURN_IF_EXCEPTION(scope, { });

    if (!signatureObject.isObject())
        return throwVMTypeError(globalObject, scope, "WebAssembly.Tag constructor expects a tag type with the 'parameters' property."_s);

    Vector<Wasm::Type> parameters;
    forEachInIterable(globalObject, signatureObject, [&] (auto& vm, auto* globalObject, JSValue nextType) -> void {
        auto scope = DECLARE_THROW_SCOPE(vm);

        Wasm::Type type;
        String valueString = nextType.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (valueString == "i32"_s)
            type = Wasm::Types::I32;
        else if (valueString == "i64"_s)
            type = Wasm::Types::I64;
        else if (valueString == "f32"_s)
            type = Wasm::Types::F32;
        else if (valueString == "f64"_s)
            type = Wasm::Types::F64;
        else if (valueString == "funcref"_s || valueString == "anyfunc"_s)
            type = Wasm::Types::Funcref;
        else if (valueString == "externref"_s)
            type = Wasm::Types::Externref;
        else {
            throwTypeError(globalObject, scope, "WebAssembly.Tag constructor expects the 'parameters' field of the first argument to be a sequence of WebAssembly value types."_s);
            return;
        }

        parameters.append(type);
    });
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<Wasm::TypeDefinition> typeDefinition = Wasm::TypeInformation::typeDefinitionForFunction({ }, parameters);
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyTagStructure, asObject(callFrame->newTarget()), callFrame->jsCallee());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSWebAssemblyTag::create(vm, globalObject, structure, Wasm::Tag::create(*typeDefinition).get())));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyTag, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Tag"));
}

WebAssemblyTagConstructor* WebAssemblyTagConstructor::create(VM& vm, Structure* structure, WebAssemblyTagPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyTagConstructor>(vm)) WebAssemblyTagConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyTagConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyTagConstructor::finishCreation(VM& vm, WebAssemblyTagPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Tag"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyTagConstructor::WebAssemblyTagConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyTag, constructJSWebAssemblyTag)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
