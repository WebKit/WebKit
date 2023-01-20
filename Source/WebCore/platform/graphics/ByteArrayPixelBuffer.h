/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PixelBuffer.h"
#include <JavaScriptCore/Uint8ClampedArray.h>

namespace WebCore {

class ByteArrayPixelBuffer : public PixelBuffer {
public:
    WEBCORE_EXPORT static Ref<ByteArrayPixelBuffer> create(const PixelBufferFormat&, const IntSize&, JSC::Uint8ClampedArray&);

    WEBCORE_EXPORT static RefPtr<ByteArrayPixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);
    WEBCORE_EXPORT static RefPtr<ByteArrayPixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&, Ref<JSC::ArrayBuffer>&&);

    JSC::Uint8ClampedArray& data() const { return m_data.get(); }
    Ref<JSC::Uint8ClampedArray>&& takeData() { return WTFMove(m_data); }

    RefPtr<PixelBuffer> createScratchPixelBuffer(const IntSize&) const override;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<ByteArrayPixelBuffer>> decode(Decoder&);

private:
    ByteArrayPixelBuffer(const PixelBufferFormat&, const IntSize&, Ref<JSC::Uint8ClampedArray>&&);

    bool isByteArrayPixelBuffer() const override { return true; }

    Ref<JSC::Uint8ClampedArray> m_data;
};

template<class Encoder> void ByteArrayPixelBuffer::encode(Encoder& encoder) const
{
    ASSERT(m_data->byteLength() == (m_size.area() * 4));

    encoder << m_format;
    encoder << m_size;
    encoder << Span { m_data->data(), m_data->byteLength() };
}

template<class Decoder> std::optional<Ref<ByteArrayPixelBuffer>> ByteArrayPixelBuffer::decode(Decoder& decoder)
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

    std::optional<Span<const uint8_t>> data;
    decoder >> data;
    if (!data || data->size_bytes() != computedBufferSize.value())
        return std::nullopt;

    auto buffer = Uint8ClampedArray::tryCreate(data->data(), data->size_bytes());
    if (!buffer)
        return std::nullopt;
    return ByteArrayPixelBuffer::create(*format, *size, buffer.releaseNonNull());
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ByteArrayPixelBuffer)
    static bool isType(const WebCore::PixelBuffer& pixelBuffer) { return pixelBuffer.isByteArrayPixelBuffer(); }
SPECIALIZE_TYPE_TRAITS_END()
