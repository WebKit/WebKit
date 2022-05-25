/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "PixelBuffer.h"

#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

bool PixelBuffer::supportedPixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
        return true;

    case PixelFormat::RGB10:
    case PixelFormat::RGB10A8:
        return false;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

CheckedUint32 PixelBuffer::computeBufferSize(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT_UNUSED(format, supportedPixelFormat(format.pixelFormat));

    constexpr unsigned bytesPerPixel = 4;

    return size.area<RecordOverflow>() * bytesPerPixel;
}

std::optional<PixelBuffer> PixelBuffer::tryCreateForDecoding(const PixelBufferFormat& format, const IntSize& size, unsigned dataByteLength)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));
    ASSERT(computeBufferSize(format, size) == dataByteLength);

    auto pixelArray = Uint8ClampedArray::tryCreateUninitialized(dataByteLength);
    if (!pixelArray)
        return std::nullopt;
    return { { format, size, pixelArray.releaseNonNull() } };
}

std::optional<PixelBuffer> PixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto bufferSize = computeBufferSize(format, size);
    if (bufferSize.hasOverflowed())
        return std::nullopt;
    if (bufferSize > std::numeric_limits<int32_t>::max())
        return std::nullopt;
    auto pixelArray = Uint8ClampedArray::tryCreateUninitialized(bufferSize);
    if (!pixelArray)
        return std::nullopt;
    return { { format, size, pixelArray.releaseNonNull() } };
}

std::optional<PixelBuffer> PixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::ArrayBuffer>&& arrayBuffer)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto bufferSize = computeBufferSize(format, size);
    if (bufferSize.hasOverflowed())
        return std::nullopt;
    if (bufferSize != arrayBuffer->byteLength())
        return std::nullopt;
    auto pixelArray = Uint8ClampedArray::tryCreate(WTFMove(arrayBuffer), 0, bufferSize);
    if (!pixelArray)
        return std::nullopt;
    return { { format, size, pixelArray.releaseNonNull() } };
}

PixelBuffer::PixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::Uint8ClampedArray>&& data)
    : m_format { format }
    , m_size { size }
    , m_data { WTFMove(data) }
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((m_size.area() * 4) <= m_data->length());
}

PixelBuffer::PixelBuffer(const PixelBufferFormat& format, const IntSize& size, JSC::Uint8ClampedArray& data)
    : m_format { format }
    , m_size { size }
    , m_data { data }
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((m_size.area() * 4) <= m_data->length());
}

PixelBuffer::~PixelBuffer() = default;

PixelBuffer PixelBuffer::deepClone() const
{
    return { m_format, m_size, Uint8ClampedArray::create(m_data->data(), m_data->length()) };
}

uint8_t* PixelBuffer::bytes() const
{
    return m_data->data();
}

size_t PixelBuffer::sizeInBytes() const
{
    ASSERT(m_data->byteLength() == m_size.area() * 4);
    return m_data->byteLength();
}

bool PixelBuffer::setRange(const uint8_t* data, size_t dataByteLength, size_t byteOffset)
{
    if (!isSumSmallerThanOrEqual(byteOffset, dataByteLength, sizeInBytes()))
        return false;

    memmove(bytes() + byteOffset, data, dataByteLength);
    return true;
}

bool PixelBuffer::zeroRange(size_t byteOffset, size_t rangeByteLength)
{
    if (!isSumSmallerThanOrEqual(byteOffset, rangeByteLength, sizeInBytes()))
        return false;

    memset(bytes() + byteOffset, 0, rangeByteLength);
    return true;
}

uint8_t PixelBuffer::item(size_t index) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < sizeInBytes());
    return bytes()[index];
}

void PixelBuffer::set(size_t index, double value)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < sizeInBytes());
    bytes()[index] = JSC::Uint8ClampedAdaptor::toNativeFromDouble(value);
}

std::optional<PixelBuffer> PixelBuffer::createScratchPixelBuffer(const IntSize& size) const
{
    return PixelBuffer::tryCreate(m_format, size);
}

TextStream& operator<<(TextStream& ts, const PixelBuffer& pixelBuffer)
{
    return ts << &pixelBuffer.data() << "format ( " << pixelBuffer.format() << ")";
}

} // namespace WebCore

