/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "StructuredClone.h"

#include "JSDOMBinding.h"
#include "JSDOMExceptionHandling.h"
#include <JavaScriptCore/JSTypedArrays.h>

namespace WebCore {
using namespace JSC;

enum class CloneMode {
    Full,
    Partial,
};

static EncodedJSValue cloneArrayBufferImpl(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame, CloneMode mode)
{
    VM& vm = lexicalGlobalObject->vm();

    ASSERT(lexicalGlobalObject);
    ASSERT(callFrame->argumentCount());
    ASSERT(callFrame->lexicalGlobalObject(vm) == lexicalGlobalObject);

    auto* buffer = toUnsharedArrayBuffer(vm, callFrame->uncheckedArgument(0));
    if (!buffer) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwDataCloneError(*lexicalGlobalObject, scope);
        return { };
    }
    if (mode == CloneMode::Partial) {
        ASSERT(callFrame->argumentCount() == 3);
        int srcByteOffset = static_cast<int>(callFrame->uncheckedArgument(1).toNumber(lexicalGlobalObject));
        int srcLength = static_cast<int>(callFrame->uncheckedArgument(2).toNumber(lexicalGlobalObject));
        return JSValue::encode(JSArrayBuffer::create(lexicalGlobalObject->vm(), lexicalGlobalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), buffer->slice(srcByteOffset, srcByteOffset + srcLength)));
    }
    return JSValue::encode(JSArrayBuffer::create(lexicalGlobalObject->vm(), lexicalGlobalObject->arrayBufferStructure(ArrayBufferSharingMode::Default), ArrayBuffer::tryCreate(buffer->data(), buffer->byteLength())));
}

EncodedJSValue JSC_HOST_CALL cloneArrayBuffer(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return cloneArrayBufferImpl(globalObject, callFrame, CloneMode::Partial);
}

EncodedJSValue JSC_HOST_CALL structuredCloneArrayBuffer(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return cloneArrayBufferImpl(globalObject, callFrame, CloneMode::Full);
}

EncodedJSValue JSC_HOST_CALL structuredCloneArrayBufferView(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount());

    JSValue value = callFrame->uncheckedArgument(0);
    VM& vm = globalObject->vm();
    auto* bufferView = jsDynamicCast<JSArrayBufferView*>(vm, value);
    ASSERT(bufferView);

    auto* buffer = bufferView->unsharedBuffer();
    if (!buffer) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwDataCloneError(*globalObject, scope);
        return { };
    }
    auto bufferClone = ArrayBuffer::tryCreate(buffer->data(), buffer->byteLength());
    Structure* structure = bufferView->structure(vm);

    if (jsDynamicCast<JSInt8Array*>(vm, value))
        return JSValue::encode(JSInt8Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSInt16Array*>(vm, value))
        return JSValue::encode(JSInt16Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSInt32Array*>(vm, value))
        return JSValue::encode(JSInt32Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSUint8Array*>(vm, value))
        return JSValue::encode(JSUint8Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSUint8ClampedArray*>(vm, value))
        return JSValue::encode(JSUint8ClampedArray::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSUint16Array*>(vm, value))
        return JSValue::encode(JSUint16Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSUint32Array*>(vm, value))
        return JSValue::encode(JSUint32Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSFloat32Array*>(vm, value))
        return JSValue::encode(JSFloat32Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSFloat64Array*>(vm, value))
        return JSValue::encode(JSFloat64Array::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (jsDynamicCast<JSDataView*>(vm, value))
        return JSValue::encode(JSDataView::create(globalObject, structure, WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));

    ASSERT_NOT_REACHED();
    return JSValue::encode(jsUndefined());
}

} // namespace WebCore
