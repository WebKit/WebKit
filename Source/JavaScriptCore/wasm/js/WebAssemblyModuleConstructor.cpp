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
#include "WebAssemblyModuleConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "ArrayBuffer.h"
#include "ButterflyInlines.h"
#include "JSArrayBuffer.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
#include "WasmModuleInformation.h"
#include "WebAssemblyModulePrototype.h"
#include <wtf/StdLibExtras.h>

namespace JSC {
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleCustomSections(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleImports(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL webAssemblyModuleExports(JSGlobalObject*, CallFrame*);
}

#include "WebAssemblyModuleConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyModuleConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyModule, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleConstructor) };

/* Source for WebAssemblyModuleConstructor.lut.h
 @begin constructorTableWebAssemblyModule
 customSections webAssemblyModuleCustomSections Function 2
 imports        webAssemblyModuleImports        Function 1
 exports        webAssemblyModuleExports        Function 1
 @end
 */

EncodedJSValue JSC_HOST_CALL webAssemblyModuleCustomSections(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame->argumentCount() < 2))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, callFrame->uncheckedArgument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.customSections called with non WebAssembly.Module argument"_s)));

    const String sectionNameString = callFrame->uncheckedArgument(1).getString(globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& customSections = module->moduleInformation().customSections;
    for (const Wasm::CustomSection& section : customSections) {
        if (String::fromUTF8(section.name) == sectionNameString) {
            auto buffer = ArrayBuffer::tryCreate(section.payload.data(), section.payload.size());
            if (!buffer)
                return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));

            result->push(globalObject, JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), WTFMove(buffer)));
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL webAssemblyModuleImports(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.imports called with non WebAssembly.Module argument"_s)));

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& imports = module->moduleInformation().imports;
    if (imports.size()) {
        Identifier module = Identifier::fromString(vm, "module");
        Identifier name = Identifier::fromString(vm, "name");
        Identifier kind = Identifier::fromString(vm, "kind");
        for (const Wasm::Import& imp : imports) {
            JSObject* obj = constructEmptyObject(globalObject);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, module, jsString(vm, String::fromUTF8(imp.module)));
            obj->putDirect(vm, name, jsString(vm, String::fromUTF8(imp.field)));
            obj->putDirect(vm, kind, jsString(vm, String(makeString(imp.kind))));
            result->push(globalObject, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL webAssemblyModuleExports(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(vm, callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.exports called with non WebAssembly.Module argument"_s)));

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& exports = module->moduleInformation().exports;
    if (exports.size()) {
        Identifier name = Identifier::fromString(vm, "name");
        Identifier kind = Identifier::fromString(vm, "kind");
        for (const Wasm::Export& exp : exports) {
            JSObject* obj = constructEmptyObject(globalObject);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, name, jsString(vm, String::fromUTF8(exp.field)));
            obj->putDirect(vm, kind, jsString(vm, String(makeString(exp.kind))));
            result->push(globalObject, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyModule(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(WebAssemblyModuleConstructor::createModule(globalObject, callFrame, WTFMove(source))));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyModule(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Module"));
}

JSWebAssemblyModule* WebAssemblyModuleConstructor::createModule(JSGlobalObject* globalObject, CallFrame* callFrame, Vector<uint8_t>&& buffer)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->webAssemblyModuleStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->webAssemblyModuleStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, JSWebAssemblyModule::createStub(vm, globalObject, structure, Wasm::Module::validateSync(&vm.wasmContext, WTFMove(buffer))));
}

WebAssemblyModuleConstructor* WebAssemblyModuleConstructor::create(VM& vm, Structure* structure, WebAssemblyModulePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyModuleConstructor>(vm.heap)) WebAssemblyModuleConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyModuleConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyModuleConstructor::finishCreation(VM& vm, WebAssemblyModulePrototype* prototype)
{
    Base::finishCreation(vm, "Module"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

WebAssemblyModuleConstructor::WebAssemblyModuleConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyModule, constructJSWebAssemblyModule)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

