/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "JSDataView.h"
#include "JSSourceCode.h"
#include "JSWebAssemblyRuntimeError.h"
#include "WasmFormat.h"
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

ALWAYS_INLINE std::pair<const uint8_t*, size_t> getWasmBufferFromValue(JSGlobalObject* globalObject, JSValue value, const WebAssemblySourceProviderBufferGuard&)
{
    VM& vm = getVM(globalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (auto* source = jsDynamicCast<JSSourceCode*>(value)) {
        auto* provider = static_cast<BaseWebAssemblySourceProvider*>(source->sourceCode().provider());
        return { provider->data(), provider->size() };
    }

    // If the given bytes argument is not a BufferSource, a TypeError exception is thrown.
    JSArrayBuffer* arrayBuffer = value.getObject() ? jsDynamicCast<JSArrayBuffer*>(value.getObject()) : nullptr;
    JSArrayBufferView* arrayBufferView = value.getObject() ? jsDynamicCast<JSArrayBufferView*>(value.getObject()) : nullptr;
    if (!(arrayBuffer || arrayBufferView)) {
        throwException(globalObject, throwScope, createTypeError(globalObject,
            "first argument must be an ArrayBufferView or an ArrayBuffer"_s, defaultSourceAppender, runtimeTypeForValue(value)));
        return { nullptr, 0 };
    }

    if (arrayBufferView) {
        if (isTypedArrayType(arrayBufferView->type())) {
            validateTypedArray(globalObject, arrayBufferView);
            RETURN_IF_EXCEPTION(throwScope, { });
        } else {
            IdempotentArrayBufferByteLengthGetter<std::memory_order_relaxed> getter;
            if (UNLIKELY(!jsCast<JSDataView*>(arrayBufferView)->viewByteLength(getter))) {
                throwTypeError(globalObject, throwScope, typedArrayBufferHasBeenDetachedErrorMessage);
                return { };
            }
        }
    } else if (arrayBuffer->impl()->isDetached()) {
        throwTypeError(globalObject, throwScope, typedArrayBufferHasBeenDetachedErrorMessage);
        return { };
    }

    uint8_t* base = arrayBufferView ? static_cast<uint8_t*>(arrayBufferView->vector()) : static_cast<uint8_t*>(arrayBuffer->impl()->data());
    size_t byteSize = arrayBufferView ? arrayBufferView->byteLength() : arrayBuffer->impl()->byteLength();
    return { base, byteSize };
}

ALWAYS_INLINE Vector<uint8_t> createSourceBufferFromValue(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    BaseWebAssemblySourceProvider* provider = nullptr;
    if (auto* source = jsDynamicCast<JSSourceCode*>(value))
        provider = static_cast<BaseWebAssemblySourceProvider*>(source->sourceCode().provider());
    WebAssemblySourceProviderBufferGuard bufferGuard(provider);

    auto [data, byteSize] = getWasmBufferFromValue(globalObject, value, bufferGuard);
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

ALWAYS_INLINE bool isWebAssemblyHostFunction(JSObject* object, WebAssemblyFunction*& wasmFunction, WebAssemblyWrapperFunction*& wasmWrapperFunction)
{
    if (object->inherits<WebAssemblyFunction>()) {
        wasmFunction = jsCast<WebAssemblyFunction*>(object);
        wasmWrapperFunction = nullptr;
        return true;
    }
    if (object->inherits<WebAssemblyWrapperFunction>()) {
        wasmWrapperFunction = jsCast<WebAssemblyWrapperFunction*>(object);
        wasmFunction = nullptr;
        return true;
    }
    return false;
}

ALWAYS_INLINE bool isWebAssemblyHostFunction(JSValue value, WebAssemblyFunction*& wasmFunction, WebAssemblyWrapperFunction*& wasmWrapperFunction)
{
    if (!value.isObject())
        return false;
    return isWebAssemblyHostFunction(jsCast<JSObject*>(value), wasmFunction, wasmWrapperFunction);
}

ALWAYS_INLINE bool isWebAssemblyHostFunction(JSValue object)
{
    WebAssemblyFunction* unused;
    WebAssemblyWrapperFunction* unused2;
    return isWebAssemblyHostFunction(object, unused, unused2);
}

ALWAYS_INLINE JSValue defaultValueForReferenceType(const Wasm::Type type)
{
    ASSERT(Wasm::isRefType(type));
    if (Wasm::isExternref(type))
        return jsUndefined();
    ASSERT(Wasm::isFuncref(type));
    return jsNull();
}

ALWAYS_INLINE JSValue toJSValue(JSGlobalObject* globalObject, const Wasm::Type type, uint64_t bits)
{
    switch (type.kind) {
    case Wasm::TypeKind::Void:
        return jsUndefined();
    case Wasm::TypeKind::I32:
        return jsNumber(static_cast<int32_t>(bits));
    case Wasm::TypeKind::F32:
        return jsNumber(purifyNaN(bitwise_cast<float>(static_cast<int32_t>(bits))));
    case Wasm::TypeKind::F64:
        return jsNumber(purifyNaN(bitwise_cast<double>(bits)));
    case Wasm::TypeKind::I64:
        return JSBigInt::createFrom(globalObject, static_cast<int64_t>(bits));
    case Wasm::TypeKind::Ref:
    case Wasm::TypeKind::RefNull:
    case Wasm::TypeKind::Externref:
    case Wasm::TypeKind::Funcref:
        return bitwise_cast<JSValue>(bits);
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return JSValue();
}

ALWAYS_INLINE uint64_t fromJSValue(JSGlobalObject* globalObject, const Wasm::Type type, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    switch (type.kind) {
    case Wasm::TypeKind::I32:
        RELEASE_AND_RETURN(scope, value.toInt32(globalObject));
    case Wasm::TypeKind::I64:
        RELEASE_AND_RETURN(scope, bitwise_cast<uint64_t>(value.toBigInt64(globalObject)));
    case Wasm::TypeKind::F32:
        RELEASE_AND_RETURN(scope, bitwise_cast<uint32_t>(value.toFloat(globalObject)));
    case Wasm::TypeKind::F64:
        RELEASE_AND_RETURN(scope, bitwise_cast<uint64_t>(value.toNumber(globalObject)));
    default: {
        if (Wasm::isExternref(type)) {
            if (!type.isNullable() && value.isNull())
                return throwVMException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "Non-null Externref cannot be null"_s));
        } else if (Wasm::isFuncref(type) || isRefWithTypeIndex(type)) {
            WebAssemblyFunction* wasmFunction = nullptr;
            WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
            if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && (!type.isNullable() || !value.isNull()))
                return throwVMException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "Funcref must be an exported wasm function"_s));
            if (isRefWithTypeIndex(type) && !value.isNull()) {
                Wasm::TypeIndex paramIndex = type.index;
                Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                if (paramIndex != argIndex)
                    return throwVMException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "Argument function did not match the reference type"_s));
            }
        } else if (Wasm::isI31ref(type))
            return throwVMException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "I31ref import from JS currently unsupported"_s));
        else
            RELEASE_ASSERT_NOT_REACHED();
    }
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(value));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
