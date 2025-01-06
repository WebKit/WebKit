/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "Float16ArrayPixelBuffer.h"

#if HAVE(HDR_SUPPORT)

#include <JavaScriptCore/TypedArrayInlines.h>

namespace WebCore {

Ref<Float16ArrayPixelBuffer> Float16ArrayPixelBuffer::create(const PixelBufferFormat& format, const IntSize& size, JSC::Float16Array& data)
{
    ASSERT(format.pixelFormat == PixelFormat::RGBA16F);
    return adoptRef(*new Float16ArrayPixelBuffer(format, size, { data }));
}

std::optional<Ref<Float16ArrayPixelBuffer>> Float16ArrayPixelBuffer::create(const PixelBufferFormat& format, const IntSize& size, std::span<const Float16> data)
{
    if (format.pixelFormat != PixelFormat::RGBA16F) {
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

    auto buffer = JSC::Float16Array::tryCreate(data);
    if (!buffer) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return Float16ArrayPixelBuffer::create(format, size, buffer.releaseNonNull());
}

RefPtr<Float16ArrayPixelBuffer> Float16ArrayPixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    if (format.pixelFormat != PixelFormat::RGBA16F) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;

    auto data = JSC::Float16Array::tryCreateUninitialized(bufferSize / sizeof(Float16));
    if (!data)
        return nullptr;

    return create(format, size, data.releaseNonNull());
}

RefPtr<Float16ArrayPixelBuffer> Float16ArrayPixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::ArrayBuffer>&& arrayBuffer)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    if (format.pixelFormat != PixelFormat::RGBA16F) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;
    if (bufferSize != arrayBuffer->byteLength())
        return nullptr;

    Ref data = JSC::Float16Array::create(WTFMove(arrayBuffer));
    return create(format, size, WTFMove(data));
}

Float16ArrayPixelBuffer::Float16ArrayPixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::Float16Array>&& data)
    : PixelBuffer(format, size, data->mutableSpan())
    , m_data(WTFMove(data))
{
}

RefPtr<PixelBuffer> Float16ArrayPixelBuffer::createScratchPixelBuffer(const IntSize& size) const
{
    return Float16ArrayPixelBuffer::tryCreate(m_format, size);
}

std::span<const uint8_t> Float16ArrayPixelBuffer::span() const
{
    Ref data = m_data;
    ASSERT(data->byteLength() == (m_size.area() * 4 * sizeof(Float16)));
    return data->span();
}

} // namespace WebCore

#endif // HAVE(HDR_SUPPORT)
