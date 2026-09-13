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

#include <WebCore/AV1Utilities.h>
#include <WebCore/TrackInfo.h>

namespace TestWebKitAPI {
using namespace WebCore;

// obu_type=1 (sequence header), has_size_field=1.
static constexpr uint8_t sequenceHeaderOBUHeader = 0x0A;
// obu_size ULEB128 for the 4-byte payload.
static constexpr uint8_t sequenceHeaderOBUSize = 0x04;

// The `>=` bounds check in getSequenceHeaderOBU() rejected an OBU ending exactly at the buffer end.
// The payload { 0x18, 0x00, 0x30, 0x00 } is a minimal reduced_still_picture_header for a 2x2 stream.
TEST(AV1Utilities, SequenceHeaderOBUExactlyFillsBuffer)
{
    std::array<uint8_t, 6> stream { sequenceHeaderOBUHeader, sequenceHeaderOBUSize, 0x18, 0x00, 0x30, 0x00 };
    auto videoInfo = createVideoInfoFromAV1Stream(stream);
    ASSERT_NE(videoInfo, nullptr);
    EXPECT_EQ(videoInfo->size(), FloatSize(2, 2));
}

TEST(AV1Utilities, SequenceHeaderOBUFollowedByTrailingData)
{
    std::array<uint8_t, 8> stream { sequenceHeaderOBUHeader, sequenceHeaderOBUSize, 0x18, 0x00, 0x30, 0x00, 0x12, 0x00 };
    auto videoInfo = createVideoInfoFromAV1Stream(stream);
    ASSERT_NE(videoInfo, nullptr);
    EXPECT_EQ(videoInfo->size(), FloatSize(2, 2));
}

TEST(AV1Utilities, SequenceHeaderOBUPayloadExceedsBuffer)
{
    std::array<uint8_t, 5> stream { sequenceHeaderOBUHeader, sequenceHeaderOBUSize, 0x18, 0x00, 0x30 };
    EXPECT_EQ(createVideoInfoFromAV1Stream(stream), nullptr);
}

} // namespace TestWebKitAPI
