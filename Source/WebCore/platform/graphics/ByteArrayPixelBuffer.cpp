/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ByteArrayPixelBuffer.h"

#include <JavaScriptCore/TypedArrayInlines.h>

namespace WebCore {

Ref<ByteArrayPixelBuffer> ByteArrayPixelBuffer::create(const PixelBufferFormat& format, const IntSize& size, JSC::Uint8ClampedArray& data)
{
    return adoptRef(*new ByteArrayPixelBuffer(format, size, { data }));
}

std::optional<Ref<ByteArrayPixelBuffer>> ByteArrayPixelBuffer::create(const PixelBufferFormat& format, const IntSize& size, std::span<const uint8_t> data)
{
    // FIXME: Support non-8 bit formats.
    if (!(format.pixelFormat == PixelFormat::RGBA8 || format.pixelFormat == PixelFormat::BGRA8)) {
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

    auto buffer = Uint8ClampedArray::tryCreate(data.data(), data.size_bytes());
    if (!buffer) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return ByteArrayPixelBuffer::create(format, size, buffer.releaseNonNull());
}

RefPtr<ByteArrayPixelBuffer> ByteArrayPixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;

    auto data = Uint8ClampedArray::tryCreateUninitialized(bufferSize);
    if (!data)
        return nullptr;

    return create(format, size, data.releaseNonNull());
}

RefPtr<ByteArrayPixelBuffer> ByteArrayPixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::ArrayBuffer>&& arrayBuffer)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;
    if (bufferSize != arrayBuffer->byteLength())
        return nullptr;

    auto data = Uint8ClampedArray::tryCreate(WTFMove(arrayBuffer), 0, bufferSize);
    if (!data)
        return nullptr;

    return create(format, size, data.releaseNonNull());
}

ByteArrayPixelBuffer::ByteArrayPixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::Uint8ClampedArray>&& data)
    : PixelBuffer(format, size, data->data(), data->byteLength())
    , m_data(WTFMove(data))
{
}

RefPtr<PixelBuffer> ByteArrayPixelBuffer::createScratchPixelBuffer(const IntSize& size) const
{
    return ByteArrayPixelBuffer::tryCreate(m_format, size);
}

std::span<const uint8_t> ByteArrayPixelBuffer::dataSpan() const
{
    ASSERT(m_data->byteLength() == (m_size.area() * 4));
    return { m_data->data(), m_data->byteLength() };
}

} // namespace WebCore
