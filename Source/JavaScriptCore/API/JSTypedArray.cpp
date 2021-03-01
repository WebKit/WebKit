/*
 * Copyright (C) 2015 Dominic Szablewski (dominic@phoboslab.org)
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

using namespace JSC;

// Helper functions.

inline JSTypedArrayType toJSTypedArrayType(TypedArrayType type)
{
    switch (type) {
    case JSC::TypeDataView:
    case NotTypedArray:
        return kJSTypedArrayTypeNone;
    case TypeInt8:
        return kJSTypedArrayTypeInt8Array;
    case TypeUint8:
        return kJSTypedArrayTypeUint8Array;
    case TypeUint8Clamped:
        return kJSTypedArrayTypeUint8ClampedArray;
    case TypeInt16:
        return kJSTypedArrayTypeInt16Array;
    case TypeUint16:
        return kJSTypedArrayTypeUint16Array;
    case TypeInt32:
        return kJSTypedArrayTypeInt32Array;
    case TypeUint32:
        return kJSTypedArrayTypeUint32Array;
    case TypeFloat32:
        return kJSTypedArrayTypeFloat32Array;
    case TypeFloat64:
        return kJSTypedArrayTypeFloat64Array;
    case TypeBigInt64:
        return kJSTypedArrayTypeBigInt64Array;
    case TypeBigUint64:
        return kJSTypedArrayTypeBigUint64Array;
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

static JSObject* createTypedArray(JSGlobalObject* globalObject, JSTypedArrayType type, RefPtr<ArrayBuffer>&& buffer, size_t offset, size_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!buffer) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    switch (type) {
    case kJSTypedArrayTypeInt8Array:
        return JSInt8Array::create(globalObject, globalObject->typedArrayStructure(TypeInt8), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeInt16Array:
        return JSInt16Array::create(globalObject, globalObject->typedArrayStructure(TypeInt16), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeInt32Array:
        return JSInt32Array::create(globalObject, globalObject->typedArrayStructure(TypeInt32), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeUint8Array:
        return JSUint8Array::create(globalObject, globalObject->typedArrayStructure(TypeUint8), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeUint8ClampedArray:
        return JSUint8ClampedArray::create(globalObject, globalObject->typedArrayStructure(TypeUint8Clamped), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeUint16Array:
        return JSUint16Array::create(globalObject, globalObject->typedArrayStructure(TypeUint16), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeUint32Array:
        return JSUint32Array::create(globalObject, globalObject->typedArrayStructure(TypeUint32), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeFloat32Array:
        return JSFloat32Array::create(globalObject, globalObject->typedArrayStructure(TypeFloat32), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeFloat64Array:
        return JSFloat64Array::create(globalObject, globalObject->typedArrayStructure(TypeFloat64), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeBigInt64Array:
        return JSBigInt64Array::create(globalObject, globalObject->typedArrayStructure(TypeBigInt64), WTFMove(buffer), offset, length);
    case kJSTypedArrayTypeBigUint64Array:
        return JSBigUint64Array::create(globalObject, globalObject->typedArrayStructure(TypeBigUint64), WTFMove(buffer), offset, length);
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
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);

    JSValue value = toJS(globalObject, valueRef);
    if (!value.isObject())
        return kJSTypedArrayTypeNone;
    JSObject* object = value.getObject();

    if (jsDynamicCast<JSArrayBuffer*>(vm, object))
        return kJSTypedArrayTypeArrayBuffer;

    return toJSTypedArrayType(object->classInfo(vm)->typedArrayStorageType);
}

JSObjectRef JSObjectMakeTypedArray(JSContextRef ctx, JSTypedArrayType arrayType, size_t length, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
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
    JSLockHolder locker(vm);
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
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, toJS(jsBufferRef));
    if (!jsBuffer) {
        setException(ctx, exception, createTypeError(globalObject, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"));
        return nullptr;
    }

    RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
    unsigned elementByteSize = elementSize(toTypedArrayType(arrayType));

    JSObject* result = createTypedArray(globalObject, arrayType, WTFMove(buffer), 0, buffer->byteLength() / elementByteSize);
    if (handleExceptionIfNeeded(scope, ctx, exception) == ExceptionStatus::DidThrow)
        return nullptr;
    return toRef(result);
}

JSObjectRef JSObjectMakeTypedArrayWithArrayBufferAndOffset(JSContextRef ctx, JSTypedArrayType arrayType, JSObjectRef jsBufferRef, size_t offset, size_t length, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (arrayType == kJSTypedArrayTypeNone || arrayType == kJSTypedArrayTypeArrayBuffer)
        return nullptr;

    JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, toJS(jsBufferRef));
    if (!jsBuffer) {
        setException(ctx, exception, createTypeError(globalObject, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"));
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
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(vm, object)) {
        if (ArrayBuffer* buffer = typedArray->possiblySharedBuffer()) {
            buffer->pinAndLock();
            return buffer->data();
        }

        setException(ctx, exception, createOutOfMemoryError(globalObject));
    }
    return nullptr;
}

size_t JSObjectGetTypedArrayLength(JSContextRef ctx, JSObjectRef objectRef, JSValueRef*)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(vm, object))
        return typedArray->length();

    return 0;
}

size_t JSObjectGetTypedArrayByteLength(JSContextRef ctx, JSObjectRef objectRef, JSValueRef*)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(vm, object))
        return typedArray->length() * elementSize(typedArray->classInfo(vm)->typedArrayStorageType);

    return 0;
}

size_t JSObjectGetTypedArrayByteOffset(JSContextRef ctx, JSObjectRef objectRef, JSValueRef*)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSObject* object = toJS(objectRef);

    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(vm, object))
        return typedArray->byteOffset();

    return 0;
}

JSObjectRef JSObjectGetTypedArrayBuffer(JSContextRef ctx, JSObjectRef objectRef, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    JSObject* object = toJS(objectRef);


    if (JSArrayBufferView* typedArray = jsDynamicCast<JSArrayBufferView*>(vm, object)) {
        if (ArrayBuffer* buffer = typedArray->possiblySharedBuffer())
            return toRef(vm.m_typedArrayController->toJS(globalObject, typedArray->globalObject(vm), buffer));

        setException(ctx, exception, createOutOfMemoryError(globalObject));
    }

    return nullptr;
}

JSObjectRef JSObjectMakeArrayBufferWithBytesNoCopy(JSContextRef ctx, void* bytes, size_t byteLength, JSTypedArrayBytesDeallocator bytesDeallocator, void* deallocatorContext, JSValueRef* exception)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
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
    VM& vm = globalObject->vm();
    JSLockHolder locker(vm);
    JSObject* object = toJS(objectRef);

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, object)) {
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

size_t JSObjectGetArrayBufferByteLength(JSContextRef ctx, JSObjectRef objectRef, JSValueRef*)
{
    JSGlobalObject* globalObject = toJS(ctx);
    VM& vm = globalObject->vm();
    JSObject* object = toJS(objectRef);

    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(vm, object))
        return jsBuffer->impl()->byteLength();
    
    return 0;
}
