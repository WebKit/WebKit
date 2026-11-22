/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(COCOA) && USE(LIBWEBRTC) && ENABLE(WEB_RTC)

#import "Helpers/Test.h"
#import <WebCore/LibWebRTCRefWrappers.h>
#import <WebCore/VideoFrameLibWebRTC.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Vector.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/video/i010_buffer.h>
#include <webrtc/api/video/i210_buffer.h>
#include <webrtc/api/video/i420_buffer.h>
#include <webrtc/api/video/i422_buffer.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

using WebCore::ComputedPlaneLayout;
using WebCore::IntSize;
using WebCore::VideoFrame;
using WebCore::VideoFrameLibWebRTC;
using WebCore::VideoPixelFormat;

namespace TestWebKitAPI {

static constexpr int frameWidth = 7;
static constexpr int frameHeight = 5;
static constexpr int chromaWidth = (frameWidth + 1) / 2;
static constexpr int chromaHeight420 = (frameHeight + 1) / 2;

template<typename Sample>
static void fillPlane(Sample* data, int stride, int width, int height, unsigned seed, unsigned maximum)
{
    auto samples = unsafeMakeSpan(data, static_cast<size_t>(stride) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            samples[y * stride + x] = static_cast<Sample>((seed + 37 * y + 11 * x) % (maximum + 1));
    }
}

// The destination plane holds the source samples shifted right by shift bits.
template<typename Sample>
static void expectPlane(std::span<const uint8_t> destination, const ComputedPlaneLayout& layout, const Sample* data, int stride, int width, int height, size_t destinationSampleSize, unsigned shift)
{
    auto samples = unsafeMakeSpan(data, static_cast<size_t>(stride) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = layout.destinationOffset + y * layout.destinationStride + x * destinationSampleSize;
            unsigned value = destinationSampleSize == 1 ? destination[index] : destination[index] | (destination[index + 1] << 8);
            EXPECT_EQ(value, static_cast<unsigned>(samples[y * stride + x] >> shift)) << "x " << x << " y " << y;
        }
    }
}

static RefPtr<VideoFrame> createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer>&& buffer)
{
    return VideoFrameLibWebRTC::create({ }, false, WebCore::VideoFrameRotation::None, { }, toRef(WTF::move(buffer)), { });
}

static std::optional<Vector<uint8_t>> copyToPlanarBuffer(VideoFrame& frame, VideoPixelFormat format)
{
    auto size = VideoFrame::planarBufferSize({ frameWidth, frameHeight }, format);
    if (!size)
        return { };
    Vector<uint8_t> destination(*size);
    if (!frame.copyToPlanarBuffer(destination.mutableSpan(), format, nullptr))
        return { };
    return destination;
}

template<typename Buffer>
static void expectPlanes(const std::optional<Vector<uint8_t>>& destination, VideoPixelFormat format, const Buffer& buffer, int chromaHeight, size_t destinationSampleSize, unsigned shift)
{
    ASSERT_TRUE(destination);
    auto layouts = VideoFrame::planarBufferLayout({ frameWidth, frameHeight }, format);
    ASSERT_EQ(layouts.size(), 3u);
    expectPlane(destination->span(), layouts[0], buffer.DataY(), buffer.StrideY(), frameWidth, frameHeight, destinationSampleSize, shift);
    expectPlane(destination->span(), layouts[1], buffer.DataU(), buffer.StrideU(), chromaWidth, chromaHeight, destinationSampleSize, shift);
    expectPlane(destination->span(), layouts[2], buffer.DataV(), buffer.StrideV(), chromaWidth, chromaHeight, destinationSampleSize, shift);
}

TEST(VideoFrameLibWebRTC, CopyI420)
{
    auto buffer = webrtc::I420Buffer::Create(frameWidth, frameHeight);
    fillPlane(buffer->MutableDataY(), buffer->StrideY(), frameWidth, frameHeight, 1, 255);
    fillPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, chromaHeight420, 2, 255);
    fillPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, chromaHeight420, 3, 255);
    RefPtr frame = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { buffer });
    ASSERT_TRUE(frame);

    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I420), VideoPixelFormat::I420, *buffer, chromaHeight420, 1, 0);
}

TEST(VideoFrameLibWebRTC, CopyI010)
{
    auto buffer = webrtc::I010Buffer::Create(frameWidth, frameHeight);
    fillPlane(buffer->MutableDataY(), buffer->StrideY(), frameWidth, frameHeight, 900, 1023);
    fillPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, chromaHeight420, 500, 1023);
    fillPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, chromaHeight420, 100, 1023);
    RefPtr frame = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { buffer });
    ASSERT_TRUE(frame);

    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I420P10), VideoPixelFormat::I420P10, *buffer, chromaHeight420, 2, 0);
    // Narrowed to 8 bits.
    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I420), VideoPixelFormat::I420, *buffer, chromaHeight420, 1, 2);
}

TEST(VideoFrameLibWebRTC, CopyI422)
{
    auto buffer = webrtc::I422Buffer::Create(frameWidth, frameHeight);
    fillPlane(buffer->MutableDataY(), buffer->StrideY(), frameWidth, frameHeight, 4, 255);
    fillPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, frameHeight, 5, 255);
    fillPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, frameHeight, 6, 255);
    RefPtr frame = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { buffer });
    ASSERT_TRUE(frame);

    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I422), VideoPixelFormat::I422, *buffer, frameHeight, 1, 0);
}

TEST(VideoFrameLibWebRTC, CopyI210)
{
    auto buffer = webrtc::I210Buffer::Create(frameWidth, frameHeight);
    fillPlane(buffer->MutableDataY(), buffer->StrideY(), frameWidth, frameHeight, 700, 1023);
    fillPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, frameHeight, 300, 1023);
    fillPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, frameHeight, 50, 1023);
    RefPtr frame = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { buffer });
    ASSERT_TRUE(frame);

    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I422P10), VideoPixelFormat::I422P10, *buffer, frameHeight, 2, 0);
    // 4:2:2 is not downsampled to 4:2:0.
    EXPECT_FALSE(copyToPlanarBuffer(*frame, VideoPixelFormat::I420).has_value());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA) && USE(LIBWEBRTC) && ENABLE(WEB_RTC)
