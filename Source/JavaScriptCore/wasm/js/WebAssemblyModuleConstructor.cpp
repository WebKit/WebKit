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
#include "WebAssemblyModuleConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "ArrayBuffer.h"
#include "ButterflyInlines.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferViewInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyModule.h"
#include "ObjectConstructor.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyModulePrototype.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyModuleCustomSections);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyModuleImports);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyModuleExports);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyModule);
static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyModule);
}

#include "WebAssemblyModuleConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyModuleConstructor::s_info = { "Function"_s, &Base::s_info, &constructorTableWebAssemblyModule, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleConstructor) };

/* Source for WebAssemblyModuleConstructor.lut.h
 @begin constructorTableWebAssemblyModule
 customSections webAssemblyModuleCustomSections Function 2
 imports        webAssemblyModuleImports        Function 1
 exports        webAssemblyModuleExports        Function 1
 @end
 */

JSC_DEFINE_HOST_FUNCTION(webAssemblyModuleCustomSections, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame->argumentCount() < 2))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(callFrame->uncheckedArgument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.customSections called with non WebAssembly.Module argument"_s)));

    String sectionNameString = callFrame->uncheckedArgument(1).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& customSections = module->moduleInformation().customSections;
    for (const Wasm::CustomSection& section : customSections) {
        // FIXME: Add a function that compares a String with a span<char8_t> so we don't need to make a string.
        if (WTF::makeString(section.name) == sectionNameString) {
            auto buffer = ArrayBuffer::tryCreate(section.payload.span());
            if (!buffer)
                return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));

            result->push(globalObject, JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), WTFMove(buffer)));
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

template<typename T>
concept IsImportOrExport = std::same_as<T, Wasm::Import> || std::same_as<T, Wasm::Export>;

// https://github.com/WebAssembly/js-types
// https://webassembly.github.io/js-types/js-api/index.html#dictdef-anyexterntype
template <IsImportOrExport T>
static JSObject* createTypeReflectionObject(JSGlobalObject* globalObject, JSWebAssemblyModule* module, const T& impOrExp)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* typeObj;
    constexpr auto errorMessage = std::same_as<T, Wasm::Import> ? "WebAssembly.Module.imports unable to produce import descriptors for the given module"_s : "WebAssembly.Module.exports unable to produce export descriptors for the given module"_s;
    switch (impOrExp.kind) {
    case Wasm::ExternalKind::Function: {
        typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);

        Wasm::TypeIndex typeIndex = module->moduleInformation().typeIndexFromFunctionIndexSpace(impOrExp.kindIndex);
        const auto& signature = Wasm::TypeInformation::getFunctionSignature(typeIndex);

        JSArray* functionParametersTypes = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(throwScope, { });
        auto argumentCount = signature.argumentCount();
        for (unsigned i = 0; i < argumentCount; ++i) {
            JSString* typeString = Wasm::typeToJSAPIString(vm, signature.argumentType(i));
            if (!typeString) {
                throwException(globalObject, throwScope, createTypeError(globalObject, errorMessage));
                return nullptr;
            }
            functionParametersTypes->push(globalObject, typeString);
            RETURN_IF_EXCEPTION(throwScope, { });
        }

        JSArray* functionResultsTypes = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(throwScope, { });
        auto returnCount = signature.returnCount();
        for (unsigned i = 0; i < returnCount; ++i) {
            JSString* typeString = Wasm::typeToJSAPIString(vm, signature.returnType(i));
            if (!typeString) {
                throwException(globalObject, throwScope, createTypeError(globalObject, errorMessage));
                return nullptr;
            }
            functionResultsTypes->push(globalObject, typeString);
            RETURN_IF_EXCEPTION(throwScope, { });
        }

        typeObj->putDirect(vm, Identifier::fromString(vm, "parameters"_s), functionParametersTypes);
        typeObj->putDirect(vm, Identifier::fromString(vm, "results"_s), functionResultsTypes);
        break;
    }
    case Wasm::ExternalKind::Memory: {
        const auto& memory = module->moduleInformation().memory;
        PageCount maximum = memory.maximum();
        if (maximum.isValid()) {
            typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 3);
            typeObj->putDirect(vm, Identifier::fromString(vm, "maximum"_s), jsNumber(maximum.pageCount()));
        } else
            typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);
        typeObj->putDirect(vm, Identifier::fromString(vm, "minimum"_s), jsNumber(memory.initial().pageCount()));
        typeObj->putDirect(vm, Identifier::fromString(vm, "shared"_s), jsBoolean(memory.isShared()));
        break;
    }
    case Wasm::ExternalKind::Table: {
        const auto& table = module->moduleInformation().table(impOrExp.kindIndex);
        JSString* typeString = Wasm::typeToJSAPIString(vm, table.wasmType());
        if (!typeString) {
            throwException(globalObject, throwScope, createTypeError(globalObject, errorMessage));
            return nullptr;
        }
        std::optional<uint32_t> maximum = table.maximum();
        if (maximum) {
            typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 3);
            typeObj->putDirect(vm, Identifier::fromString(vm, "maximum"_s), jsNumber(maximum.value()));
        } else
            typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);
        typeObj->putDirect(vm, Identifier::fromString(vm, "minimum"_s), jsNumber(table.initial()));
        typeObj->putDirect(vm, Identifier::fromString(vm, "element"_s), typeString);
        break;
    }
    case Wasm::ExternalKind::Global: {
        typeObj = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);
        const auto& global = module->moduleInformation().global(impOrExp.kindIndex);
        JSString* typeString = Wasm::typeToJSAPIString(vm, global.type);
        if (!typeString) {
            throwException(globalObject, throwScope, createTypeError(globalObject, errorMessage));
            return nullptr;
        }
        typeObj->putDirect(vm, Identifier::fromString(vm, "mutable"_s), jsBoolean(global.mutability == Wasm::Mutability::Mutable));
        typeObj->putDirect(vm, vm.propertyNames->value, typeString);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return typeObj;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyModuleImports, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.imports called with non WebAssembly.Module argument"_s)));

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& imports = module->moduleInformation().imports;
    if (imports.size()) {
        Identifier moduleId = Identifier::fromString(vm, "module"_s);
        Identifier name = Identifier::fromString(vm, "name"_s);
        Identifier kind = Identifier::fromString(vm, "kind"_s);
        Identifier type = Identifier::fromString(vm, "type"_s);
        for (const Wasm::Import& imp : imports) {
            JSObject* obj = constructEmptyObject(globalObject);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, moduleId, jsString(vm, WTF::makeString(imp.module)));
            obj->putDirect(vm, name, jsString(vm, WTF::makeString(imp.field)));
            obj->putDirect(vm, kind, jsString(vm, String::fromLatin1(makeString(imp.kind))));
            if (imp.kind == Wasm::ExternalKind::Function || imp.kind == Wasm::ExternalKind::Table || imp.kind == Wasm::ExternalKind::Memory || imp.kind == Wasm::ExternalKind::Global) {
                JSObject* typeReflectionObject = createTypeReflectionObject(globalObject, module, imp);
                RETURN_IF_EXCEPTION(throwScope, { });
                obj->putDirect(vm, type, typeReflectionObject);
            }
            result->push(globalObject, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyModuleExports, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyModule* module = jsDynamicCast<JSWebAssemblyModule*>(callFrame->argument(0));
    if (!module)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Module.exports called with non WebAssembly.Module argument"_s)));

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(throwScope, { });

    const auto& exports = module->moduleInformation().exports;
    if (exports.size()) {
        Identifier name = Identifier::fromString(vm, "name"_s);
        Identifier kind = Identifier::fromString(vm, "kind"_s);
        Identifier type = Identifier::fromString(vm, "type"_s);
        for (const Wasm::Export& exp : exports) {
            JSObject* obj = constructEmptyObject(globalObject);
            RETURN_IF_EXCEPTION(throwScope, { });
            obj->putDirect(vm, name, jsString(vm, WTF::makeString(exp.field)));
            obj->putDirect(vm, kind, jsString(vm, String::fromLatin1(makeString(exp.kind))));
            if (exp.kind == Wasm::ExternalKind::Function || exp.kind == Wasm::ExternalKind::Table || exp.kind == Wasm::ExternalKind::Memory || exp.kind == Wasm::ExternalKind::Global) {
                JSObject* typeReflectionObject = createTypeReflectionObject(globalObject, module, exp);
                RETURN_IF_EXCEPTION(throwScope, { });
                obj->putDirect(vm, type, typeReflectionObject);
            }
            result->push(globalObject, obj);
            RETURN_IF_EXCEPTION(throwScope, { });
        }
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyModule, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    Vector<uint8_t> source = createSourceBufferFromValue(vm, globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(WebAssemblyModuleConstructor::createModule(globalObject, callFrame, WTFMove(source))));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyModule, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Module"_s));
}

JSWebAssemblyModule* WebAssemblyModuleConstructor::createModule(JSGlobalObject* globalObject, CallFrame* callFrame, Vector<uint8_t>&& buffer)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyModuleStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto result = Wasm::Module::validateSync(vm, WTFMove(buffer));
    if (UNLIKELY(!result.has_value())) {
        throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, JSWebAssemblyModule::create(vm, structure, WTFMove(result.value())));
}

WebAssemblyModuleConstructor* WebAssemblyModuleConstructor::create(VM& vm, Structure* structure, WebAssemblyModulePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyModuleConstructor>(vm)) WebAssemblyModuleConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyModuleConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyModuleConstructor::finishCreation(VM& vm, WebAssemblyModulePrototype* prototype)
{
    Base::finishCreation(vm, 1, "Module"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyModuleConstructor::WebAssemblyModuleConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyModule, constructJSWebAssemblyModule)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

