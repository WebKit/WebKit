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

#if PLATFORM(COCOA)

#import <WebCore/H264UtilitiesCocoa.h>
#import <WebCore/TrackInfo.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace TestWebKitAPI {
using namespace WebCore;
// Arbitrary placeholder payloads: convertAVCCMSampleBufferToAnnexB() and CMVideoFormatDescriptionGetH264ParameterSetAtIndex() only need to copy these bytes, they don't need to be syntactically valid H.264 parameter sets.
static constexpr uint8_t spsBytes[] = { 0xcc, 0xdd, 0xee };
static constexpr uint8_t ppsBytes[] = { 0xff };
static constexpr uint8_t slice1Bytes[] = { 0x65, 0x11, 0x22, 0x33 };
static constexpr uint8_t slice2Bytes[] = { 0x41, 0x44, 0x55 };

static void appendByte(Vector<uint8_t>& buffer, uint8_t byte)
{
    buffer.append(byte);
}

static void appendBytes(Vector<uint8_t>& buffer, std::span<const uint8_t> bytes)
{
    buffer.append(bytes);
}

// Builds a valid ISO/IEC 14496-15 AVCDecoderConfigurationRecord carrying exactly one SPS and one PPS NAL unit.
static RetainPtr<NSData> makeAVCCData(std::span<const uint8_t> sps, std::span<const uint8_t> pps)
{
    Vector<uint8_t> avcC;
    appendByte(avcC, 0x01); // configurationVersion
    appendByte(avcC, 0x42); // AVCProfileIndication (Baseline)
    appendByte(avcC, 0x00); // profile_compatibility
    appendByte(avcC, 0x1f); // AVCLevelIndication (level 3.1)
    appendByte(avcC, 0xff); // reserved(6)='111111' | lengthSizeMinusOne(2)=3
    appendByte(avcC, 0xe1); // reserved(3)='111' | numOfSequenceParameterSets(5)=1
    appendBytes(avcC, std::array<uint8_t, 2> { static_cast<uint8_t>(sps.size() >> 8), static_cast<uint8_t>(sps.size() & 0xff) });
    appendBytes(avcC, sps);
    appendByte(avcC, 0x01); // numOfPictureParameterSets
    appendBytes(avcC, std::array<uint8_t, 2> { static_cast<uint8_t>(pps.size() >> 8), static_cast<uint8_t>(pps.size() & 0xff) });
    appendBytes(avcC, pps);

    return adoptNS([[NSData alloc] initWithBytes:avcC.span().data() length:avcC.size()]);
}

static RetainPtr<CMFormatDescriptionRef> createAVCFormatDescription(std::span<const uint8_t> sps, std::span<const uint8_t> pps)
{
    RetainPtr avcCData = makeAVCCData(sps, pps);
    NSDictionary *extensions = @{
        (__bridge NSString *)PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms: @{ @"avcC": avcCData.get() },
    };
    CMFormatDescriptionRef rawDescription = nullptr;
    PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, kCMVideoCodecType_H264, 64, 64, (__bridge CFDictionaryRef)extensions, &rawDescription);
    return adoptCF(rawDescription);
}

// Builds a CMSampleBuffer whose data buffer is a sequence of 4-byte-length-prefixed NAL units.
static RetainPtr<CMSampleBufferRef> createAVCSampleBuffer(CMVideoFormatDescriptionRef formatDescription, std::initializer_list<std::span<const uint8_t>> nalus)
{
    Vector<uint8_t> frameData;
    for (auto nalu : nalus) {
        uint32_t length = CFSwapInt32HostToBig(static_cast<uint32_t>(nalu.size()));
        frameData.append(std::span { reinterpret_cast<const uint8_t*>(&length), 4 });
        frameData.append(nalu);
    }

    CMBlockBufferRef rawBlockBuffer = nullptr;
    if (PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, frameData.size(), kCFAllocatorDefault, nullptr, 0, frameData.size(), kCMBlockBufferAssureMemoryNowFlag, &rawBlockBuffer))
        return nullptr;
    RetainPtr blockBuffer = adoptCF(rawBlockBuffer);

    if (PAL::CMBlockBufferReplaceDataBytes(frameData.span().data(), blockBuffer.get(), 0, frameData.size()))
        return nullptr;

    CMSampleTimingInfo timing = { PAL::CMTimeMake(1, 30), PAL::CMTimeMake(0, 30), PAL::CMTimeMake(0, 30) };
    size_t sampleSize = frameData.size();
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, formatDescription, 1, 1, &timing, 1, &sampleSize, &rawSampleBuffer))
        return nullptr;
    return adoptCF(rawSampleBuffer);
}

static Vector<uint8_t> annexBStartCode()
{
    return { 0, 0, 0, 1 };
}

TEST(H264UtilitiesCocoa, MalformedAVCCWithTruncatedSPSDoesNotCrash)
{
    // The SPS size field claims 0xFFFF bytes, but only the 1-byte NAL header
    // actually follows in the buffer. createVideoInfoFromAVCC must reject this
    // before forming a span past the end of the input.
    constexpr auto avcc = WTF::toArray<uint8_t>({
        0x01,        // configurationVersion
        0x42,        // AVCProfileIndication
        0x00,        // profile_compatibility
        0x1e,        // AVCLevelIndication
        0xff,        // reserved (6) | lengthSizeMinusOne (2) = 3
        0xe1,        // reserved (3) | numOfSequenceParameterSets (5) = 1
        0xff, 0xff,  // SPS size = 65535 (attacker-chosen)
        0x07,        // NAL type = SPS, with no payload bytes following
    });
    EXPECT_FALSE(!!WebCore::createVideoInfoFromAVCC(avcc));
}

TEST(H264UtilitiesCocoa, MalformedAVCCWithTruncatedPPSDoesNotCrash)
{
    // Same bug pattern as the SPS loop, but in the PPS loop. Provide one
    // well-formed (empty) SPS, then a PPS whose size field overruns the buffer.
    constexpr auto avcc = WTF::toArray<uint8_t>({
        0x01,        // configurationVersion
        0x42,        // AVCProfileIndication
        0x00,        // profile_compatibility
        0x1e,        // AVCLevelIndication
        0xff,        // reserved | lengthSizeMinusOne = 3
        0xe1,        // reserved | numOfSequenceParameterSets = 1
        0x00, 0x01,  // SPS size = 1 (NAL header only)
        0x07,        // SPS NAL type
        0x01,        // numOfPictureParameterSets = 1
        0xff, 0xff,  // PPS size = 65535 (attacker-chosen)
        0x08,        // NAL type = PPS, with no payload bytes following
    });
    EXPECT_FALSE(!!WebCore::createVideoInfoFromAVCC(avcc));
}


TEST(H264UtilitiesCocoa, KeyframePrependsParameterSets)
{
    auto description = createAVCFormatDescription(spsBytes, ppsBytes);
    ASSERT_TRUE(description);
    auto sample = createAVCSampleBuffer(description.get(), { slice1Bytes, slice2Bytes });
    ASSERT_TRUE(sample);

    auto annexB = convertAVCCMSampleBufferToAnnexB(sample.get(), true);

    Vector<uint8_t> expected;
    expected.append(annexBStartCode().span());
    expected.append(std::span { spsBytes });
    expected.append(annexBStartCode().span());
    expected.append(std::span { ppsBytes });
    expected.append(annexBStartCode().span());
    expected.append(std::span { slice1Bytes });
    expected.append(annexBStartCode().span());
    expected.append(std::span { slice2Bytes });

    EXPECT_EQ(annexB, expected);
}

TEST(H264UtilitiesCocoa, NonKeyframeOmitsParameterSets)
{
    auto description = createAVCFormatDescription(spsBytes, ppsBytes);
    ASSERT_TRUE(description);
    auto sample = createAVCSampleBuffer(description.get(), { slice1Bytes, slice2Bytes });
    ASSERT_TRUE(sample);

    auto annexB = convertAVCCMSampleBufferToAnnexB(sample.get(), false);

    Vector<uint8_t> expected;
    expected.append(annexBStartCode().span());
    expected.append(std::span { slice1Bytes });
    expected.append(annexBStartCode().span());
    expected.append(std::span { slice2Bytes });

    EXPECT_EQ(annexB, expected);
}

// Regression test mirroring HEVCUtilitiesCocoaTests's equivalent: the second (and later) NAL
// unit's length must be read from the current position in the block buffer, not a stale pointer.
TEST(H264UtilitiesCocoa, MultipleNALUsOfDifferentSizesAreAllExtracted)
{
    auto description = createAVCFormatDescription(spsBytes, ppsBytes);
    ASSERT_TRUE(description);
    auto sample = createAVCSampleBuffer(description.get(), { slice1Bytes, slice2Bytes });
    ASSERT_TRUE(sample);

    auto annexB = convertAVCCMSampleBufferToAnnexB(sample.get(), false);
    ASSERT_EQ(annexB.size(), 4 + sizeof(slice1Bytes) + 4 + sizeof(slice2Bytes));

    size_t slice2Offset = 4 + sizeof(slice1Bytes) + 4;
    for (size_t i = 0; i < sizeof(slice2Bytes); ++i)
        EXPECT_EQ(annexB[slice2Offset + i], slice2Bytes[i]);
}

// A NAL length prefix claiming more data than actually remains in the block buffer must be
// rejected rather than read out of bounds.
TEST(H264UtilitiesCocoa, TruncatedNALULengthReturnsEmpty)
{
    auto description = createAVCFormatDescription(spsBytes, ppsBytes);
    ASSERT_TRUE(description);

    Vector<uint8_t> frameData;
    uint32_t badLength = CFSwapInt32HostToBig(100);
    frameData.append(std::span { reinterpret_cast<const uint8_t*>(&badLength), 4 });
    frameData.append(std::span { slice1Bytes });

    CMBlockBufferRef rawBlockBuffer = nullptr;
    ASSERT_EQ(PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, frameData.size(), kCFAllocatorDefault, nullptr, 0, frameData.size(), kCMBlockBufferAssureMemoryNowFlag, &rawBlockBuffer), noErr);
    RetainPtr blockBuffer = adoptCF(rawBlockBuffer);
    ASSERT_EQ(PAL::CMBlockBufferReplaceDataBytes(frameData.span().data(), blockBuffer.get(), 0, frameData.size()), noErr);

    CMSampleTimingInfo timing = { PAL::CMTimeMake(1, 30), PAL::CMTimeMake(0, 30), PAL::CMTimeMake(0, 30) };
    size_t sampleSize = frameData.size();
    CMSampleBufferRef rawSampleBuffer = nullptr;
    ASSERT_EQ(PAL::CMSampleBufferCreate(kCFAllocatorDefault, blockBuffer.get(), true, nullptr, nullptr, description.get(), 1, 1, &timing, 1, &sampleSize, &rawSampleBuffer), noErr);
    RetainPtr sample = adoptCF(rawSampleBuffer);

    EXPECT_TRUE(convertAVCCMSampleBufferToAnnexB(sample.get(), false).isEmpty());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
