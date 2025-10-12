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
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

bool PixelBuffer::supportedPixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
#endif
        return true;

    case PixelFormat::BGRX8:
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
#endif
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static CheckedUint32 mustFitInInt32(CheckedUint32 uint32)
{
    if (!uint32.hasOverflowed() && !isInBounds<int32_t>(uint32.value()))
        uint32.overflowed();
    return uint32;
}

static CheckedUint32 computeRawPixelCount(const IntSize& size)
{
    return CheckedUint32 { size.width() } * size.height();
}

static CheckedUint32 computeRawPixelComponentCount(PixelFormat pixelFormat, const IntSize& size)
{
    ASSERT(PixelBuffer::supportedPixelFormat(pixelFormat));
    return computeRawPixelCount(size) * PixelBuffer::componentsPerPixel(pixelFormat);
}

CheckedUint32 PixelBuffer::computePixelCount(const IntSize& size)
{
    return mustFitInInt32(computeRawPixelCount(size));
}

CheckedUint32 PixelBuffer::computePixelComponentCount(PixelFormat pixelFormat, const IntSize& size)
{
    return mustFitInInt32(computeRawPixelComponentCount(pixelFormat, size));
}

CheckedUint32 PixelBuffer::computeBufferSize(PixelFormat pixelFormat, const IntSize& size)
{
    return mustFitInInt32(computeRawPixelComponentCount(pixelFormat, size) * bytesPerPixelComponent(pixelFormat));
}

PixelBuffer::PixelBuffer(const PixelBufferFormat& format, const IntSize& size, std::span<uint8_t> bytes)
    : m_format(format)
    , m_size(size)
    , m_bytes(bytes)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(computeBufferSize(format.pixelFormat, size).value() <= bytes.size());
}

PixelBuffer::~PixelBuffer() = default;

bool PixelBuffer::setRange(std::span<const uint8_t> data, size_t byteOffset)
{
    if (!isSumSmallerThanOrEqual(byteOffset, data.size(), m_bytes.size()))
        return false;

    memmoveSpan(m_bytes.subspan(byteOffset), data);
    return true;
}

bool PixelBuffer::zeroRange(size_t byteOffset, size_t rangeByteLength)
{
    if (!isSumSmallerThanOrEqual(byteOffset, rangeByteLength, m_bytes.size()))
        return false;

    zeroSpan(m_bytes.subspan(byteOffset, rangeByteLength));
    return true;
}

uint8_t PixelBuffer::item(size_t index) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_bytes.size());
    return m_bytes[index];
}

void PixelBuffer::set(size_t index, double value)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_bytes.size());
    m_bytes[index] = JSC::Uint8ClampedAdaptor::toNativeFromDouble(value);
}

} // namespace WebCore
