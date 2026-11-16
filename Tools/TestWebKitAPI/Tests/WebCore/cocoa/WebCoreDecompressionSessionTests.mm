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

#include "config.h"

#if USE(AVFOUNDATION)

#include "Helpers/PlatformUtilities.h"
#include <WebCore/CMUtilities.h>
#include <WebCore/WebCoreDecompressionSession.h>
#include <wtf/NativePromise.h>
#include <wtf/RunLoop.h>

#include <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

namespace TestWebKitAPI {

// H.264 SPS/PPS from mp4-generator.js (640x480, Main profile level 3.0)
static constexpr uint8_t h264SPS[] = {
    0x67, 0x4D, 0x40, 0x1E, 0xDA, 0x02, 0x80, 0xF6,
    0xC0, 0x44, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
    0x00, 0x03, 0x00, 0xC0, 0x3C, 0x58, 0xBA, 0x80
};

static constexpr uint8_t h264PPS[] = { 0x68, 0xEF, 0x0F, 0x2C, 0x80 };

// H.264 IDR frame NAL data from mp4-generator.js — a valid keyframe
static constexpr uint8_t h264IDR[] = {
    0x65, 0x88, 0x84, 0x0B,
    0xFF, 0xFE, 0xF6, 0xAE, 0xFC, 0xCB, 0x2B, 0x74,
    0x7E, 0x95, 0x2E, 0x1D, 0x59, 0x7B, 0xB3, 0x51,
    0xF2, 0xE8, 0x49, 0x72, 0xFD, 0x88, 0x30, 0x7D,
    0x68, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x08, 0x07, 0xEA, 0x2F, 0x7A, 0xE6, 0x31,
    0x2D, 0xF2, 0x90, 0x00, 0x00, 0x04, 0xF0, 0x01,
    0x1C, 0x06, 0x88, 0x3B, 0x42, 0xBA, 0x22, 0xE2,
    0x10, 0x23, 0x83, 0x3C, 0x4B, 0x08, 0x50, 0xE9,
    0x19, 0x40, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x12, 0xF1
};

// Alternate SPS for format change — same resolution, different level_idc (4.0 instead of 3.0)
static constexpr uint8_t h264SPS2[] = {
    0x67, 0x4D, 0x40, 0x28, 0xDA, 0x02, 0x80, 0xF6,
    0xC0, 0x44, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00,
    0x00, 0x03, 0x00, 0xC0, 0x3C, 0x58, 0xBA, 0x80
};

static RetainPtr<CMVideoFormatDescriptionRef> createFormatDescription(std::span<const uint8_t> sps, std::span<const uint8_t> pps)
{
    const uint8_t* parameterSetPointers[] = { sps.data(), pps.data() };
    size_t parameterSetSizes[] = { sps.size(), pps.size() };

    CMVideoFormatDescriptionRef rawDescription = nullptr;
    auto status = PAL::CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, 2, parameterSetPointers, parameterSetSizes, 4, &rawDescription);
    if (status != noErr)
        return nullptr;
    return adoptCF(rawDescription);
}

static RetainPtr<CMSampleBufferRef> createH264SampleBuffer(CMVideoFormatDescriptionRef formatDescription, CMTime presentationTime, bool isSync)
{
    // Build Annex B → length-prefixed NAL
    uint32_t nalLength = CFSwapInt32HostToBig(sizeof(h264IDR));
    Vector<uint8_t> frameData;
    frameData.append(std::span { reinterpret_cast<const uint8_t*>(&nalLength), 4 });
    frameData.append(std::span { h264IDR });

    CMBlockBufferRef rawBlockBuffer = nullptr;
    if (PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, frameData.size(), kCFAllocatorDefault, nullptr, 0, frameData.size(), kCMBlockBufferAssureMemoryNowFlag, &rawBlockBuffer))
        return nullptr;
    RetainPtr blockBuffer = adoptCF(rawBlockBuffer);

    if (PAL::CMBlockBufferReplaceDataBytes(frameData.span().data(), blockBuffer.get(), 0, frameData.size()))
        return nullptr;

    CMSampleTimingInfo timing = { PAL::CMTimeMake(1, 30), presentationTime, presentationTime };
    size_t sampleSize = frameData.size();
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, formatDescription, 1, 1, &timing, 1, &sampleSize, &rawSampleBuffer))
        return nullptr;

    if (!isSync) {
        RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(rawSampleBuffer, true);
        if (attachments && CFArrayGetCount(attachments.get()) > 0) {
            auto dict = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachments.get(), 0));
            CFDictionarySetValue(dict, PAL::kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
        }
    }

    return adoptCF(rawSampleBuffer);
}

struct DecodeResult {
    bool resolved { false };
    size_t sampleCount { 0 };
    OSStatus error { 0 };
};

static DecodeResult decodeSampleAndWait(WebCoreDecompressionSession& session, CMSampleBufferRef sample)
{
    bool done = false;
    DecodeResult decodeResult;

    session.decodeSample(sample, { })->whenSettled(RunLoop::mainSingleton(), [&done, &decodeResult](auto&& result) {
        decodeResult.resolved = !!result;
        if (result)
            decodeResult.sampleCount = result->size();
        else
            decodeResult.error = result.error();
        done = true;
    });
    Util::run(&done);
    return decodeResult;
}

TEST(WebCoreDecompressionSession, FormatChangeOnNonKeyframeDropsUntilKeyframe)
{
    auto session = WebCoreDecompressionSession::createRGB(nullptr);

    auto fmt1 = createFormatDescription(h264SPS, h264PPS);
    ASSERT_TRUE(fmt1);

    // Decode a keyframe to initialize the session.
    auto keyframe1 = createH264SampleBuffer(fmt1.get(), PAL::CMTimeMake(0, 30), true);
    ASSERT_TRUE(keyframe1);
    auto r1 = decodeSampleAndWait(session, keyframe1.get());
    EXPECT_TRUE(r1.resolved);

    // Decode another keyframe with same format to confirm decoding works.
    auto keyframe2 = createH264SampleBuffer(fmt1.get(), PAL::CMTimeMake(1, 30), true);
    ASSERT_TRUE(keyframe2);
    auto r2 = decodeSampleAndWait(session, keyframe2.get());
    EXPECT_TRUE(r2.resolved);
    EXPECT_EQ(r2.sampleCount, 1u);

    // Non-keyframe with a different format description.
    // All frames use the same IDR NAL data (decodable), but the format description changes.
    // This triggers format change on a non-keyframe — should drop, not error.
    auto fmt2 = createFormatDescription(h264SPS2, h264PPS);
    ASSERT_TRUE(fmt2);
    auto nonKeyframe = createH264SampleBuffer(fmt2.get(), PAL::CMTimeMake(2, 30), false);
    ASSERT_TRUE(nonKeyframe);
    auto r3 = decodeSampleAndWait(session, nonKeyframe.get());
    EXPECT_TRUE(r3.resolved);
    EXPECT_EQ(r3.sampleCount, 0u);

    // Another non-keyframe with new format — still waiting, should drop.
    auto nonKeyframe2 = createH264SampleBuffer(fmt2.get(), PAL::CMTimeMake(3, 30), false);
    ASSERT_TRUE(nonKeyframe2);
    auto r4 = decodeSampleAndWait(session, nonKeyframe2.get());
    EXPECT_TRUE(r4.resolved);
    EXPECT_EQ(r4.sampleCount, 0u);

    // Keyframe with new format — should create new decoder and succeed.
    auto keyframe3 = createH264SampleBuffer(fmt2.get(), PAL::CMTimeMake(4, 30), true);
    ASSERT_TRUE(keyframe3);
    auto r5 = decodeSampleAndWait(session, keyframe3.get());
    EXPECT_TRUE(r5.resolved);
    EXPECT_EQ(r5.sampleCount, 1u);
}

TEST(WebCoreDecompressionSession, FlushSetsWaitingForKeyframe)
{
    auto session = WebCoreDecompressionSession::createRGB(nullptr);

    auto fmt = createFormatDescription(h264SPS, h264PPS);
    ASSERT_TRUE(fmt);

    // Decode a keyframe to initialize.
    auto keyframe1 = createH264SampleBuffer(fmt.get(), PAL::CMTimeMake(0, 30), true);
    ASSERT_TRUE(keyframe1);
    auto r1 = decodeSampleAndWait(session, keyframe1.get());
    EXPECT_TRUE(r1.resolved);

    // Flush — should require a keyframe before resuming decode.
    session->flush();

    // Non-keyframe after flush — should be dropped.
    auto nonKeyframe = createH264SampleBuffer(fmt.get(), PAL::CMTimeMake(1, 30), false);
    ASSERT_TRUE(nonKeyframe);
    auto r2 = decodeSampleAndWait(session, nonKeyframe.get());
    EXPECT_TRUE(r2.resolved);
    EXPECT_EQ(r2.sampleCount, 0u);

    // Keyframe after flush — should succeed.
    auto keyframe2 = createH264SampleBuffer(fmt.get(), PAL::CMTimeMake(2, 30), true);
    ASSERT_TRUE(keyframe2);
    auto r3 = decodeSampleAndWait(session, keyframe2.get());
    EXPECT_TRUE(r3.resolved);
    EXPECT_EQ(r3.sampleCount, 1u);
}

TEST(WebCoreDecompressionSession, IsCMSampleBufferRandomAccess)
{
    auto fmt = createFormatDescription(h264SPS, h264PPS);
    ASSERT_TRUE(fmt);

    auto syncSample = createH264SampleBuffer(fmt.get(), PAL::CMTimeMake(0, 30), true);
    ASSERT_TRUE(syncSample);
    EXPECT_TRUE(isCMSampleBufferRandomAccess(syncSample.get()));

    auto nonSyncSample = createH264SampleBuffer(fmt.get(), PAL::CMTimeMake(1, 30), false);
    ASSERT_TRUE(nonSyncSample);
    EXPECT_FALSE(isCMSampleBufferRandomAccess(nonSyncSample.get()));
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
