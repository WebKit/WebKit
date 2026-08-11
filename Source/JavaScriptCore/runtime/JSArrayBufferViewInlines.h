/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSArrayBufferViewInlinesLight.h>
#include <JavaScriptCore/JSDataView.h>
#include <JavaScriptCore/TypedArrayType.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace JSC {

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

inline void JSArrayBufferView::refreshVector(void* newData)
{
    // We ensure that the vector is really there because these notifications are delivered to
    // incoming references of a buffer, and an incoming reference from a view to a buffer remains in
    // place even after a view detaches.
    if (hasVector()) {
        void* newVectorPtr = static_cast<uint8_t*>(newData) + byteOffsetRaw();
        m_vector.setWithoutBarrier(newVectorPtr);
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

template<typename Getter>
bool isArrayBufferViewOutOfBounds(JSArrayBufferView* view, Getter& getter)
{
    // https://tc39.es/proposal-resizablearraybuffer/#sec-isintegerindexedobjectoutofbounds
    // https://tc39.es/proposal-resizablearraybuffer/#sec-isarraybufferviewoutofbounds
    //
    // This function should work with DataView too.

    if (view->isDetached()) [[unlikely]]
        return true;

    if (!view->isResizableOrGrowableShared()) [[likely]]
        return false;

    ASSERT(hasArrayBuffer(view->mode()) && isResizableOrGrowableShared(view->mode()));
    RefPtr<ArrayBuffer> buffer = view->possiblySharedBuffer();
    if (!buffer)
        return true;

    size_t bufferByteLength = getter(*buffer);
    size_t byteOffsetStart = view->byteOffsetRaw();
    size_t byteOffsetEnd = 0;
    if (view->isAutoLength())
        byteOffsetEnd = bufferByteLength;
    else
        byteOffsetEnd = byteOffsetStart + view->byteLengthRaw();

    return byteOffsetStart > bufferByteLength || byteOffsetEnd > bufferByteLength;
}

template<typename Getter>
bool isIntegerIndexedObjectOutOfBounds(JSArrayBufferView* typedArray, Getter& getter)
{
    return isArrayBufferViewOutOfBounds(typedArray, getter);
}

template<typename Getter>
std::optional<size_t> integerIndexedObjectLength(JSArrayBufferView* typedArray, Getter& getter)
{
    // https://tc39.es/proposal-resizablearraybuffer/#sec-integerindexedobjectlength

    if (isIntegerIndexedObjectOutOfBounds(typedArray, getter)) [[unlikely]]
        return std::nullopt;

    if (!typedArray->isAutoLength()) [[likely]]
        return typedArray->lengthRaw();

    ASSERT(hasArrayBuffer(typedArray->mode()) && isResizableOrGrowableShared(typedArray->mode()));
    RefPtr<ArrayBuffer> buffer = typedArray->possiblySharedBuffer();
    if (!buffer)
        return std::nullopt;

    size_t bufferByteLength = getter(*buffer);
    size_t byteOffset = typedArray->byteOffsetRaw();
    return (bufferByteLength - byteOffset) >> logElementSize(typedArray->type());
}

template<typename Getter>
size_t integerIndexedObjectByteLength(JSArrayBufferView* typedArray, Getter& getter)
{
    std::optional<size_t> length = integerIndexedObjectLength(typedArray, getter);
    if (!length || !length.value())
        return 0;

    if (!typedArray->isAutoLength()) [[likely]]
        return typedArray->byteLengthRaw();

    return length.value() << logElementSize(typedArray->type());
}

inline JSArrayBufferView* validateTypedArray(JSGlobalObject* globalObject, JSArrayBufferView* typedArray)
{
    // https://tc39.es/ecma262/#sec-validatetypedarray
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isTypedView(typedArray->type())) [[unlikely]] {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    if (isIntegerIndexedObjectOutOfBounds(typedArray, getter)) [[unlikely]] {
        throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return nullptr;
    }
    return typedArray;
}

inline JSArrayBufferView* validateTypedArray(JSGlobalObject* globalObject, JSValue typedArrayValue)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!typedArrayValue.isCell()) [[unlikely]] {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    JSCell* typedArrayCell = typedArrayValue.asCell();
    if (!isTypedView(typedArrayCell->type())) [[unlikely]] {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, validateTypedArray(globalObject, uncheckedDowncast<JSArrayBufferView>(typedArrayCell)));
}

} // namespace JSC
