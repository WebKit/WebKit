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

#if USE(AVFOUNDATION)

#import "Helpers/Test.h"
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <WebCore/AudioSampleFormat.h>
#import <WebCore/CMUtilities.h>
#import <WebCore/MediaSamplesBlock.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TrackInfo.h>
#import <WebCore/VideoPixelFormat.h>
#import <wtf/Scope.h>
#import <wtf/Vector.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace TestWebKitAPI {

using namespace WebCore;

static constexpr uint32_t sampleRate = 48000;

static Ref<AudioInfo> makeDecodedAudioInfo(uint32_t channels, AudioSampleFormat sampleFormat = AudioSampleFormat::F32)
{
    return AudioInfo::create({
        {
            .codecName = FourCC { "lpcm" }
        }, {
            .rate = sampleRate,
            .channels = channels,
            .framesPerPacket = 1,
            .bitDepth = 32,
            .sampleFormat = sampleFormat
        }
    });
}

static Ref<VideoInfo> makeDecodedVideoInfo(FloatSize size, std::optional<VideoPixelFormat> pixelFormat = VideoPixelFormat::I420)
{
    return VideoInfo::create({
        {
            .codecName = FourCC { "I420" }
        }, {
            .size = size,
            .displaySize = size,
            .bitDepth = 8,
            .pixelFormat = pixelFormat
        }
    });
}

static MediaSamplesBlock::MediaSampleItem makeAudioItem(MediaTime presentationTime, Vector<float>&& samples, uint32_t channels)
{
    size_t frames = samples.size() / channels;
    Vector<uint8_t> bytes(samples.size() * sizeof(float));
    memcpySpan(bytes.mutableSpan(), asByteSpan(samples.span()));
    return {
        .presentationTime = presentationTime,
        .duration = MediaTime(frames, sampleRate),
        .data = SharedBuffer::create(WTF::move(bytes)),
        .flags = MediaSample::IsSync
    };
}

TEST(DecodedMediaSamplesBlock, AudioToCMSampleBuffer)
{
    constexpr uint32_t channels = 2;
    Ref info = makeDecodedAudioInfo(channels);

    MediaSamplesBlock::SamplesVector items;
    items.append(makeAudioItem(MediaTime(1, 1), { 0.f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f }, channels));
    items.append(makeAudioItem(MediaTime(1, 1) + MediaTime(3, sampleRate), { 0.6f, 0.7f }, channels));
    MediaSamplesBlock block { info.ptr(), WTF::move(items) };

    auto result = toCMSampleBuffer(block);
    ASSERT_TRUE(!!result);
    RetainPtr sampleBuffer = *result;

    EXPECT_EQ(4, PAL::CMSampleBufferGetNumSamples(sampleBuffer.get()));
    EXPECT_NEAR(1.0, PAL::CMTimeGetSeconds(PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer.get())), 1e-9);
    EXPECT_NEAR(4.0 / sampleRate, PAL::CMTimeGetSeconds(PAL::CMSampleBufferGetDuration(sampleBuffer.get())), 1e-9);

    CMFormatDescriptionRef description = PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get());
    ASSERT_NE(nullptr, description);
    EXPECT_EQ(static_cast<FourCharCode>(kCMMediaType_Audio), PAL::CMFormatDescriptionGetMediaType(description));
    const AudioStreamBasicDescription* streamDescription = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(description);
    ASSERT_NE(nullptr, streamDescription);
    EXPECT_EQ(static_cast<UInt32>(kAudioFormatLinearPCM), streamDescription->mFormatID);
    EXPECT_DOUBLE_EQ(sampleRate, streamDescription->mSampleRate);
    EXPECT_EQ(channels, streamDescription->mChannelsPerFrame);
    EXPECT_TRUE(streamDescription->mFormatFlags & kAudioFormatFlagIsFloat);
    EXPECT_FALSE(streamDescription->mFormatFlags & kAudioFormatFlagIsNonInterleaved);
    EXPECT_EQ(static_cast<UInt32>(channels * sizeof(float)), streamDescription->mBytesPerFrame);

    CMBlockBufferRef blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer.get());
    ASSERT_NE(nullptr, blockBuffer);
    ASSERT_EQ(8 * sizeof(float), PAL::CMBlockBufferGetDataLength(blockBuffer));
    std::array<float, 8> copied { };
    ASSERT_EQ(kCMBlockBufferNoErr, PAL::CMBlockBufferCopyDataBytes(blockBuffer, 0, sizeof(copied), copied.data()));
    for (size_t i = 0; i < copied.size(); ++i)
        EXPECT_FLOAT_EQ(static_cast<float>(i) / 10.f, copied[i]);
}

TEST(DecodedMediaSamplesBlock, AudioDiscontinuityAttachment)
{
    Ref info = makeDecodedAudioInfo(1);
    MediaSamplesBlock::SamplesVector items;
    items.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f }, 1));
    MediaSamplesBlock block { info.ptr(), WTF::move(items) };
    block.setDiscontinuity(true);

    auto result = toCMSampleBuffer(block);
    ASSERT_TRUE(!!result);
    CFTypeRef attachment = PAL::CMGetAttachment(result->get(), PAL::kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, nullptr);
    EXPECT_EQ(kCFBooleanTrue, attachment);
}

TEST(DecodedMediaSamplesBlock, AudioRejectsInvalidData)
{
    Ref info = makeDecodedAudioInfo(2);

    // Not a whole number of frames (3 floats for 2 channels).
    MediaSamplesBlock::SamplesVector partialFrame;
    partialFrame.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f, 0.f }, 1));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { info.ptr(), WTF::move(partialFrame) }));

    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { info.ptr(), { } }));

    // Only interleaved F32 is supported.
    Ref integerInfo = makeDecodedAudioInfo(2, AudioSampleFormat::S16);
    MediaSamplesBlock::SamplesVector items;
    items.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f }, 2));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { integerInfo.ptr(), WTF::move(items) }));
}

TEST(DecodedMediaSamplesBlock, AudioRejectsMismatchedFormatDescription)
{
    Ref stereo = makeDecodedAudioInfo(2);
    MediaSamplesBlock::SamplesVector stereoItems;
    stereoItems.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f }, 2));
    auto stereoResult = toCMSampleBuffer(MediaSamplesBlock { stereo.ptr(), WTF::move(stereoItems) });
    ASSERT_TRUE(!!stereoResult);
    RetainPtr stereoFormat = PAL::CMSampleBufferGetFormatDescription(stereoResult->get());
    ASSERT_TRUE(!!stereoFormat);

    MediaSamplesBlock::SamplesVector moreStereoItems;
    moreStereoItems.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f }, 2));
    EXPECT_TRUE(!!toCMSampleBuffer(MediaSamplesBlock { stereo.ptr(), WTF::move(moreStereoItems) }, stereoFormat.get()));

    Ref mono = makeDecodedAudioInfo(1);
    MediaSamplesBlock::SamplesVector monoItems;
    monoItems.append(makeAudioItem(MediaTime::zeroTime(), { 0.f, 0.f }, 1));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { mono.ptr(), WTF::move(monoItems) }, stereoFormat.get()));
}

static MediaSamplesBlock::MediaSampleItem makeI420Item(MediaTime presentationTime, size_t width, size_t height, uint8_t y, uint8_t u, uint8_t v)
{
    size_t chromaSize = ((width + 1) / 2) * ((height + 1) / 2);
    Vector<uint8_t> bytes;
    bytes.appendUsingFunctor(width * height, [&](size_t) {
        return y;
    });
    bytes.appendUsingFunctor(chromaSize, [&](size_t i) {
        return static_cast<uint8_t>(u + i);
    });
    bytes.appendUsingFunctor(chromaSize, [&](size_t i) {
        return static_cast<uint8_t>(v + i);
    });
    return {
        .presentationTime = presentationTime,
        .duration = MediaTime(1, 30),
        .data = SharedBuffer::create(WTF::move(bytes)),
        .flags = MediaSample::IsSync
    };
}

static void checkI420Frame(size_t width, size_t height)
{
    Ref info = makeDecodedVideoInfo(FloatSize(static_cast<float>(width), static_cast<float>(height)));
    MediaSamplesBlock::SamplesVector items;
    items.append(makeI420Item(MediaTime(2, 1), width, height, 90, 10, 100));
    MediaSamplesBlock block { info.ptr(), WTF::move(items) };

    auto result = toCMSampleBuffer(block);
    ASSERT_TRUE(!!result);
    RetainPtr sampleBuffer = *result;

    EXPECT_NEAR(2.0, PAL::CMTimeGetSeconds(PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer.get())), 1e-9);
    EXPECT_NEAR(1.0 / 30, PAL::CMTimeGetSeconds(PAL::CMSampleBufferGetDuration(sampleBuffer.get())), 1e-9);

    CVImageBufferRef imageBuffer = PAL::CMSampleBufferGetImageBuffer(sampleBuffer.get());
    ASSERT_NE(nullptr, imageBuffer);
    EXPECT_EQ(width, CVPixelBufferGetWidth(imageBuffer));
    EXPECT_EQ(height, CVPixelBufferGetHeight(imageBuffer));
    EXPECT_EQ(static_cast<OSType>(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange), CVPixelBufferGetPixelFormatType(imageBuffer));
    EXPECT_NE(nullptr, CVPixelBufferGetIOSurface(imageBuffer));

    CMVideoFormatDescriptionRef description = PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get());
    ASSERT_NE(nullptr, description);
    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(description);
    EXPECT_EQ(static_cast<int32_t>(width), dimensions.width);
    EXPECT_EQ(static_cast<int32_t>(height), dimensions.height);

    size_t chromaWidth = (width + 1) / 2;
    size_t chromaHeight = (height + 1) / 2;
    ASSERT_EQ(kCVReturnSuccess, CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly));
    auto unlock = makeScopeExit([&] {
        CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    });

    auto* luma = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0));
    size_t lumaStride = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
    for (size_t row = 0; row < height; ++row) {
        for (size_t column = 0; column < width; ++column)
            ASSERT_EQ(90, luma[row * lumaStride + column]);
    }

    auto* chroma = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1));
    size_t chromaStride = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
    for (size_t row = 0; row < chromaHeight; ++row) {
        for (size_t column = 0; column < chromaWidth; ++column) {
            size_t index = row * chromaWidth + column;
            ASSERT_EQ(static_cast<uint8_t>(10 + index), chroma[row * chromaStride + 2 * column]);
            ASSERT_EQ(static_cast<uint8_t>(100 + index), chroma[row * chromaStride + 2 * column + 1]);
        }
    }
}

TEST(DecodedMediaSamplesBlock, VideoToCMSampleBuffer)
{
    checkI420Frame(64, 48);
}

TEST(DecodedMediaSamplesBlock, VideoWithOddDimensions)
{
    checkI420Frame(33, 17);
}

TEST(DecodedMediaSamplesBlock, VideoRejectsInvalidData)
{
    Ref info = makeDecodedVideoInfo(FloatSize(16, 16));

    // A block holds exactly one frame.
    MediaSamplesBlock::SamplesVector twoFrames;
    twoFrames.append(makeI420Item(MediaTime::zeroTime(), 16, 16, 0, 0, 0));
    twoFrames.append(makeI420Item(MediaTime(1, 30), 16, 16, 0, 0, 0));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { info.ptr(), WTF::move(twoFrames) }));

    // The data size must match the frame size.
    MediaSamplesBlock::SamplesVector wrongSize;
    wrongSize.append(makeI420Item(MediaTime::zeroTime(), 8, 8, 0, 0, 0));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { info.ptr(), WTF::move(wrongSize) }));

    // Only 8-bit 4:2:0 is supported.
    Ref unsupported = makeDecodedVideoInfo(FloatSize(16, 16), VideoPixelFormat::I444);
    MediaSamplesBlock::SamplesVector items;
    items.append(makeI420Item(MediaTime::zeroTime(), 16, 16, 0, 0, 0));
    EXPECT_FALSE(!!toCMSampleBuffer(MediaSamplesBlock { unsupported.ptr(), WTF::move(items) }));
}

TEST(DecodedMediaSamplesBlock, VideoFormatDescriptionFromTrackInfo)
{
    Ref info = makeDecodedVideoInfo(FloatSize(64, 48));
    RetainPtr description = createFormatDescriptionFromTrackInfo(info.get());
    ASSERT_NE(nullptr, description.get());
    EXPECT_EQ(static_cast<FourCharCode>(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange), PAL::CMFormatDescriptionGetMediaSubType(description.get()));

    EXPECT_EQ(nullptr, createFormatDescriptionFromTrackInfo(makeDecodedVideoInfo(FloatSize(64, 48), VideoPixelFormat::I422).get()).get());
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
