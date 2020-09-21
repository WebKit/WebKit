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
#include "WebAssemblyMemoryConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyMemory.h"
#include "StructureInlines.h"
#include "WasmMemory.h"
#include "WasmPageCount.h"
#include "WebAssemblyMemoryPrototype.h"

#include "WebAssemblyMemoryConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyMemoryConstructor::s_info = { "Function", &Base::s_info, &constructorTableWebAssemblyMemory, nullptr, CREATE_METHOD_TABLE(WebAssemblyMemoryConstructor) };

/* Source for WebAssemblyMemoryConstructor.lut.h
 @begin constructorTableWebAssemblyMemory
 @end
 */

static EncodedJSValue JSC_HOST_CALL constructJSWebAssemblyMemory(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    if (callFrame->argumentCount() != 1)
        return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Memory expects exactly one argument"_s)));

    JSObject* memoryDescriptor;
    {
        JSValue argument = callFrame->argument(0);
        if (!argument.isObject())
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Memory expects its first argument to be an object"_s)));
        memoryDescriptor = jsCast<JSObject*>(argument);
    }

    Wasm::PageCount initialPageCount;
    {
        Identifier initial = Identifier::fromString(vm, "initial");
        JSValue minSizeValue = memoryDescriptor->get(globalObject, initial);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        uint32_t size = toNonWrappingUint32(globalObject, minSizeValue);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (!Wasm::PageCount::isValid(size))
            return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory 'initial' page count is too large"_s)));
        if (Wasm::PageCount(size).bytes() > MAX_ARRAY_BUFFER_SIZE)
            return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));
        initialPageCount = Wasm::PageCount(size);
    }

    Wasm::PageCount maximumPageCount;
    {
        // In WebIDL, "present" means that [[Get]] result is undefined, not [[HasProperty]] result.
        // https://heycam.github.io/webidl/#idl-dictionaries
        Identifier maximum = Identifier::fromString(vm, "maximum");
        JSValue maxSizeValue = memoryDescriptor->get(globalObject, maximum);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (!maxSizeValue.isUndefined()) {
            uint32_t size = toNonWrappingUint32(globalObject, maxSizeValue);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            if (!Wasm::PageCount::isValid(size))
                return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory 'maximum' page count is too large"_s)));
            maximumPageCount = Wasm::PageCount(size);

            if (initialPageCount > maximumPageCount) {
                return JSValue::encode(throwException(globalObject, throwScope,
                    createRangeError(globalObject, "'maximum' page count must be than greater than or equal to the 'initial' page count"_s)));
            }
        }
    }

    auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, globalObject->webAssemblyMemoryStructure());
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    RefPtr<Wasm::Memory> memory = Wasm::Memory::tryCreate(initialPageCount, maximumPageCount,
        [&vm] (Wasm::Memory::NotifyPressure) { vm.heap.collectAsync(CollectionScope::Full); },
        [&vm] (Wasm::Memory::SyncTryToReclaim) { vm.heap.collectSync(CollectionScope::Full); },
        [&vm, jsMemory] (Wasm::Memory::GrowSuccess, Wasm::PageCount oldPageCount, Wasm::PageCount newPageCount) { jsMemory->growSuccessCallback(vm, oldPageCount, newPageCount); });
    if (!memory)
        return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));

    jsMemory->adopt(memory.releaseNonNull());
    
    return JSValue::encode(jsMemory);
}

static EncodedJSValue JSC_HOST_CALL callJSWebAssemblyMemory(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, throwScope, "WebAssembly.Memory"));
}

WebAssemblyMemoryConstructor* WebAssemblyMemoryConstructor::create(VM& vm, Structure* structure, WebAssemblyMemoryPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyMemoryConstructor>(vm.heap)) WebAssemblyMemoryConstructor(vm, structure);
    constructor->finishCreation(vm, thisPrototype);
    return constructor;
}

Structure* WebAssemblyMemoryConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

void WebAssemblyMemoryConstructor::finishCreation(VM& vm, WebAssemblyMemoryPrototype* prototype)
{
    Base::finishCreation(vm, 1, "Memory"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

WebAssemblyMemoryConstructor::WebAssemblyMemoryConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callJSWebAssemblyMemory, constructJSWebAssemblyMemory)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

