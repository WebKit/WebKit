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

#include <WebCore/H264Utilities.h>

namespace TestWebKitAPI {
using namespace WebCore;

// The byte streams below are borrowed verbatim from libwebrtc's
// common_video/h264/h264_bitstream_parser_unittest.cc (kH264* constants), which are
// real, validated H.264 Annex B chunks with known slice QPs. Reusing them here cross-checks
// H264BitstreamParser (a from-scratch, safe-C++ port of that same algorithm) against the
// upstream implementation's expected outputs.

// SPS/PPS part of below chunk.
static constexpr uint8_t kH264SpsPps[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda,
    0x01, 0x40, 0x16, 0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00,
    0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2
};

// Contains enough of the image slice to contain slice QP.
static constexpr uint8_t kH264BitstreamChunk[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda, 0x01, 0x40, 0x16,
    0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06,
    0xe2, 0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
    0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 0xa0, 0xb2, 0x12, 0x1e, 0xf8,
};

static constexpr uint8_t kH264BitstreamChunkCabac[] = {
    0x00, 0x00, 0x00, 0x01, 0x27, 0x64, 0x00, 0x0d, 0xac, 0x52, 0x30,
    0x50, 0x7e, 0xc0, 0x5a, 0x81, 0x01, 0x01, 0x18, 0x56, 0xbd, 0xef,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x01, 0x28, 0xfe, 0x09, 0x8b,
};

// Contains enough of the image slice to contain slice QP.
static constexpr uint8_t kH264BitstreamNextImageSliceChunk[] = {
    0x00, 0x00, 0x00, 0x01, 0x41, 0xe2, 0x01, 0x16, 0x0e, 0x3e, 0x2b, 0x86,
};

// Contains enough of the image slice to contain slice QP.
static constexpr uint8_t kH264BitstreamNextImageSliceChunkCabac[] = {
    0x00, 0x00, 0x00, 0x01, 0x21, 0xe1, 0x05, 0x11, 0x3f, 0x9a, 0xae, 0x46,
    0x70, 0xbf, 0xc1, 0x4a, 0x16, 0x8f, 0x51, 0xf4, 0xca, 0xfb, 0xa3, 0x65,
};

static constexpr uint8_t kH264BitstreamWeightedPred[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x28, 0xac, 0xb4, 0x03, 0xc0,
    0x11, 0x3f, 0x2e, 0x02, 0xd4, 0x04, 0x04, 0x05, 0x00, 0x00, 0x03, 0x00,
    0x01, 0x00, 0x00, 0x03, 0x00, 0x30, 0x8f, 0x18, 0x32, 0xa0, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x68, 0xef, 0x3c, 0xb0, 0x00, 0x00,
    0x00, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x41, 0x9a, 0x26, 0x21, 0xf7, 0xff,
    0xfe, 0x9e, 0x10, 0x00, 0x00, 0x08, 0x78, 0x00, 0x00, 0x00, 0x12
};

TEST(H264BitstreamParser, ReportsNoQpBeforeAnyParsing)
{
    H264BitstreamParser parser;
    EXPECT_FALSE(parser.lastSliceQP().has_value());
}

TEST(H264BitstreamParser, ReportsNoQpAfterParsingOnlyParameterSets)
{
    H264BitstreamParser parser;
    parser.parseBitstream(std::span { kH264SpsPps });
    EXPECT_FALSE(parser.lastSliceQP().has_value());
}

TEST(H264BitstreamParser, ReportsQpForImageSlices)
{
    H264BitstreamParser parser;
    parser.parseBitstream(std::span { kH264BitstreamChunk });
    auto qp = parser.lastSliceQP();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(*qp, 35);

    parser.parseBitstream(std::span { kH264BitstreamNextImageSliceChunk });
    qp = parser.lastSliceQP();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(*qp, 37);
}

TEST(H264BitstreamParser, ReportsQpForCABACImageSlices)
{
    H264BitstreamParser parser;
    parser.parseBitstream(std::span { kH264BitstreamChunkCabac });
    EXPECT_FALSE(parser.lastSliceQP().has_value());

    parser.parseBitstream(std::span { kH264BitstreamNextImageSliceChunkCabac });
    auto qp = parser.lastSliceQP();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(*qp, 24);
}

TEST(H264BitstreamParser, ReportsQpForWeightedPredSlices)
{
    H264BitstreamParser parser;
    parser.parseBitstream(std::span { kH264BitstreamWeightedPred });
    auto qp = parser.lastSliceQP();
    ASSERT_TRUE(qp.has_value());
    EXPECT_EQ(*qp, 11);
}

}
