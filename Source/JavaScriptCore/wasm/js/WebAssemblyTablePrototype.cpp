/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncLength);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncGrow);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncGet);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyTableProtoFuncSet);
}

#include "WebAssemblyTablePrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyTablePrototype::s_info = { "WebAssembly.Table", &Base::s_info, &prototypeTableWebAssemblyTable, nullptr, CREATE_METHOD_TABLE(WebAssemblyTablePrototype) };

/* Source for WebAssemblyTablePrototype.lut.h
 @begin prototypeTableWebAssemblyTable
 length webAssemblyTableProtoFuncLength Accessor 0
 grow   webAssemblyTableProtoFuncGrow   Function 1
 get    webAssemblyTableProtoFuncGet    Function 1
 set    webAssemblyTableProtoFuncSet    Function 2
 @end
 */

static ALWAYS_INLINE JSWebAssemblyTable* getTable(JSGlobalObject* globalObject, VM& vm, JSValue v)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSWebAssemblyTable* result = jsDynamicCast<JSWebAssemblyTable*>(vm, v);
    if (!result) {
        throwException(globalObject, throwScope, 
            createTypeError(globalObject, "expected |this| value to be an instance of WebAssembly.Table"_s));
        return nullptr;
    }
    return result;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncLength, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
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

    uint32_t oldLength = table->length();

    if (!table->grow(delta))
        return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Table.prototype.grow could not grow the table"_s)));

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
        return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Table.prototype.get expects an integer less than the length of the table"_s)));

    return JSValue::encode(table->get(index));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyTableProtoFuncSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyTable* table = getTable(globalObject, vm, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    JSValue value = callFrame->argument(1);

    uint32_t index = toNonWrappingUint32(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    if (index >= table->length())
        return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Table.prototype.set expects an integer less than the length of the table"_s)));

    if (table->table()->asFuncrefTable()) {
        WebAssemblyFunction* wasmFunction = nullptr;
        WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
        if (!value.isNull() && !isWebAssemblyHostFunction(vm, value, wasmFunction, wasmWrapperFunction))
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Table.prototype.set expects the second argument to be null or an instance of WebAssembly.Function"_s)));

        if (value.isNull())
            table->clear(index);
        else {
            ASSERT(value.isObject() && isWebAssemblyHostFunction(vm, jsCast<JSObject*>(value), wasmFunction, wasmWrapperFunction));
            ASSERT(!!wasmFunction || !!wasmWrapperFunction);
            if (wasmFunction)
                table->set(index, wasmFunction);
            else
                table->set(index, wasmWrapperFunction);
        }
    } else
        table->set(index, value);
    
    return JSValue::encode(jsUndefined());
}

WebAssemblyTablePrototype* WebAssemblyTablePrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyTablePrototype>(vm.heap)) WebAssemblyTablePrototype(vm, structure);
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
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyTablePrototype::WebAssemblyTablePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
