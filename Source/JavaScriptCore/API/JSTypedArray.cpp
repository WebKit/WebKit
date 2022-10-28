/*
 * Copyright (C) 2015 Dominic Szablewski (dominic@phoboslab.org)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSTypedArray.h"

#include "APICast.h"
#include "APIUtils.h"
#include "ClassInfo.h"
#include "JSCInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSTypedArrays.h"
#include "TypedArrayController.h"
#include <wtf/RefPtr.h>

#if PLATFORM(IOS)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

using namespace JSC;

// Helper functions.

inline JSTypedArrayType toJSTypedArrayType(JSC::JSType type)
{
    switch (type) {
    case JSC::Int8ArrayType:
        return kJSTypedArrayTypeInt8Array;
    case JSC::Uint8ArrayType:
        return kJSTypedArrayTypeUint8Array;
    case JSC::Uint8ClampedArrayType:
        return kJSTypedArrayTypeUint8ClampedArray;
    case JSC::Int16ArrayType:
        return kJSTypedArrayTypeInt16Array;
    case JSC::Uint16ArrayType:
        return kJSTypedArrayTypeUint16Array;
    case JSC::Int32ArrayType:
        return kJSTypedArrayTypeInt32Array;
    case JSC::Uint32ArrayType:
        return kJSTypedArrayTypeUint32Array;
    case JSC::Float32ArrayType:
        return kJSTypedArrayTypeFloat32Array;
    case JSC::Float64ArrayType:
        return kJSTypedArrayTypeFloat64Array;
    case JSC::BigInt64ArrayType:
        return kJSTypedArrayTypeBigInt64Array;
    case JSC::BigUint64ArrayType:
        return kJSTypedArrayTypeBigUint64Array;
    default:
        return kJSTypedArrayTypeNone;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline TypedArrayType toTypedArrayType(JSTypedArrayType type)
{
    switch (type) {
    case kJSTypedArrayTypeArrayBuffer:
    case kJSTypedArrayTypeNone:
        return NotTypedArray;
    case kJSTypedArrayTypeInt8Array:
        return TypeInt8;
    case kJSTypedArrayTypeUint8Array:
        return TypeUint8;
    case kJSTypedArrayTypeUint8ClampedArray:
        return TypeUint8Clamped;
    case kJSTypedArrayTypeInt16Array:
        return TypeInt16;
    case kJSTypedArrayTypeUint16Array:
        return TypeUint16;
    case kJSTypedArrayTypeInt32Array:
        return TypeInt32;
    case kJSTypedArrayTypeUint32Array:
        return TypeUint32;
    case kJSTypedArrayTypeFloat32Array:
        return TypeFloat32;
    case kJSTypedArrayTypeFloat64Array:
        return TypeFloat64;
    case kJSTypedArrayTypeBigInt64Array:
        return TypeBigInt64;
    case kJSTypedArrayTypeBigUint64Array:
        return TypeBigUint64;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static JSObject* createTypedArray(JSGlobalObject* globalObject, JSTypedArrayType type, RefPtr<ArrayBuffer>&& buffer, size_t offset, std::optional<size_t> length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!buffer) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    bool isResizableOrGrowableShared = buffer->isResizableOrGrowableShared();
    switch (type) {
#define JSC_TYPED_ARRAY_FACTORY(type) case kJSTypedArrayType##type##Array: { \
        return JS##type##Array::create(globalObject, globalObject->typedArrayStructure(Type##type, isResizableOrGrowableShared), WTFMove(buffer), offset, length.value()); \
    }
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_TYPED_ARRAY_FACTORY)
#undef JSC_TYPED_ARRAY_CHECK
    case kJSTypedArrayTypeArrayBuffer:
    case kJSTypedArrayTypeNone:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return nullptr;
}

// Implementations of the API functions.

JSTypedArrayType JSValueGetTypedArrayType(JSContextRef ctx, JSValueRef valueRef, JSValueRef*)
{

    JSGlobalObject* globalObject = toJS(ctx);


    JSValue value = toJS(globalObject, valueRef);
    if (!value.isObject())
        return kJSTypedArrayTypeNone;
    JSObject* object = value.getObject();

    if (jsDynamicCast<JSArrayBuffer*>(object))
        return kJSTypedArrayTypeArrayBuffer;

    return toJSTypedArrayType(object->type());
}

JSObjectRef JSObjectMakeTypedArray(JSContextRef ctx, JSTypedArrayType arrayType, size_t length, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    unsigned elementByteSize = elementSize(toTypedArrayType(arrayType));

    auto buffer = ArrayBuffer::tryCreate(length, elementByteSize);
    JSObject* result = createTypedArray(globalObject, arrayType, WTFMove(buffer), 0, length);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(result);
}

JSObjectRef JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef ctx, JSTypedArrayType arrayType, void* bytes, size_t length, JSTypedArrayBytesDeallocator destructor, void* destructorContext, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    unsigned elementByteSize = elementSize(toTypedArrayType(arrayType));

    auto buffer = ArrayBuffer::createFromBytes(bytes, length, createSharedTask<void(void*)>([=](void* p) {
        if (destructor)
            destructor(p, destructorContext);
    }));
    JSObject* result = createTypedArray(globalObject, arrayType, WTFMove(buffer), 0, length / elementByteSize);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(result);
}

JSObjectRef JSObjectMakeTypedArrayWithArrayBuffer(JSContextRef ctx, JSTypedArrayType arrayType, JSObjectRef jsBufferRef, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(toJS(jsBufferRef));
    if (!jsBuffer) {
        setException(ctx, exception, createTypeError(globalObject, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"_s));
        return nullptr;
    }

    RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
    unsigned elementByteSize = elementSize(toTypedArrayType(arrayType));

    std::optional<size_t> length;
    if (!buffer->isResizableOrGrowableShared())
        length = buffer->byteLength() / elementByteSize;
    JSObject* result = createTypedArray(globalObject, arrayType, WTFMove(buffer), 0, length);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(result);
}

JSObjectRef JSObjectMakeTypedArrayWithArrayBufferAndOffset(JSContextRef ctx, JSTypedArrayType arrayType, JSObjectRef jsBufferRef, size_t offset, size_t length, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(toJS(jsBufferRef));
    if (!jsBuffer) {
        setException(ctx, exception, createTypeError(globalObject, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"_s));
        return nullptr;
    }

    JSObject* result = createTypedArray(globalObject, arrayType, jsBuffer->impl(), offset, length);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(result);
}

void* JSObjectGetTypedArrayBytesPtr(JSContextRef ctx, JSObjectRef objectRef, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);

    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(object)) {
        if (ArrayBuffer* buffer = typedArray->possiblySharedBuffer()) {
            buffer->pinAndLock();
            return buffer->data();
        }

        setException(ctx, exception, createOutOfMemoryError(globalObject));
    }
    return nullptr;
}

size_t JSObjectGetTypedArrayLength(JSContextRef, JSObjectRef objectRef, JSValueRef*)
{
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(object))
        return typedArray->length();

    return 0;
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef, JSObjectRef objectRef, JSValueRef*)
{
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(object))
        return typedArray->byteLength();

    return 0;
}

size_t JSObjectGetTypedArrayByteOffset(JSContextRef, JSObjectRef objectRef, JSValueRef*)
{
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(object))
        return typedArray->byteOffset();

    return 0;
}

JSObjectRef JSObjectGetTypedArrayBuffer(JSContextRef ctx, JSObjectRef objectRef, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    auto &vm = globalObject->vm();
    
    JSObject* object = toJS(objectRef);


    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(object)) {
        if (ArrayBuffer* buffer = typedArray->possiblySharedBuffer())
            return toRef(vm.m_typedArrayController->toJS(globalObject, typedArray->globalObject(), buffer));

        setException(ctx, exception, createOutOfMemoryError(globalObject));
    }

    return nullptr;
}

JSObjectRef JSObjectMakeArrayBufferWithBytesNoCopy(JSContextRef ctx, void* bytes, size_t byteLength, JSTypedArrayBytesDeallocator bytesDeallocator, void* deallocatorContext, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();

    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto buffer = ArrayBuffer::createFromBytes(bytes, byteLength, createSharedTask<void(void*)>([=](void* p) {
        if (bytesDeallocator)
            bytesDeallocator(p, deallocatorContext);
    }));

    JSArrayBuffer* jsBuffer = JSArrayBuffer::create(vm, globalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), WTFMove(buffer));
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;

    return toRef(jsBuffer);
}

void* JSObjectGetArrayBufferBytesPtr(JSContextRef ctx, JSObjectRef objectRef, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    JSObject* object = toJS(objectRef);

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(object)) {
        ArrayBuffer* buffer = jsBuffer->impl();
        if (buffer->isWasmMemory()) {
            setException(ctx, exception, createTypeError(globalObject, "Cannot get the backing buffer for a WebAssembly.Memory"_s));
            return nullptr;
        }

        buffer->pinAndLock();
        return buffer->data();
    }
    return nullptr;
}

#if PLATFORM(IOS)
inline static bool isLinkedBeforeTypedArrayLengthQuirk()
{
    return !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoTypedArrayAPIQuirk);
}
#else
inline static bool isLinkedBeforeTypedArrayLengthQuirk() { return false; }
#endif

size_t JSObjectGetArrayBufferByteLength(JSContextRef, JSObjectRef objectRef, JSValueRef*)
{
    JSObject* object = toJS(objectRef);

    if (!object) {
        // For some reason prior to https://bugs.webkit.org/show_bug.cgi?id=235720 Clang would emit code
        // to early return if objectRef is 0 but not after. Passing 0 should be invalid API use.
        static bool shouldntCrash = isLinkedBeforeTypedArrayLengthQuirk();
        RELEASE_ASSERT(shouldntCrash);
        return 0;
    }

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(object))
        return jsBuffer->impl()->byteLength();
    
    return 0;
}
