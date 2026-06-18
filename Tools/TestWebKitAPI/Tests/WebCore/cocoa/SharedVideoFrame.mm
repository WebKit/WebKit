/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "Utilities.h"
#import <WebCore/SharedVideoFrameInfo.h>
#include <wtf/Vector.h>

#if USE(LIBWEBRTC)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/video/i010_buffer.h>
#include <webrtc/api/video/i420_buffer.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace TestWebKitAPI {

TEST(SharedVideoFrame, PlaneAlphaSize)
{
    Vector<uint8_t> data(128);
    WebCore::SharedVideoFrameInfo info {
        'v0a8', 1, 2, 1, 1, 1, 1, 1
    };
    info.encode(data.mutableSpan());
    EXPECT_EQ(info.storageSize(), 45u);

    auto info2 = WebCore::SharedVideoFrameInfo::decode(data.span());
    EXPECT_TRUE(info2);
    EXPECT_EQ(info2->storageSize(), 45u);

    data[28] = 2;

    info2 = WebCore::SharedVideoFrameInfo::decode(data.span());
    EXPECT_TRUE(info2);
    EXPECT_EQ(info2->storageSize(), 47u);

    info2 = WebCore::SharedVideoFrameInfo::decode(data.span().subspan(0, 28));
    EXPECT_FALSE(info2);
}

#if USE(LIBWEBRTC)

template<typename SampleType, typename Buffer>
static void testSharedVideoFrameInfoRoundTrip(Buffer& buffer)
{
    static_assert(sizeof(SampleType) == 1 || sizeof(SampleType) == 2);

    const int width = buffer.width();
    const int height = buffer.height();
    const int chromaWidth = (width + 1) / 2;
    const int chromaHeight = (height + 1) / 2;

    // I420 stores 8-bit samples; I010 stores 10-bit samples in uint16_t.
    constexpr unsigned valueMask = sizeof(SampleType) == 1 ? 0xFFu : 0x3FFu;
    constexpr unsigned msbShift = sizeof(SampleType) == 1 ? 0u : 6u;

    auto fillPlane = [&](std::span<SampleType> plane, int stride, int planeWidth, int planeHeight, unsigned seed) {
        for (int y = 0; y < planeHeight; ++y) {
            auto row = plane.subspan(y * stride, planeWidth);
            for (int x = 0; x < planeWidth; ++x)
                row[x] = static_cast<SampleType>((seed + 31u * y + 7u * x) & valueMask);
        }
    };
    fillPlane(unsafeMakeSpan(buffer.MutableDataY(), buffer.StrideY() * height), buffer.StrideY(), width, height, 1);
    fillPlane(unsafeMakeSpan(buffer.MutableDataU(), buffer.StrideU() * chromaHeight), buffer.StrideU(), chromaWidth, chromaHeight, 101);
    fillPlane(unsafeMakeSpan(buffer.MutableDataV(), buffer.StrideV() * chromaHeight), buffer.StrideV(), chromaWidth, chromaHeight, 211);

    auto info = WebCore::SharedVideoFrameInfo::fromVideoFrameBuffer(buffer);
    EXPECT_GT(info.storageSize(), 0u);

    constexpr size_t guardSize = 64;
    constexpr uint8_t guardByte = 0xCD;

    Vector<uint8_t> storage(info.storageSize() + guardSize);
    auto allBytes = storage.mutableSpan();
    auto frameBytes = allBytes.subspan(0, info.storageSize());
    auto guardBytes = allBytes.subspan(info.storageSize());
    for (auto& b : guardBytes)
        b = guardByte;

    EXPECT_TRUE(info.writeVideoFrameBuffer(buffer, frameBytes));

    for (auto byte : guardBytes)
        EXPECT_EQ(byte, guardByte);

    auto payload = spanReinterpretCast<const SampleType>(frameBytes.subspan(sizeof(WebCore::SharedVideoFrameInfo)));
    const int outStrideY = width;
    const int outStrideUV = (width & 1) ? width + 1 : width;
    auto outY = payload.first(outStrideY * height);
    auto outUV = payload.subspan(outStrideY * height, outStrideUV * chromaHeight);

    auto srcY = unsafeMakeSpan(buffer.DataY(), buffer.StrideY() * height);
    auto srcU = unsafeMakeSpan(buffer.DataU(), buffer.StrideU() * chromaHeight);
    auto srcV = unsafeMakeSpan(buffer.DataV(), buffer.StrideV() * chromaHeight);

    for (int y = 0; y < height; ++y) {
        auto srcRow = srcY.subspan(y * buffer.StrideY(), width);
        auto outRow = outY.subspan(y * outStrideY, width);
        for (int x = 0; x < width; ++x) {
            auto expected = static_cast<SampleType>(static_cast<uint32_t>(srcRow[x]) << msbShift);
            EXPECT_EQ(outRow[x], expected) << "Y mismatch at (" << x << ", " << y << ")";
        }
    }

    for (int y = 0; y < chromaHeight; ++y) {
        auto srcURow = srcU.subspan(y * buffer.StrideU(), chromaWidth);
        auto srcVRow = srcV.subspan(y * buffer.StrideV(), chromaWidth);
        auto outRow = outUV.subspan(y * outStrideUV, 2 * chromaWidth);
        for (int x = 0; x < chromaWidth; ++x) {
            auto expectedU = static_cast<SampleType>(static_cast<uint32_t>(srcURow[x]) << msbShift);
            auto expectedV = static_cast<SampleType>(static_cast<uint32_t>(srcVRow[x]) << msbShift);
            EXPECT_EQ(outRow[2 * x], expectedU) << "U mismatch at (" << x << ", " << y << ")";
            EXPECT_EQ(outRow[2 * x + 1], expectedV) << "V mismatch at (" << x << ", " << y << ")";
        }
    }
}

TEST(SharedVideoFrame, OddWidthI420)
{
    auto buffer = webrtc::I420Buffer::Create(681, 1280);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint8_t>(*buffer);
}

TEST(SharedVideoFrame, OddHeightI420)
{
    auto buffer = webrtc::I420Buffer::Create(680, 15);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint8_t>(*buffer);
}

TEST(SharedVideoFrame, OddWidthAndHeightI420)
{
    auto buffer = webrtc::I420Buffer::Create(681, 15);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint8_t>(*buffer);
}

TEST(SharedVideoFrame, OddWidthI010)
{
    auto buffer = webrtc::I010Buffer::Create(681, 1280);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint16_t>(*buffer);
}

TEST(SharedVideoFrame, OddHeightI010)
{
    auto buffer = webrtc::I010Buffer::Create(680, 15);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint16_t>(*buffer);
}

TEST(SharedVideoFrame, OddWidthAndHeightI010)
{
    auto buffer = webrtc::I010Buffer::Create(681, 15);
    ASSERT_TRUE(buffer);
    testSharedVideoFrameInfoRoundTrip<uint16_t>(*buffer);
}

#endif // USE(LIBWEBRTC)

}; // namespace TestWebKitAPI
