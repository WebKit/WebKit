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

    case PixelFormat::BGRX8:
    case PixelFormat::RGB10:
    case PixelFormat::RGB10A8:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

CheckedUint32 PixelBuffer::computeBufferSize(PixelFormat pixelFormat, const IntSize& size)
{
    ASSERT_UNUSED(pixelFormat, supportedPixelFormat(pixelFormat));
    constexpr unsigned bytesPerPixel = 4;
    auto bufferSize = CheckedUint32 { size.width() } * size.height() * bytesPerPixel;
    if (!bufferSize.hasOverflowed() && bufferSize.value() > std::numeric_limits<int32_t>::max())
        bufferSize.overflowed();
    return bufferSize;

}

PixelBuffer::PixelBuffer(const PixelBufferFormat& format, const IntSize& size, uint8_t* bytes, size_t sizeInBytes)
    : m_format(format)
    , m_size(size)
    , m_bytes(bytes)
    , m_sizeInBytes(sizeInBytes)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((m_size.area() * 4) <= m_sizeInBytes);
}

PixelBuffer::~PixelBuffer() = default;

bool PixelBuffer::setRange(const uint8_t* data, size_t dataByteLength, size_t byteOffset)
{
    if (!isSumSmallerThanOrEqual(byteOffset, dataByteLength, m_sizeInBytes))
        return false;

    memmove(m_bytes + byteOffset, data, dataByteLength);
    return true;
}

bool PixelBuffer::zeroRange(size_t byteOffset, size_t rangeByteLength)
{
    if (!isSumSmallerThanOrEqual(byteOffset, rangeByteLength, m_sizeInBytes))
        return false;

    memset(m_bytes + byteOffset, 0, rangeByteLength);
    return true;
}

uint8_t PixelBuffer::item(size_t index) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_sizeInBytes);
    return m_bytes[index];
}

void PixelBuffer::set(size_t index, double value)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_sizeInBytes);
    m_bytes[index] = JSC::Uint8ClampedAdaptor::toNativeFromDouble(value);
}

} // namespace WebCore
