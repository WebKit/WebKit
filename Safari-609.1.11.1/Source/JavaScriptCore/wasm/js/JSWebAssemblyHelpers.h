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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferView.h"
#include "JSCJSValue.h"
#include "JSSourceCode.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyWrapperFunction.h"

namespace JSC {

ALWAYS_INLINE uint32_t toNonWrappingUint32(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = getVM(globalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (value.isUInt32())
        return value.asUInt32();

    double doubleValue = value.toNumber(globalObject);
    RETURN_IF_EXCEPTION(throwScope, { });

    if (!std::isnan(doubleValue) && !std::isinf(doubleValue)) {
        double truncedValue = trunc(doubleValue);
        if (truncedValue >= 0 && truncedValue <= UINT_MAX)
            return static_cast<uint32_t>(truncedValue);
    }

    throwException(globalObject, throwScope, createTypeError(globalObject, "Expect an integer argument in the range: [0, 2^32 - 1]"_s));
    return { };
}

ALWAYS_INLINE std::pair<const uint8_t*, size_t> getWasmBufferFromValue(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = getVM(globalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (auto* source = jsDynamicCast<JSSourceCode*>(vm, value)) {
        auto* provider = static_cast<WebAssemblySourceProvider*>(source->sourceCode().provider());
        return { provider->data().data(), provider->data().size() };
    }

    // If the given bytes argument is not a BufferSource, a TypeError exception is thrown.
    JSArrayBuffer* arrayBuffer = value.getObject() ? jsDynamicCast<JSArrayBuffer*>(vm, value.getObject()) : nullptr;
    JSArrayBufferView* arrayBufferView = value.getObject() ? jsDynamicCast<JSArrayBufferView*>(vm, value.getObject()) : nullptr;
    if (!(arrayBuffer || arrayBufferView)) {
        throwException(globalObject, throwScope, createTypeError(globalObject,
            "first argument must be an ArrayBufferView or an ArrayBuffer"_s, defaultSourceAppender, runtimeTypeForValue(vm, value)));
        return { nullptr, 0 };
    }

    if (arrayBufferView ? arrayBufferView->isNeutered() : arrayBuffer->impl()->isNeutered()) {
        throwException(globalObject, throwScope, createTypeError(globalObject,
            "underlying TypedArray has been detatched from the ArrayBuffer"_s, defaultSourceAppender, runtimeTypeForValue(vm, value)));
        return { nullptr, 0 };
    }

    uint8_t* base = arrayBufferView ? static_cast<uint8_t*>(arrayBufferView->vector()) : static_cast<uint8_t*>(arrayBuffer->impl()->data());
    size_t byteSize = arrayBufferView ? arrayBufferView->length() : arrayBuffer->impl()->byteLength();
    return { base, byteSize };
}

ALWAYS_INLINE Vector<uint8_t> createSourceBufferFromValue(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto [data, byteSize] = getWasmBufferFromValue(globalObject, value);
    RETURN_IF_EXCEPTION(throwScope, Vector<uint8_t>());

    Vector<uint8_t> result;
    if (!result.tryReserveCapacity(byteSize)) {
        throwException(globalObject, throwScope, createOutOfMemoryError(globalObject));
        return result;
    }

    result.grow(byteSize);
    memcpy(result.data(), data, byteSize);
    return result;
}

ALWAYS_INLINE bool isWebAssemblyHostFunction(VM& vm, JSObject* object, WebAssemblyFunction*& wasmFunction, WebAssemblyWrapperFunction*& wasmWrapperFunction)
{
    if (object->inherits<WebAssemblyFunction>(vm)) {
        wasmFunction = jsCast<WebAssemblyFunction*>(object);
        wasmWrapperFunction = nullptr;
        return true;
    }
    if (object->inherits<WebAssemblyWrapperFunction>(vm)) {
        wasmWrapperFunction = jsCast<WebAssemblyWrapperFunction*>(object);
        wasmFunction = nullptr;
        return true;
    }
    return false;
}

ALWAYS_INLINE bool isWebAssemblyHostFunction(VM& vm, JSValue value, WebAssemblyFunction*& wasmFunction, WebAssemblyWrapperFunction*& wasmWrapperFunction)
{
    if (!value.isObject())
        return false;
    return isWebAssemblyHostFunction(vm, jsCast<JSObject*>(value), wasmFunction, wasmWrapperFunction);
}


ALWAYS_INLINE bool isWebAssemblyHostFunction(VM& vm, JSValue object)
{
    WebAssemblyFunction* unused;
    WebAssemblyWrapperFunction* unused2;
    return isWebAssemblyHostFunction(vm, object, unused, unused2);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
