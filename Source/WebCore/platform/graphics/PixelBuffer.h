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
#include "ColorSpace.h"
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

    WEBCORE_EXPORT static Optional<PixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);
    WEBCORE_EXPORT static Optional<PixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&, Ref<JSC::ArrayBuffer>&&);

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
    template<class Decoder> static Optional<PixelBuffer> decode(Decoder&);

private:
    WEBCORE_EXPORT static Optional<PixelBuffer> tryCreateForDecoding(const PixelBufferFormat&, const IntSize&, unsigned dataByteLength);

    WEBCORE_EXPORT static Checked<unsigned, RecordOverflow> computeBufferSize(const PixelBufferFormat&, const IntSize&);

    PixelBufferFormat m_format;
    IntSize m_size;
    Ref<JSC::Uint8ClampedArray> m_data;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PixelBuffer&);

template<class Encoder> void PixelBuffer::encode(Encoder& encoder) const
{
    ASSERT(m_data->byteLength() == (m_size.area().unsafeGet() * 4));

    encoder << m_format;
    encoder << m_size;
    encoder.encodeFixedLengthData(m_data->data(), m_data->byteLength(), 1);
}

template<class Decoder> Optional<PixelBuffer> PixelBuffer::decode(Decoder& decoder)
{
    PixelBufferFormat format;
    if (!decoder.decode(format))
        return WTF::nullopt;

    // FIXME: Support non-8 bit formats.
    if (!(format.pixelFormat == PixelFormat::RGBA8 || format.pixelFormat == PixelFormat::BGRA8))
        return WTF::nullopt;

    IntSize size;
    if (!decoder.decode(size))
        return WTF::nullopt;

    auto computedBufferSize = PixelBuffer::computeBufferSize(format, size);
    if (computedBufferSize.hasOverflowed())
        return WTF::nullopt;

    auto bufferSize = computedBufferSize.unsafeGet();
    if (!decoder.template bufferIsLargeEnoughToContain<uint8_t>(bufferSize))
        return WTF::nullopt;

    auto result = PixelBuffer::tryCreateForDecoding(format, size, bufferSize);
    if (!result)
        return WTF::nullopt;

    if (!decoder.decodeFixedLengthData(result->m_data->data(), result->m_data->byteLength(), 1))
        return WTF::nullopt;

    return result;
}

}
