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
#include "WebAssemblyTableConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyTable.h"
#include "StructureInlines.h"
#include "WebAssemblyTablePrototype.h"

#include "WebAssemblyTableConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTableConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyTable, nullptr, CREATE_METHOD_TABLE(WebAssemblyTableConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyTable);
static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyTable);

/* Source for WebAssemblyTableConstructor.lut.h
 @begin constructorTableWebAssemblyTable
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyTable, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* webAssemblyTableStructure = newTarget == callFrame->jsCallee()
        ? globalObject->webAssemblyTableStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->webAssemblyTableStructure());
    RETURN_IF_EXCEPTION(throwScope, { });

    JSObject* memoryDescriptor;
    {
        JSValue argument = callFrame->argument(0);
        if (!argument.isObject())
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table expects its first argument to be an object"_s);
        memoryDescriptor = jsCast<JSObject*>(argument);
    }

    Wasm::TableElementType type;
    {
        Identifier elementIdent = Identifier::fromString(vm, "element");
        JSValue elementValue = memoryDescriptor->get(globalObject, elementIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        String elementString = elementValue.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (elementString == "funcref"_s || elementString == "anyfunc"_s)
            type = Wasm::TableElementType::Funcref;
        else if (elementString == "externref"_s)
            type = Wasm::TableElementType::Externref;
        else
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table expects its 'element' field to be the string 'funcref' or 'externref'"_s);
    }

    Identifier initialIdent = Identifier::fromString(vm, "initial");
    JSValue initialSizeValue = memoryDescriptor->get(globalObject, initialIdent);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    Identifier minimumIdent = Identifier::fromString(vm, "minimum");
    JSValue minSizeValue = memoryDescriptor->get(globalObject, minimumIdent);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    if (!initialSizeValue.isUndefined() && !minSizeValue.isUndefined())
        return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table 'initial' and 'minimum' options are specified at the same time");

    if (!minSizeValue.isUndefined())
        initialSizeValue = minSizeValue;

    uint32_t initial = toNonWrappingUint32(globalObject, initialSizeValue);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    // In WebIDL, "present" means that [[Get]] result is undefined, not [[HasProperty]] result.
    // https://heycam.github.io/webidl/#idl-dictionaries
    Optional<uint32_t> maximum;
    Identifier maximumIdent = Identifier::fromString(vm, "maximum");
    JSValue maxSizeValue = memoryDescriptor->get(globalObject, maximumIdent);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    if (!maxSizeValue.isUndefined()) {
        maximum = toNonWrappingUint32(globalObject, maxSizeValue);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

        if (initial > *maximum)
            return throwVMRangeError(globalObject, throwScope, "'maximum' property must be greater than or equal to the 'initial' property"_s);
    }

    RefPtr<Wasm::Table> wasmTable = Wasm::Table::tryCreate(initial, maximum, type);
    if (!wasmTable)
        return throwVMRangeError(globalObject, throwScope, "couldn't create Table"_s);

    JSWebAssemblyTable* jsWebAssemblyTable = JSWebAssemblyTable::tryCreate(globalObject, vm, webAssemblyTableStructure, wasmTable.releaseNonNull());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (Options::useWebAssemblyReferences()) {
        JSValue defaultValue = callFrame->argumentCount() < 2
            ? defaultValueForReferenceType(jsWebAssemblyTable->table()->wasmType())
            : callFrame->uncheckedArgument(1);
        WebAssemblyFunction* wasmFunction = nullptr;
        WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
        if (jsWebAssemblyTable->table()->isFuncrefTable() && !defaultValue.isNull() && !isWebAssemblyHostFunction(vm, defaultValue, wasmFunction, wasmWrapperFunction))
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.constructor expects the second argument to be null or an instance of WebAssembly.Function"_s);
        for (uint32_t tableIndex = 0; tableIndex < initial; ++tableIndex) {
            if (jsWebAssemblyTable->table()->isFuncrefTable() && wasmFunction)
                jsWebAssemblyTable->set(tableIndex, wasmFunction);
            if (jsWebAssemblyTable->table()->isExternrefTable())
                jsWebAssemblyTable->set(tableIndex, defaultValue);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        }
    }

    RELEASE_AND_RETURN(throwScope, JSValue::encode(jsWebAssemblyTable));
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyTable, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "WebAssembly.Table"));
}

WebAssemblyTableConstructor* WebAssemblyTableConstructor::create(VM& vm, Structure* structure, WebAssemblyTablePrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyTableConstructor>(vm.heap)) WebAssemblyTableConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyTableConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyTableConstructor::finishCreation(VM& vm, WebAssemblyTablePrototype* prototype)
{
    Base::finishCreation(vm, 1, "Table"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyTableConstructor::WebAssemblyTableConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyTable, constructJSWebAssemblyTable)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

