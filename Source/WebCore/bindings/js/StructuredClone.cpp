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

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include <runtime/JSTypedArrays.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL structuredCloneArrayBuffer(ExecState* execState)
{
    ASSERT(execState);
    ASSERT(execState->argumentCount());
    ASSERT(execState->lexicalGlobalObject());

    ArrayBuffer* buffer = toArrayBuffer(execState->uncheckedArgument(0));
    if (!buffer) {
        setDOMException(execState, DATA_CLONE_ERR);
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(JSArrayBuffer::create(execState->vm(), execState->lexicalGlobalObject()->arrayBufferStructure(), ArrayBuffer::tryCreate(buffer->data(), buffer->byteLength())));
}

EncodedJSValue JSC_HOST_CALL structuredCloneArrayBufferView(ExecState* execState)
{
    ASSERT(execState);
    ASSERT(execState->argumentCount());

    JSValue value = execState->uncheckedArgument(0);
    auto* bufferView = JSC::jsDynamicCast<JSArrayBufferView*>(value);
    ASSERT(bufferView);

    auto* buffer = bufferView->buffer();
    if (!buffer) {
        setDOMException(execState, DATA_CLONE_ERR);
        return JSValue::encode(jsUndefined());
    }
    auto bufferClone = ArrayBuffer::tryCreate(buffer->data(), buffer->byteLength());

    if (JSC::jsDynamicCast<JSInt8Array*>(value))
        return JSValue::encode(JSInt8Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSInt16Array*>(value))
        return JSValue::encode(JSInt16Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSInt32Array*>(value))
        return JSValue::encode(JSInt32Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSUint8Array*>(value))
        return JSValue::encode(JSUint8Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSUint8ClampedArray*>(value))
        return JSValue::encode(JSUint8ClampedArray::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSUint16Array*>(value))
        return JSValue::encode(JSUint16Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSUint32Array*>(value))
        return JSValue::encode(JSUint32Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSFloat32Array*>(value))
        return JSValue::encode(JSFloat32Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSFloat64Array*>(value))
        return JSValue::encode(JSFloat64Array::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));
    if (JSC::jsDynamicCast<JSDataView*>(value))
        return JSValue::encode(JSDataView::create(execState, bufferView->structure(), WTFMove(bufferClone), bufferView->byteOffset(), bufferView->length()));

    ASSERT_NOT_REACHED();
    return JSValue::encode(jsUndefined());
}

} // namespace WebCore
