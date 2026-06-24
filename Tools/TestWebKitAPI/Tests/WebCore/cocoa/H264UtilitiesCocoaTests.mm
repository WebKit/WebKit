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

#import "Test.h"
#import <WebCore/H264UtilitiesCocoa.h>
#import <WebCore/TrackInfo.h>
#import <array>

namespace TestWebKitAPI {

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

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
