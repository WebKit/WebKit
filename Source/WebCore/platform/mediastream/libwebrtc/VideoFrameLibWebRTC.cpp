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

#include "LibWebRTCRefWrappers.h"
#include "NativeImage.h"
#include "PixelBufferConformerCV.h"

#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static PlatformVideoColorSpace defaultVPXColorSpace()
{
    return { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Bt709, PlatformVideoMatrixCoefficients::Bt709, false };
}

RefPtr<VideoFrameLibWebRTC> VideoFrameLibWebRTC::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, std::optional<PlatformVideoColorSpace>&& colorSpace, Ref<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
{
    auto bufferType = buffer->type();
    if (bufferType != webrtc::VideoFrameBuffer::Type::kI420
        && bufferType != webrtc::VideoFrameBuffer::Type::kI010
        && bufferType != webrtc::VideoFrameBuffer::Type::kI422
        && bufferType != webrtc::VideoFrameBuffer::Type::kI210)
        return nullptr;

    auto finalColorSpace = WTFMove(colorSpace).value_or(defaultVPXColorSpace());
    return adoptRef(*new VideoFrameLibWebRTC(presentationTime, isMirrored, rotation, WTFMove(finalColorSpace), WTFMove(buffer), WTFMove(conversionCallback)));
}

VideoFrameLibWebRTC::VideoFrameLibWebRTC(MediaTime presentationTime, bool isMirrored, Rotation rotation, PlatformVideoColorSpace&& colorSpace, Ref<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
    : VideoFrame(presentationTime, isMirrored, rotation, WTFMove(colorSpace))
    , m_buffer(WTFMove(buffer))
    , m_size(m_buffer->width(),  m_buffer->height())
    , m_conversionCallback(WTFMove(conversionCallback))
{
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
    case webrtc::VideoFrameBuffer::Type::kI422:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI010:
        m_videoPixelFormat = kCVPixelFormatType_420YpCbCr10BiPlanarFullRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI210:
        m_videoPixelFormat = kCVPixelFormatType_422YpCbCr10BiPlanarFullRange;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

CVPixelBufferRef VideoFrameLibWebRTC::pixelBuffer() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=302866 VideoFrameLibWebRTC should construct its CVPixelBuffer from the LibWebRTC buffer.
    Locker locker { m_pixelBufferLock };
    if (!m_pixelBuffer && m_conversionCallback)
        m_pixelBuffer = std::exchange(m_conversionCallback, { })(m_buffer.get());
    return m_pixelBuffer.get();
}

RefPtr<NativeImage> VideoFrameLibWebRTC::copyNativeImage() const
{
    // FIXME: It is not efficient to create a conformer everytime. We might want to make it more efficient, for instance by storing it in GraphicsContext.
    auto conformer = makeUnique<PixelBufferConformerCV>(kCVPixelFormatType_32BGRA);
    return NativeImage::create(conformer->createImageFromPixelBuffer(protectedPixelBuffer().get()));
}

Ref<VideoFrame> VideoFrameLibWebRTC::clone()
{
    return adoptRef(*new VideoFrameLibWebRTC(presentationTime(), isMirrored(), rotation(), PlatformVideoColorSpace { colorSpace() }, m_buffer.get(), ConversionCallback { m_conversionCallback }));
}

}
#endif // PLATFORM(COCOA) && USE(LIBWEBRTC)
