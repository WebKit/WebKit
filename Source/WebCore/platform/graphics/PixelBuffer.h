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

#pragma once

#include "AlphaPremultiplication.h"
#include "DestinationColorSpace.h"
#include "IntSize.h"
#include "PixelBufferFormat.h"
#include "PixelFormat.h"
#include <JavaScriptCore/Uint8ClampedArray.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class PixelBuffer {
    WTF_MAKE_NONCOPYABLE(PixelBuffer);
public:
    static bool supportedPixelFormat(PixelFormat);

    WEBCORE_EXPORT static std::optional<PixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);
    WEBCORE_EXPORT static std::optional<PixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&, Ref<JSC::ArrayBuffer>&&);

    PixelBuffer(const PixelBufferFormat&, const IntSize&, Ref<JSC::Uint8ClampedArray>&&);
    PixelBuffer(const PixelBufferFormat&, const IntSize&, JSC::Uint8ClampedArray&);
    WEBCORE_EXPORT ~PixelBuffer();

    PixelBuffer(PixelBuffer&&) = default;
    PixelBuffer& operator=(PixelBuffer&&) = default;

    const PixelBufferFormat& format() const { return m_format; }
    const IntSize& size() const { return m_size; }
    JSC::Uint8ClampedArray& data() const { return m_data.get(); }

    Ref<JSC::Uint8ClampedArray>&& takeData() { return WTFMove(m_data); }

    PixelBuffer deepClone() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PixelBuffer> decode(Decoder&);

private:
    WEBCORE_EXPORT static std::optional<PixelBuffer> tryCreateForDecoding(const PixelBufferFormat&, const IntSize&, unsigned dataByteLength);

    WEBCORE_EXPORT static CheckedUint32 computeBufferSize(const PixelBufferFormat&, const IntSize&);

    PixelBufferFormat m_format;
    IntSize m_size;
    Ref<JSC::Uint8ClampedArray> m_data;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PixelBuffer&);

template<class Encoder> void PixelBuffer::encode(Encoder& encoder) const
{
    ASSERT(m_data->byteLength() == (m_size.area() * 4));

    encoder << m_format;
    encoder << m_size;
    encoder.encodeFixedLengthData(m_data->data(), m_data->byteLength(), 1);
}

template<class Decoder> std::optional<PixelBuffer> PixelBuffer::decode(Decoder& decoder)
{
    std::optional<PixelBufferFormat> format;
    decoder >> format;
    if (!format)
        return std::nullopt;

    // FIXME: Support non-8 bit formats.
    if (!(format->pixelFormat == PixelFormat::RGBA8 || format->pixelFormat == PixelFormat::BGRA8))
        return std::nullopt;

    std::optional<IntSize> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    auto computedBufferSize = PixelBuffer::computeBufferSize(*format, *size);
    if (computedBufferSize.hasOverflowed())
        return std::nullopt;

    auto bufferSize = computedBufferSize;
    if (!decoder.template bufferIsLargeEnoughToContain<uint8_t>(bufferSize))
        return std::nullopt;

    auto result = PixelBuffer::tryCreateForDecoding(WTFMove(*format), *size, bufferSize);
    if (!result)
        return std::nullopt;

    if (!decoder.decodeFixedLengthData(result->m_data->data(), result->m_data->byteLength(), 1))
        return std::nullopt;

    return result;
}

}
