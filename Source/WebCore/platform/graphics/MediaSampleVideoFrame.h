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

#if ENABLE(VIDEO)
#include "ImageOrientation.h"

#if USE(AVFOUNDATION)
#include <wtf/RetainPtr.h>

using CVPixelBufferRef = struct __CVBuffer*;
#endif

namespace WebCore {

class MediaSampleVideoFrame {
public:
    MediaSampleVideoFrame() = delete;
#if USE(AVFOUNDATION)
    WEBCORE_EXPORT MediaSampleVideoFrame(RetainPtr<CVPixelBufferRef>&&, ImageOrientation);
#endif

    ImageOrientation orientation() const { return m_orientation; }

#if USE(AVFOUNDATION)
    CVPixelBufferRef pixelBuffer() const { return m_pixelBuffer.get(); }
#endif

    bool operator==(const MediaSampleVideoFrame& other) const;

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<MediaSampleVideoFrame> decode(Decoder&);

private:
#if USE(AVFOUNDATION)
    RetainPtr<CVPixelBufferRef> m_pixelBuffer;
#endif
    ImageOrientation m_orientation;
};

inline bool MediaSampleVideoFrame::operator==(const MediaSampleVideoFrame& other) const
{
#if USE(AVFOUNDATION)
    if (m_pixelBuffer != other.m_pixelBuffer)
        return false;
#endif
    return ImageOrientation::Orientation(other.m_orientation) == ImageOrientation::Orientation(m_orientation);
}

template<typename Encoder> void MediaSampleVideoFrame::encode(Encoder& encoder) const
{
#if USE(AVFOUNDATION)
    encoder << m_pixelBuffer;
    encoder << ImageOrientation::Orientation(m_orientation);
#else
    UNUSED_PARAM(encoder);
#endif
}

template<typename Decoder> std::optional<MediaSampleVideoFrame> MediaSampleVideoFrame::decode(Decoder& decoder)
{
#if USE(AVFOUNDATION)
    std::optional<RetainPtr<CVPixelBufferRef>> pixelBuffer;
    decoder >> pixelBuffer;
    if (!pixelBuffer)
        return std::nullopt;
    std::optional<ImageOrientation::Orientation> orientation;
    decoder >> orientation;
    if (!orientation)
        return std::nullopt;
    return MediaSampleVideoFrame { WTFMove(*pixelBuffer), *orientation };
#else
    UNUSED_PARAM(decoder);
    return std::nullopt;
#endif
}

}
#endif
