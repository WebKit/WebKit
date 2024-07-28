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
#include "WebAssemblyTablePrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyTable.h"
#include "StructureInlines.h"
#include "WasmTypeDefinition.h"

namespace JSC {
static JSC_DECLARE_CUSTOM_GETTER(webAssemblyTableProtoGetterLength);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncGrow);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncGet);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncSet);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncType);
}

#include "WebAssemblyTablePrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTablePrototype::s_info = { "WebAssembly.Table"_s, &Base::s_info, &prototypeTableWebAssemblyTable, nullptr, CREATE_METHOD_TABLE(WebAssemblyTablePrototype) };

/* Source for WebAssemblyTablePrototype.lut.h
 @begin prototypeTableWebAssemblyTable
 length webAssemblyTableProtoGetterLength ReadOnly|CustomAccessor
 grow   webAssemblyTableProtoFuncGrow   Function 1
 get    webAssemblyTableProtoFuncGet    Function 1
 set    webAssemblyTableProtoFuncSet    Function 1
 type   webAssemblyTableProtoFuncType   Function 0
 @end
 */

static ALWAYS_INLINE JSWebAssemblyTable* getTable(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyTable* result = jsDynamicCast<JSWebAssemblyTable*>(v);
    if (!result) {
        throwException(globalObject, throwScope, 
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Table"_s));
        return nullptr;
    }
    return result;
}

JSC_DEFINE_CUSTOM_GETTER(webAssemblyTableProtoGetterLength, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, JSValue::decode(thisValue));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    return JSValue::encode(jsNumber(table->length()));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncGrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    uint32_t delta = toNonWrappingUint32(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    JSValue defaultValue = jsNull();
    if (callFrame->argumentCount() < 2) {
        if (!table->table()->wasmType().isNullable())
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.grow requires the second argument for non-defaultable table type"_s);
        defaultValue = defaultValueForReferenceType(table->table()->wasmType());
    } else
        defaultValue = callFrame->uncheckedArgument(1);

    if (table->table()->isFuncrefTable() && !defaultValue.isNull() && !isWebAssemblyHostFunction(defaultValue))
        return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.grow expects the second argument to be null or an instance of WebAssembly.Function"_s);
    else if (table->table()->isExternrefTable() && !isExternref(table->table()->wasmType())) {
        RELEASE_ASSERT(Options::useWasmGC());
        JSValue internalValue = Wasm::internalizeExternref(defaultValue);
        if (!Wasm::TypeInformation::castReference(internalValue, true, table->table()->wasmType().index))
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.grow failed to cast the second argument to the table's element type"_s);
    }
    uint32_t oldLength = table->length();

    if (!table->grow(delta, defaultValue))
        return throwVMRangeError(globalObject, throwScope, "WebAssembly.Table.prototype.grow could not grow the table"_s);

    return JSValue::encode(jsNumber(oldLength));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncGet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    uint32_t index = toNonWrappingUint32(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    if (index >= table->length())
        return throwVMRangeError(globalObject, throwScope, "WebAssembly.Table.prototype.get expects an integer less than the length of the table"_s);

    return JSValue::encode(table->get(index));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    uint32_t index = toNonWrappingUint32(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (index >= table->length())
        return throwVMRangeError(globalObject, throwScope, "WebAssembly.Table.prototype.set expects an integer less than the length of the table"_s);

    JSValue value = callFrame->argument(1);
    if (callFrame->argumentCount() < 2) {
        value = defaultValueForReferenceType(table->table()->wasmType());
        if (!table->table()->wasmType().isNullable())
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.set requires the second argument for non-defaultable table type"_s);
    }

    if (table->table()->asFuncrefTable()) {
        WebAssemblyFunction* wasmFunction = nullptr;
        WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
        if (!value.isNull() && !isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction))
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.set expects the second argument to be null or an instance of WebAssembly.Function"_s);

        if (value.isNull())
            table->clear(index);
        else {
            ASSERT(value.isObject() && isWebAssemblyHostFunction(jsCast<JSObject*>(value), wasmFunction, wasmWrapperFunction));
            ASSERT(!!wasmFunction || !!wasmWrapperFunction);
            if (wasmFunction)
                table->set(index, wasmFunction);
            else
                table->set(index, wasmWrapperFunction);
        }
    } else {
        if (isExternref(table->table()->wasmType()))
            table->set(index, value);
        else {
            RELEASE_ASSERT(Options::useWasmGC());
            JSValue internalValue = Wasm::internalizeExternref(value);
            if (!Wasm::TypeInformation::castReference(internalValue, true, table->table()->wasmType().index))
                return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.set failed to cast the second argument to the table's element type"_s);
            table->set(index, internalValue);
        }
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncType, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    JSObject* typeDescriptor = table->type(globalObject);
    if (!typeDescriptor)
        return throwVMTypeError(globalObject, throwScope, "WebAssembly.Table.prototype.type unable to produce type descriptor for the given table"_s);
    RELEASE_AND_RETURN(throwScope, JSValue::encode(typeDescriptor));
}

WebAssemblyTablePrototype* WebAssemblyTablePrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyTablePrototype>(vm)) WebAssemblyTablePrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyTablePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyTablePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyTablePrototype::WebAssemblyTablePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
