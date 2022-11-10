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
#include "WebAssemblyMemoryConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyMemory.h"
#include "PageCount.h"
#include "StructureInlines.h"
#include "WasmMemory.h"
#include "WebAssemblyMemoryPrototype.h"

#include "WebAssemblyMemoryConstructor.lut.h"

namespace JSC {

const ClassInfo WebAssemblyMemoryConstructor::s_info = { "Function"_s, &Base::s_info, &constructorTableWebAssemblyMemory, nullptr, CREATE_METHOD_TABLE(WebAssemblyMemoryConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructJSWebAssemblyMemory);
static JSC_DECLARE_HOST_FUNCTION(callJSWebAssemblyMemory);

/* Source for WebAssemblyMemoryConstructor.lut.h
 @begin constructorTableWebAssemblyMemory
 @end
 */

JSC_DEFINE_HOST_FUNCTION(constructJSWebAssemblyMemory, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* webAssemblyMemoryStructure = JSC_GET_DERIVED_STRUCTURE(vm, webAssemblyMemoryStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(throwScope, { });

    JSObject* memoryDescriptor;
    {
        JSValue argument = callFrame->argument(0);
        if (!argument.isObject())
            return JSValue::encode(throwException(globalObject, throwScope, createTypeError(globalObject, "WebAssembly.Memory expects its first argument to be an object"_s)));
        memoryDescriptor = jsCast<JSObject*>(argument);
    }

    PageCount initialPageCount;
    {
        Identifier initial = Identifier::fromString(vm, "initial"_s);
        JSValue initSizeValue = memoryDescriptor->get(globalObject, initial);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        Identifier minimum = Identifier::fromString(vm, "minimum"_s);
        JSValue minSizeValue = memoryDescriptor->get(globalObject, minimum);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (!minSizeValue.isUndefined() && !initSizeValue.isUndefined()) {
            // Error because both specified.
            return throwVMTypeError(globalObject, throwScope, "WebAssembly.Memory 'initial' and 'minimum' options are specified at the same time"_s);
        }
        if (!initSizeValue.isUndefined())
            minSizeValue = initSizeValue;

        uint32_t size = toNonWrappingUint32(globalObject, minSizeValue);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (!PageCount::isValid(size))
            return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory 'initial' page count is too large"_s)));
        if (PageCount(size).bytes() > MAX_ARRAY_BUFFER_SIZE)
            return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));
        initialPageCount = PageCount(size);
    }

    PageCount maximumPageCount;
    {
        // In WebIDL, "present" means that [[Get]] result is undefined, not [[HasProperty]] result.
        // https://webidl.spec.whatwg.org/#idl-dictionaries
        Identifier maximum = Identifier::fromString(vm, "maximum"_s);
        JSValue maxSizeValue = memoryDescriptor->get(globalObject, maximum);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (!maxSizeValue.isUndefined()) {
            uint32_t size = toNonWrappingUint32(globalObject, maxSizeValue);
            RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
            if (!PageCount::isValid(size))
                return JSValue::encode(throwException(globalObject, throwScope, createRangeError(globalObject, "WebAssembly.Memory 'maximum' page count is too large"_s)));
            maximumPageCount = PageCount(size);

            if (initialPageCount > maximumPageCount) {
                return JSValue::encode(throwException(globalObject, throwScope,
                    createRangeError(globalObject, "'maximum' page count must be than greater than or equal to the 'initial' page count"_s)));
            }
        }
    }

    // Even though Options::useSharedArrayBuffer() is false, we can create SharedArrayBuffer through wasm shared memory.
    // But we cannot send SharedArrayBuffer to the other workers, so it is not effective.
    MemorySharingMode sharingMode = MemorySharingMode::Default;
    if (LIKELY(Options::useWasmFaultSignalHandler())) {
        JSValue sharedValue = memoryDescriptor->get(globalObject, Identifier::fromString(vm, "shared"_s));
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        bool shared = sharedValue.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
        if (shared) {
            if (!maximumPageCount)
                return throwVMTypeError(globalObject, throwScope, "'maximum' page count must be defined if 'shared' is true"_s);
            sharingMode = MemorySharingMode::Shared;
        }
    }

    auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, webAssemblyMemoryStructure);
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    RefPtr<Wasm::Memory> memory = Wasm::Memory::tryCreate(vm, initialPageCount, maximumPageCount, sharingMode,
        [&vm, jsMemory] (Wasm::Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { jsMemory->growSuccessCallback(vm, oldPageCount, newPageCount); });
    if (!memory)
        return JSValue::encode(throwException(globalObject, throwScope, createOutOfMemoryError(globalObject)));

    jsMemory->adopt(memory.releaseNonNull());
    
    return JSValue::encode(jsMemory);
}

JSC_DEFINE_HOST_FUNCTION(callJSWebAssemblyMemory, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, throwScope, "WebAssembly.Memory"));
}

WebAssemblyMemoryConstructor* WebAssemblyMemoryConstructor::create(VM& vm, Structure* structure, WebAssemblyMemoryPrototype* thisPrototype)
{
    auto* constructor = new (NotNull, allocateCell<WebAssemblyMemoryConstructor>(vm)) WebAssemblyMemoryConstructor(vm, structure);
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

