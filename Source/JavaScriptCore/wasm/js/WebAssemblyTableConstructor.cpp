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

#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyTable.h"
#include "WebAssemblyTablePrototype.h"

#include "WebAssemblyTableConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTableConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyTable, nullptr, CREATE_METHOD_TABLE(WebAssemblyTableConstructor) };

/* Source for WebAssemblyTableConstructor.lut.h
 @begin constructorTableWebAssemblyTable
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyTable(ExecState* exec)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* memoryDescriptor;
    {
        JSValue argument = exec->argument(0);
        if (!argument.isObject())
            return JSValue::encode(throwException(exec, throwScope, createTypeError(exec, "WebAssembly.Table expects its first argument to be an object"_s)));
        memoryDescriptor = jsCast<JSObject*>(argument);
    }

    Wasm::TableElementType type;
    {
        Identifier elementIdent = Identifier::fromString(vm, "element");
        JSValue elementValue = memoryDescriptor->get(exec, elementIdent);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        String elementString = elementValue.toWTFString(exec);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (elementString == "funcref" || elementString == "anyfunc")
            type = Wasm::TableElementType::Funcref;
        else if (elementString == "anyref")
            type = Wasm::TableElementType::Anyref;
        else
            return JSValue::encode(throwException(exec, throwScope, createTypeError(exec, "WebAssembly.Table expects its 'element' field to be the string 'funcref' or 'anyref'"_s)));
    }

    Identifier initialIdent = Identifier::fromString(vm, "initial");
    JSValue initialSizeValue = memoryDescriptor->get(exec, initialIdent);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    uint32_t initial = toNonWrappingUint32(exec, initialSizeValue);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    // In WebIDL, "present" means that [[Get]] result is undefined, not [[HasProperty]] result.
    // https://heycam.github.io/webidl/#idl-dictionaries
    Optional<uint32_t> maximum;
    Identifier maximumIdent = Identifier::fromString(vm, "maximum");
    JSValue maxSizeValue = memoryDescriptor->get(exec, maximumIdent);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    if (!maxSizeValue.isUndefined()) {
        maximum = toNonWrappingUint32(exec, maxSizeValue);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

        if (initial > *maximum) {
            return JSValue::encode(throwException(exec, throwScope,
                createRangeError(exec, "'maximum' property must be greater than or equal to the 'initial' property"_s)));
        }
    }

    RefPtr<Wasm::Table> wasmTable = Wasm::Table::tryCreate(initial, maximum, type);
    if (!wasmTable) {
        return JSValue::encode(throwException(exec, throwScope,
            createRangeError(exec, "couldn't create Table"_s)));
    }

    RELEASE_AND_RETURN(throwScope, JSValue::encode(JSWebAssemblyTable::create(exec, vm, exec->lexicalGlobalObject()->webAssemblyTableStructure(), wasmTable.releaseNonNull())));
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyTable(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, scope, "WebAssembly.Table"));
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
    Base::finishCreation(vm, "Table"_s, NameVisibility::Visible, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

WebAssemblyTableConstructor::WebAssemblyTableConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyTable, constructJSWebAssemblyTable)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

