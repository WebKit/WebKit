/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

Checked<unsigned, RecordOverflow> PixelBuffer::computeBufferSize(const PixelBufferFormat& format, const IntSize& size)
{
    // NOTE: Only 8-bit formats are currently supported.
    ASSERT_UNUSED(format, format.pixelFormat == PixelFormat::RGBA8 || format.pixelFormat == PixelFormat::BGRA8);

    constexpr unsigned bytesPerPixel = 4;

    return size.area<RecordOverflow>() * bytesPerPixel;
}

Optional<PixelBuffer> PixelBuffer::tryCreateForDecoding(const PixelBufferFormat& format, const IntSize& size, unsigned dataByteLength)
{
    ASSERT(format.pixelFormat == PixelFormat::RGBA8 || format.pixelFormat == PixelFormat::BGRA8);
    ASSERT(computeBufferSize(format, size).unsafeGet() == dataByteLength);

    auto pixelArray = Uint8ClampedArray::tryCreateUninitialized(dataByteLength);
    if (!pixelArray)
        return WTF::nullopt;
    return { { format, size, pixelArray.releaseNonNull() } };
}

Optional<PixelBuffer> PixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    // NOTE: Only 8-bit formats are currently supported.
    ASSERT(format.pixelFormat == PixelFormat::RGBA8 || format.pixelFormat == PixelFormat::BGRA8);

    auto bufferSize = computeBufferSize(format, size);
    if (bufferSize.hasOverflowed())
        return WTF::nullopt;
    auto pixelArray = Uint8ClampedArray::tryCreateUninitialized(bufferSize.unsafeGet());
    if (!pixelArray)
        return WTF::nullopt;
    return { { format, size, pixelArray.releaseNonNull() } };
}

PixelBuffer::PixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::Uint8ClampedArray>&& data)
    : m_format { format }
    , m_size { size }
    , m_data { WTFMove(data) }
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((m_size.area() * 4).unsafeGet() <= m_data->length());
}

PixelBuffer::~PixelBuffer() = default;

PixelBuffer PixelBuffer::deepClone() const
{
    return { m_format, m_size, Uint8ClampedArray::create(m_data->data(), m_data->length()) };
}

TextStream& operator<<(TextStream& ts, const PixelBuffer& pixelBuffer)
{
    return ts << &pixelBuffer.data() << "format ( " << pixelBuffer.format() << ")";
}

}
