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

#pragma once

#include <WebCore/IntSize.h>
#include <WebCore/PixelBufferFormat.h>
#include <optional>
#include <span>
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

// Type for holding pixel buffers data.
// For functions that source pixel buffers, see PixelBufferSourceView.
class PixelBuffer : public RefCounted<PixelBuffer> {
    WTF_MAKE_NONCOPYABLE(PixelBuffer);
public:
    static constexpr uint32_t bytesPerPixelComponent(PixelFormat);
    static constexpr uint32_t componentsPerPixel(PixelFormat);
    static constexpr uint32_t bytesPerPixel(PixelFormat);

    static CheckedUint32 computePixelCount(const IntSize&);
    static CheckedUint32 computePixelComponentCount(PixelFormat, const IntSize&);
    WEBCORE_EXPORT static CheckedUint32 computeBufferSize(PixelFormat, const IntSize&);

    WEBCORE_EXPORT static bool supportedPixelFormat(PixelFormat);

    WEBCORE_EXPORT virtual ~PixelBuffer();

    const PixelBufferFormat& format() const { return m_format; }
    const IntSize& size() const { return m_size; }

    std::span<uint8_t> bytes() const { return m_bytes; }

    enum class Type {
        ByteArray,
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        Float16Array,
#endif
        Other
    };
    virtual Type type() const { return Type::Other; }
    virtual RefPtr<PixelBuffer> createScratchPixelBuffer(const IntSize&) const = 0;

    bool setRange(std::span<const uint8_t> data, size_t byteOffset);
    WEBCORE_EXPORT bool zeroRange(size_t byteOffset, size_t rangeByteLength);
    void zeroFill() { zeroRange(0, bytes().size()); }

    WEBCORE_EXPORT uint8_t item(size_t index) const;
    void set(size_t index, double value);

protected:
    WEBCORE_EXPORT PixelBuffer(const PixelBufferFormat&, const IntSize&, std::span<uint8_t> bytes);

    PixelBufferFormat m_format;
    IntSize m_size;

    std::span<uint8_t> m_bytes;
};

// Type to use for functions that use the PixelBuffer data as source during the call, but do not store a reference to the object or modify the data.
class PixelBufferSourceView {
public:
    PixelBufferSourceView() = delete;
    PixelBufferSourceView(const PixelBuffer& pixelBuffer)
        : PixelBufferSourceView(pixelBuffer.format(), pixelBuffer.size(), pixelBuffer.bytes())
    {
    }

    static std::optional<PixelBufferSourceView> create(const PixelBufferFormat& format, const IntSize& size, std::span<const uint8_t> bytes)
    {
        if (!PixelBuffer::supportedPixelFormat(format.pixelFormat))
            return std::nullopt;
        auto bufferSize = PixelBuffer::computeBufferSize(format.pixelFormat, size);
        if (bufferSize.hasOverflowed() || bytes.size() != bufferSize)
            return std::nullopt;
        return PixelBufferSourceView(format, size, bytes);
    }

    const PixelBufferFormat& format() const { return m_format; }
    IntSize size() const { return m_size; }
    std::span<const uint8_t> bytes() const LIFETIME_BOUND { return m_bytes; }

private:
    PixelBufferSourceView(const PixelBufferFormat& format, const IntSize& size, std::span<const uint8_t> bytes)
        : m_format(format)
        , m_size(size)
        , m_bytes(bytes)
    {
    }

    PixelBufferFormat m_format;
    IntSize m_size;
    std::span<const uint8_t> m_bytes;
};

constexpr uint32_t PixelBuffer::bytesPerPixelComponent(PixelFormat pixelFormat)
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    return (pixelFormat == PixelFormat::RGBA16F) ? 2 : 1;
#else
    UNUSED_PARAM(pixelFormat);
    return 1;
#endif
}

constexpr uint32_t PixelBuffer::componentsPerPixel(PixelFormat)
{
    return 4;
}

constexpr uint32_t PixelBuffer::bytesPerPixel(PixelFormat pixelFormat)
{
    return bytesPerPixelComponent(pixelFormat) * componentsPerPixel(pixelFormat);
}

} // namespace WebCore
