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
#include "CoreVideoSoftLink.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

NODELETE static PlatformVideoColorSpace defaultVPXColorSpace()
{
    return { .primaries = PlatformVideoColorPrimaries::Bt709, .transfer = PlatformVideoTransferCharacteristics::Bt709, .matrix = PlatformVideoMatrixCoefficients::Bt709, .fullRange = false };
}

RefPtr<VideoFrameLibWebRTC> VideoFrameLibWebRTC::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, std::optional<PlatformVideoColorSpace>&& colorSpace, Ref<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
{
    auto bufferType = buffer->type();
    if (bufferType != webrtc::VideoFrameBuffer::Type::kI420
        && bufferType != webrtc::VideoFrameBuffer::Type::kI010
        && bufferType != webrtc::VideoFrameBuffer::Type::kI422
        && bufferType != webrtc::VideoFrameBuffer::Type::kI210)
        return nullptr;

    auto finalColorSpace = WTF::move(colorSpace).value_or(defaultVPXColorSpace());
    return adoptRef(*new VideoFrameLibWebRTC(presentationTime, isMirrored, rotation, WTF::move(finalColorSpace), WTF::move(buffer), WTF::move(conversionCallback)));
}

VideoFrameLibWebRTC::VideoFrameLibWebRTC(MediaTime presentationTime, bool isMirrored, Rotation rotation, PlatformVideoColorSpace&& colorSpace, Ref<webrtc::VideoFrameBuffer>&& buffer, ConversionCallback&& conversionCallback)
    : VideoFrame(presentationTime, isMirrored, rotation, WTF::move(colorSpace))
    , m_buffer(WTF::move(buffer))
    , m_size(m_buffer->width(),  m_buffer->height())
    , m_conversionCallback(WTF::move(conversionCallback))
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
    Locker locker { m_pixelBufferLock };
    if (!m_pixelBuffer && m_conversionCallback)
        m_pixelBuffer = std::exchange(m_conversionCallback, { })(m_buffer.get());
    return m_pixelBuffer.get();
}

static void copyPlane(std::span<uint8_t> destination, size_t width, size_t height, const uint8_t* source, int sourceStride)
{
    size_t stride = sourceStride;
    auto sourcePlane = unsafeMakeSpan(source, (height - 1) * stride + width);
    for (size_t y = 0; y < height; ++y)
        memcpySpan(destination.subspan(y * width, width), sourcePlane.subspan(y * stride, width));
}

bool VideoFrameLibWebRTC::copyToI420(std::span<uint8_t> destination)
{
    if (m_buffer->type() != webrtc::VideoFrameBuffer::Type::kI420)
        return VideoFrame::copyToI420(destination);

    auto* buffer = m_buffer->GetI420();
    size_t width = buffer->width();
    size_t height = buffer->height();
    size_t chromaWidth = (width + 1) / 2;
    size_t chromaHeight = (height + 1) / 2;
    size_t lumaSize = width * height;
    size_t chromaSize = chromaWidth * chromaHeight;
    copyPlane(destination, width, height, buffer->DataY(), buffer->StrideY());
    copyPlane(destination.subspan(lumaSize), chromaWidth, chromaHeight, buffer->DataU(), buffer->StrideU());
    copyPlane(destination.subspan(lumaSize + chromaSize), chromaWidth, chromaHeight, buffer->DataV(), buffer->StrideV());
    return true;
}

bool VideoFrameLibWebRTC::copyLumaPlane(std::span<uint8_t> destination)
{
    if (m_buffer->type() != webrtc::VideoFrameBuffer::Type::kI420)
        return VideoFrame::copyLumaPlane(destination);

    auto* buffer = m_buffer->GetI420();
    copyPlane(destination, buffer->width(), buffer->height(), buffer->DataY(), buffer->StrideY());
    return true;
}

Ref<VideoFrame> VideoFrameLibWebRTC::clone()
{
    Locker locker { m_pixelBufferLock };
    Ref clone = adoptRef(*new VideoFrameLibWebRTC(presentationTime(), isMirrored(), rotation(), PlatformVideoColorSpace { colorSpace() }, m_buffer.get(), ConversionCallback { m_conversionCallback }));

    Locker cloneLocker { clone->m_pixelBufferLock };
    clone->m_pixelBuffer = m_pixelBuffer;

    return clone;
}

}
#endif // PLATFORM(COCOA) && USE(LIBWEBRTC)
