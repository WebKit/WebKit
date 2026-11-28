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
#include <wtf/Borrow.h>

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
        && bufferType != webrtc::VideoFrameBuffer::Type::kI210
        && bufferType != webrtc::VideoFrameBuffer::Type::kI444
        && bufferType != webrtc::VideoFrameBuffer::Type::kI410)
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
    bool isFullRange = this->colorSpace().fullRange.value_or(false);
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI422:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_422YpCbCr8BiPlanarFullRange : kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI010:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI210:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_422YpCbCr10BiPlanarFullRange : kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI444:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_444YpCbCr8BiPlanarFullRange : kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
        break;
    case webrtc::VideoFrameBuffer::Type::kI410:
        m_videoPixelFormat = isFullRange ? kCVPixelFormatType_444YpCbCr10BiPlanarFullRange : kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
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

template<typename Sample>
static std::optional<PlanarImageConverter::SourcePlane> bufferPlane(const Sample* data, size_t sampleStride, size_t width, size_t height, PlanarImageConverter::SampleFormat format)
{
    return PlanarImageConverter::makePlane(unsafeMakeSpan(data, (height - 1) * sampleStride + width), 0, sampleStride, width, height, format);
}

static bool copyBufferPlane(const std::optional<PlanarImageConverter::SourcePlane>& source, std::span<uint8_t> destination, const ComputedPlaneLayout& layout, PlanarImageConverter::SampleFormat format)
{
    auto destinationPlane = PlanarImageConverter::makePlane(destination, layout.destinationOffset, layout.destinationStride, layout.sourceWidthBytes / format.sampleSize(), layout.sourceHeight, format);
    return source && destinationPlane && PlanarImageConverter::copyPlane(*source, *destinationPlane);
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I420BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical, PlanarImageConverter::eightBit };
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I010BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::HorizontalAndVertical, PlanarImageConverter::tenBitLSB };
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I422BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::Horizontal, PlanarImageConverter::eightBit };
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I210BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::Horizontal, PlanarImageConverter::tenBitLSB };
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I444BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::None, PlanarImageConverter::eightBit };
}

static PlanarYUVFormatLayout bufferLayout(const webrtc::I410BufferInterface&)
{
    return { PlanarImageConverter::ChromaSubsampling::None, PlanarImageConverter::tenBitLSB };
}

template<typename Buffer>
static bool copyBufferPlanes(const Buffer& buffer, std::span<uint8_t> destination, VideoPixelFormat format)
{
    auto layout = planarYUVFormatLayout(format);
    auto layouts = VideoFrame::planarBufferLayout({ buffer.width(), buffer.height() }, format);
    auto source = bufferLayout(buffer);
    if (!layout || layout->subsampling != source.subsampling || layouts.size() != 3)
        return false;

    size_t width = buffer.width();
    size_t height = buffer.height();
    size_t chromaWidth = PlanarImageConverter::chromaWidth(source.subsampling, width);
    size_t chromaHeight = PlanarImageConverter::chromaHeight(source.subsampling, height);
    Borrow borrowedLayouts = layouts;
    return copyBufferPlane(bufferPlane(buffer.DataY(), buffer.StrideY(), width, height, source.format), destination, borrowedLayouts[0], layout->format)
        && copyBufferPlane(bufferPlane(buffer.DataU(), buffer.StrideU(), chromaWidth, chromaHeight, source.format), destination, borrowedLayouts[1], layout->format)
        && copyBufferPlane(bufferPlane(buffer.DataV(), buffer.StrideV(), chromaWidth, chromaHeight, source.format), destination, borrowedLayouts[2], layout->format);
}

// A buffer already in the requested format is copied directly.
bool VideoFrameLibWebRTC::copyToPlanarYUV(std::span<uint8_t> destination, VideoPixelFormat format)
{
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
        if (format == VideoPixelFormat::I420)
            return copyBufferPlanes(*m_buffer->GetI420(), destination, format);
        break;
    case webrtc::VideoFrameBuffer::Type::kI010:
        if (format == VideoPixelFormat::I420P10)
            return copyBufferPlanes(*m_buffer->GetI010(), destination, format);
        break;
    case webrtc::VideoFrameBuffer::Type::kI422:
        if (format == VideoPixelFormat::I422)
            return copyBufferPlanes(*m_buffer->GetI422(), destination, format);
        break;
    case webrtc::VideoFrameBuffer::Type::kI210:
        if (format == VideoPixelFormat::I422P10)
            return copyBufferPlanes(*m_buffer->GetI210(), destination, format);
        break;
    case webrtc::VideoFrameBuffer::Type::kI444:
        if (format == VideoPixelFormat::I444)
            return copyBufferPlanes(*m_buffer->GetI444(), destination, format);
        break;
    case webrtc::VideoFrameBuffer::Type::kI410:
        if (format == VideoPixelFormat::I444P10)
            return copyBufferPlanes(*m_buffer->GetI410(), destination, format);
        break;
    default:
        break;
    }
    return VideoFrame::copyToPlanarYUV(destination, format);
}

template<typename Buffer>
static bool copyBufferLumaPlane(const Buffer& buffer, std::span<uint8_t> destination, PlanarImageConverter::SampleFormat format)
{
    size_t width = buffer.width();
    size_t height = buffer.height();
    size_t rowBytes = width * format.sampleSize();
    return copyBufferPlane(bufferPlane(buffer.DataY(), buffer.StrideY(), width, height, bufferLayout(buffer).format), destination, { .destinationOffset = 0, .destinationStride = rowBytes, .sourceHeight = height, .sourceWidthBytes = rowBytes }, format);
}

bool VideoFrameLibWebRTC::copyLumaPlane(std::span<uint8_t> destination, PlanarImageConverter::SampleFormat format)
{
    switch (m_buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420:
        return copyBufferLumaPlane(*m_buffer->GetI420(), destination, format);
    case webrtc::VideoFrameBuffer::Type::kI010:
        return copyBufferLumaPlane(*m_buffer->GetI010(), destination, format);
    case webrtc::VideoFrameBuffer::Type::kI422:
        return copyBufferLumaPlane(*m_buffer->GetI422(), destination, format);
    case webrtc::VideoFrameBuffer::Type::kI210:
        return copyBufferLumaPlane(*m_buffer->GetI210(), destination, format);
    case webrtc::VideoFrameBuffer::Type::kI444:
        return copyBufferLumaPlane(*m_buffer->GetI444(), destination, format);
    case webrtc::VideoFrameBuffer::Type::kI410:
        return copyBufferLumaPlane(*m_buffer->GetI410(), destination, format);
    default:
        return VideoFrame::copyLumaPlane(destination, format);
    }
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
