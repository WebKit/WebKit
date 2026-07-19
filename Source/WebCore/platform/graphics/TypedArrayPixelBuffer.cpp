/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TypedArrayPixelBuffer.h"

#include <JavaScriptCore/TypedArrayInlines.h>

namespace WebCore {

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
Ref<TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>> TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::create(const PixelBufferFormat& format, const IntSize& size, JSCTypedArray& data)
{
    ASSERT(((format.pixelFormat == pixelFormats) || ...));
    return adoptRef(*new TypedArrayPixelBuffer(format, size, { data }));
}

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
std::optional<Ref<TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>>> TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::create(const PixelBufferFormat& format, const IntSize& size, std::span<const ComponentType> data)
{
    if (!((format.pixelFormat == pixelFormats) || ...)) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto computedBufferSize = PixelBuffer::computeBufferSize(format.pixelFormat, size);
    if (computedBufferSize.hasOverflowed()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    if (data.size_bytes() != computedBufferSize.value()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    auto buffer = JSCTypedArray::tryCreate(data);
    if (!buffer) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return TypedArrayPixelBuffer::create(format, size, buffer.releaseNonNull());
}

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
RefPtr<TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>> TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    if (!((format.pixelFormat == pixelFormats) || ...)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;

    auto data = JSCTypedArray::tryCreateUninitialized(bufferSize / sizeof(ComponentType));
    if (!data)
        return nullptr;

    return create(format, size, data.releaseNonNull());
}

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
RefPtr<TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>> TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::tryCreate(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::ArrayBuffer>&& arrayBuffer)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    if (!((format.pixelFormat == pixelFormats) || ...)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;
    if (bufferSize != arrayBuffer->byteLength())
        return nullptr;

    Ref data = JSCTypedArray::create(WTF::move(arrayBuffer));
    return create(format, size, WTF::move(data));
}

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::TypedArrayPixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSCTypedArray>&& data)
    : ArrayPixelBuffer(format, size, WTF::move(data))
{
}

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
RefPtr<PixelBuffer> TypedArrayPixelBuffer<pixelBufferType, JSCTypedArrayAdaptor, pixelFormats...>::createScratchPixelBuffer(const IntSize& size) const
{
    return TypedArrayPixelBuffer::tryCreate(format(), size);
}

template class TypedArrayPixelBuffer<PixelBuffer::Type::ByteArray, JSC::Uint8ClampedAdaptor, PixelFormat::RGBA8, PixelFormat::BGRA8>;

#if ENABLE(PIXEL_FORMAT_RGBA16F)
template class TypedArrayPixelBuffer<PixelBuffer::Type::Float16Array, JSC::Float16Adaptor, PixelFormat::RGBA16F>;
#endif

} // namespace WebCore
