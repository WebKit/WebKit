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
#include "VideoFrameLibWebRTC.h"

#if PLATFORM(COCOA) && USE(LIBWEBRTC)

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

Ref<VideoFrameLibWebRTC> VideoFrameLibWebRTC::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
{
    return adoptRef(*new VideoFrameLibWebRTC(presentationTime, isMirrored, rotation, WTFMove(buffer), WTFMove(conversionCallback)));
}

VideoFrameLibWebRTC::VideoFrameLibWebRTC(MediaTime presentationTime, bool isMirrored, Rotation rotation, rtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
    : VideoFrame(presentationTime, isMirrored, rotation)
    , m_buffer(WTFMove(buffer))
    , m_size(m_buffer->width(),  m_buffer->height())
    , m_conversionCallback(WTFMove(conversionCallback))
{
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI010:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr10BiPlanarFullRange;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

CVPixelBufferRef VideoFrameLibWebRTC::pixelBuffer() const
{
    Locker locker { m_pixelBufferLock };
    if (!m_pixelBuffer && m_conversionCallback)
        m_pixelBuffer = std::exchange(m_conversionCallback, { })(*m_buffer);
    return m_pixelBuffer.get();
}

}
#endif // PLATFORM(COCOA) && USE(LIBWEBRTC)
