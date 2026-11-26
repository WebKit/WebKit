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

static std::optional<Vector<uint8_t>> copyToPlanarBuffer(VideoFrame& frame, VideoPixelFormat format, VideoFrame* alphaFrame = nullptr)
{
    auto size = VideoFrame::planarBufferSize({ frameWidth, frameHeight }, format);
    if (!size)
        return { };
    Vector<uint8_t> destination(*size);
    if (!frame.copyToPlanarBuffer(destination.mutableSpan(), format, alphaFrame))
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
    // 10-bit samples are not narrowed to 8 bits.
    EXPECT_FALSE(copyToPlanarBuffer(*frame, VideoPixelFormat::I420).has_value());
}

TEST(VideoFrameLibWebRTC, CopyI422)
{
    auto buffer = webrtc::I422Buffer::Create(frameWidth, frameHeight);
    fillPlane(buffer->MutableDataY(), buffer->StrideY(), frameWidth, frameHeight, 4, 255);
    fillPlane(buffer->MutableDataU(), buffer->StrideU(), chromaWidth, frameHeight, 5, 255);
    fillPlane(buffer->MutableDataV(), buffer->StrideV(), chromaWidth, frameHeight, 6, 255);
    RefPtr frame = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { buffer });
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->pixelFormat(), static_cast<uint32_t>(kCVPixelFormatType_422YpCbCr8BiPlanarFullRange));

    expectPlanes(copyToPlanarBuffer(*frame, VideoPixelFormat::I422), VideoPixelFormat::I422, *buffer, frameHeight, 1, 0);
    // 4:2:2 is not downsampled to 4:2:0.
    EXPECT_FALSE(copyToPlanarBuffer(*frame, VideoPixelFormat::I420).has_value());
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

template<typename Buffer>
static void expectColorPlanes(const Vector<uint8_t>& destination, VideoPixelFormat format, const Buffer& buffer, int chromaHeight, size_t sampleSize)
{
    auto layouts = VideoFrame::planarBufferLayout({ frameWidth, frameHeight }, format);
    ASSERT_EQ(layouts.size(), 4u);
    expectPlane(destination.span(), layouts[0], buffer.DataY(), buffer.StrideY(), frameWidth, frameHeight, sampleSize, 0);
    expectPlane(destination.span(), layouts[1], buffer.DataU(), buffer.StrideU(), chromaWidth, chromaHeight, sampleSize, 0);
    expectPlane(destination.span(), layouts[2], buffer.DataV(), buffer.StrideV(), chromaWidth, chromaHeight, sampleSize, 0);
}

template<typename Expected>
static void expectAlphaPlane(const Vector<uint8_t>& destination, VideoPixelFormat format, size_t sampleSize, Expected&& expectedAlpha)
{
    auto layouts = VideoFrame::planarBufferLayout({ frameWidth, frameHeight }, format);
    ASSERT_EQ(layouts.size(), 4u);
    for (int y = 0; y < frameHeight; ++y) {
        for (int x = 0; x < frameWidth; ++x) {
            size_t index = layouts[3].destinationOffset + y * layouts[3].destinationStride + x * sampleSize;
            unsigned value = sampleSize == 1 ? destination[index] : destination[index] | (destination[index + 1] << 8);
            EXPECT_EQ(value, static_cast<unsigned>(expectedAlpha(x, y))) << "x " << x << " y " << y;
        }
    }
}

template<typename Buffer>
static unsigned lumaAt(const Buffer& buffer, int x, int y)
{
    return unsafeMakeSpan(buffer.DataY(), static_cast<size_t>(buffer.StrideY()) * frameHeight)[y * buffer.StrideY() + x];
}

// The alpha plane holds the alpha frame's luma at the colour format's bit depth, and is opaque without an alpha frame.
TEST(VideoFrameLibWebRTC, CopyWithAlpha)
{
    auto alpha8 = webrtc::I420Buffer::Create(frameWidth, frameHeight);
    fillPlane(alpha8->MutableDataY(), alpha8->StrideY(), frameWidth, frameHeight, 9, 255);
    RefPtr alphaFrame8 = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { alpha8 });
    auto alpha10 = webrtc::I010Buffer::Create(frameWidth, frameHeight);
    fillPlane(alpha10->MutableDataY(), alpha10->StrideY(), frameWidth, frameHeight, 600, 1023);
    RefPtr alphaFrame10 = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { alpha10 });
    ASSERT_TRUE(alphaFrame8 && alphaFrame10);

    auto alpha8At = [&](int x, int y) {
        return lumaAt(*alpha8, x, y);
    };
    auto alpha10At = [&](int x, int y) {
        return lumaAt(*alpha10, x, y);
    };

    auto color420P10 = webrtc::I010Buffer::Create(frameWidth, frameHeight);
    fillPlane(color420P10->MutableDataY(), color420P10->StrideY(), frameWidth, frameHeight, 900, 1023);
    fillPlane(color420P10->MutableDataU(), color420P10->StrideU(), chromaWidth, chromaHeight420, 500, 1023);
    fillPlane(color420P10->MutableDataV(), color420P10->StrideV(), chromaWidth, chromaHeight420, 100, 1023);
    RefPtr frame420P10 = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { color420P10 });
    ASSERT_TRUE(frame420P10);

    auto destination = copyToPlanarBuffer(*frame420P10, VideoPixelFormat::I420AP10, alphaFrame10.get());
    ASSERT_TRUE(destination);
    expectColorPlanes(*destination, VideoPixelFormat::I420AP10, *color420P10, chromaHeight420, 2);
    expectAlphaPlane(*destination, VideoPixelFormat::I420AP10, 2, alpha10At);

    // 8-bit alpha is widened, 255 becoming 1023.
    destination = copyToPlanarBuffer(*frame420P10, VideoPixelFormat::I420AP10, alphaFrame8.get());
    ASSERT_TRUE(destination);
    expectAlphaPlane(*destination, VideoPixelFormat::I420AP10, 2, [&](int x, int y) {
        return (alpha8At(x, y) << 2) | (alpha8At(x, y) >> 6);
    });

    auto color422 = webrtc::I422Buffer::Create(frameWidth, frameHeight);
    fillPlane(color422->MutableDataY(), color422->StrideY(), frameWidth, frameHeight, 4, 255);
    fillPlane(color422->MutableDataU(), color422->StrideU(), chromaWidth, frameHeight, 5, 255);
    fillPlane(color422->MutableDataV(), color422->StrideV(), chromaWidth, frameHeight, 6, 255);
    RefPtr frame422 = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { color422 });
    ASSERT_TRUE(frame422);

    destination = copyToPlanarBuffer(*frame422, VideoPixelFormat::I422A, alphaFrame8.get());
    ASSERT_TRUE(destination);
    expectColorPlanes(*destination, VideoPixelFormat::I422A, *color422, frameHeight, 1);
    expectAlphaPlane(*destination, VideoPixelFormat::I422A, 1, alpha8At);

    destination = copyToPlanarBuffer(*frame422, VideoPixelFormat::I422A, alphaFrame10.get());
    ASSERT_TRUE(destination);
    expectAlphaPlane(*destination, VideoPixelFormat::I422A, 1, [&](int x, int y) {
        return alpha10At(x, y) >> 2;
    });

    destination = copyToPlanarBuffer(*frame422, VideoPixelFormat::I422A);
    ASSERT_TRUE(destination);
    expectAlphaPlane(*destination, VideoPixelFormat::I422A, 1, [](int, int) {
        return 255;
    });

    auto color422P10 = webrtc::I210Buffer::Create(frameWidth, frameHeight);
    fillPlane(color422P10->MutableDataY(), color422P10->StrideY(), frameWidth, frameHeight, 700, 1023);
    fillPlane(color422P10->MutableDataU(), color422P10->StrideU(), chromaWidth, frameHeight, 300, 1023);
    fillPlane(color422P10->MutableDataV(), color422P10->StrideV(), chromaWidth, frameHeight, 50, 1023);
    RefPtr frame422P10 = createFrame(webrtc::scoped_refptr<webrtc::VideoFrameBuffer> { color422P10 });
    ASSERT_TRUE(frame422P10);

    destination = copyToPlanarBuffer(*frame422P10, VideoPixelFormat::I422AP10, alphaFrame10.get());
    ASSERT_TRUE(destination);
    expectColorPlanes(*destination, VideoPixelFormat::I422AP10, *color422P10, frameHeight, 2);
    expectAlphaPlane(*destination, VideoPixelFormat::I422AP10, 2, alpha10At);

    destination = copyToPlanarBuffer(*frame422P10, VideoPixelFormat::I422AP10);
    ASSERT_TRUE(destination);
    expectAlphaPlane(*destination, VideoPixelFormat::I422AP10, 2, [](int, int) {
        return 1023;
    });
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA) && USE(LIBWEBRTC) && ENABLE(WEB_RTC)
