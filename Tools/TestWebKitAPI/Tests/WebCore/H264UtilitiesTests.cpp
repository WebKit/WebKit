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
#include <utility>
#include <wtf/Vector.h>

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

TEST(H264BitstreamParser, ComputesZeroMaxNumReorderFramesWhenPicOrderCntTypeIsTwo)
{
    // kH264SpsPps's SPS (bytes [4, 17)) has pic_order_cnt_type == 2, which per the H.264 spec means frames are never reordered.
    auto sps = std::span { kH264SpsPps }.subspan(4, 13);
    auto maxNumReorderFrames = H264BitstreamParser::parseSpsMaxNumReorderFrames(sps);
    ASSERT_TRUE(maxNumReorderFrames.has_value());
    EXPECT_EQ(*maxNumReorderFrames, 0u);
}

// Minimal MSB-first bit writer used to hand-craft a syntactically valid SPS RBSP below.
class TestBitWriter {
public:
    void writeFlag(bool flag) { writeBit(flag ? 1 : 0); }

    void writeBits(uint32_t value, size_t numBits)
    {
        for (size_t i = 0; i < numBits; ++i)
            writeBit((value >> (numBits - 1 - i)) & 1);
    }

    void writeExpGolomb(uint32_t value)
    {
        uint32_t codeNum = value + 1;
        size_t leadingZeroBits = 0;
        while ((codeNum >> (leadingZeroBits + 1)))
            ++leadingZeroBits;
        for (size_t i = 0; i < leadingZeroBits; ++i)
            writeBit(0);
        writeBits(codeNum, leadingZeroBits + 1);
    }

    Vector<uint8_t> takeBytes()
    {
        if (m_bitCount)
            flushByte();
        return std::exchange(m_bytes, { });
    }

private:
    void writeBit(uint32_t bit)
    {
        m_currentByte = (m_currentByte << 1) | (bit & 1);
        if (++m_bitCount == 8)
            flushByte();
    }

    void flushByte()
    {
        m_currentByte <<= (8 - m_bitCount);
        m_bytes.append(m_currentByte);
        m_currentByte = 0;
        m_bitCount = 0;
    }

    Vector<uint8_t> m_bytes;
    uint8_t m_currentByte { 0 };
    size_t m_bitCount { 0 };
};

// Builds an SPS NAL unit (NAL header + RBSP) whose VUI bitstream_restriction() sets
// max_num_reorder_frames to reorderFrames, so findAVCCMaxNumReorderFrames() has to walk
// all the way through profile/frame-size/VUI parsing to get a non-trivial, non-zero answer
// (unlike kH264SpsPps's SPS, whose pic_order_cnt_type == 2 shortcuts straight to 0).
static Vector<uint8_t> makeSpsWithMaxNumReorderFrames(uint32_t reorderFrames)
{
    TestBitWriter writer;
    writer.writeBits(66, 8); // profile_idc = 66 (Baseline), so the high-profile chroma block is skipped.
    writer.writeBits(0, 8); // constraint_set flags + reserved_zero_2bits.
    writer.writeBits(30, 8); // level_idc = 3.0, whose MaxDpbMbs (8100) comfortably exceeds this 16x16 frame.
    writer.writeExpGolomb(0); // seq_parameter_set_id.
    writer.writeExpGolomb(0); // log2_max_frame_num_minus4.
    writer.writeExpGolomb(0); // pic_order_cnt_type = 0.
    writer.writeExpGolomb(0); // log2_max_pic_order_cnt_lsb_minus4.
    writer.writeExpGolomb(0); // max_num_ref_frames.
    writer.writeFlag(false); // gaps_in_frame_num_value_allowed_flag.
    writer.writeExpGolomb(0); // pic_width_in_mbs_minus1 -> 1 macroblock wide.
    writer.writeExpGolomb(0); // pic_height_in_map_units_minus1 -> 1 macroblock tall.
    writer.writeFlag(true); // frame_mbs_only_flag.
    writer.writeFlag(false); // direct_8x8_inference_flag.
    writer.writeFlag(false); // frame_cropping_flag.
    writer.writeFlag(true); // vui_parameters_present_flag.
    writer.writeFlag(false); // aspect_ratio_info_present_flag.
    writer.writeFlag(false); // overscan_info_present_flag.
    writer.writeFlag(false); // video_signal_type_present_flag.
    writer.writeFlag(false); // chroma_loc_info_present_flag.
    writer.writeFlag(false); // timing_info_present_flag.
    writer.writeFlag(false); // nal_hrd_parameters_present_flag.
    writer.writeFlag(false); // vcl_hrd_parameters_present_flag.
    writer.writeFlag(false); // pic_struct_present_flag.
    writer.writeFlag(true); // bitstream_restriction_flag.
    writer.writeFlag(false); // motion_vectors_over_pic_boundaries_flag.
    writer.writeExpGolomb(0); // max_bytes_per_pic_denom.
    writer.writeExpGolomb(0); // max_bits_per_mb_denom.
    writer.writeExpGolomb(0); // log2_max_mv_length_horizontal.
    writer.writeExpGolomb(0); // log2_max_mv_length_vertical.
    writer.writeExpGolomb(reorderFrames); // max_num_reorder_frames.
    writer.writeExpGolomb(reorderFrames); // max_dec_frame_buffering.

    Vector<uint8_t> sps;
    sps.append(0x67); // NAL header: nal_ref_idc = 3, nal_unit_type = 7 (SPS).
    sps.appendVector(writer.takeBytes());
    return sps;
}

// Builds a minimal ISO/IEC 14496-15 AVCDecoderConfigurationRecord carrying a single SPS NAL unit,
// which is all findAVCCMaxNumReorderFrames() looks at before it stops walking the record.
static Vector<uint8_t> makeAVCCWithSingleSps(std::span<const uint8_t> sps)
{
    Vector<uint8_t> avcC;
    avcC.append(0x01); // configurationVersion
    avcC.append(0x42); // AVCProfileIndication (Baseline)
    avcC.append(0x00); // profile_compatibility
    avcC.append(0x1f); // AVCLevelIndication (level 3.1)
    avcC.append(0xff); // reserved(6)='111111' | lengthSizeMinusOne(2)=3
    avcC.append(0xe1); // reserved(3)='111' | numOfSequenceParameterSets(5)=1
    avcC.append(static_cast<uint8_t>(sps.size() >> 8));
    avcC.append(static_cast<uint8_t>(sps.size() & 0xff));
    avcC.append(sps);
    return avcC;
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesParsesEmbeddedSps)
{
    // kH264SpsPps's SPS (bytes [4, 17)) has pic_order_cnt_type == 2, which per the H.264 spec means frames are never reordered.
    auto sps = std::span { kH264SpsPps }.subspan(4, 13);
    auto avcc = makeAVCCWithSingleSps(sps);
    auto maxNumReorderFrames = findAVCCMaxNumReorderFrames(avcc.span());
    ASSERT_TRUE(maxNumReorderFrames.has_value());
    EXPECT_EQ(*maxNumReorderFrames, 0u);
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesReturnsNonZeroValueFromBitstreamRestriction)
{
    auto sps = makeSpsWithMaxNumReorderFrames(2);
    auto avcc = makeAVCCWithSingleSps(sps.span());
    auto maxNumReorderFrames = findAVCCMaxNumReorderFrames(avcc.span());
    ASSERT_TRUE(maxNumReorderFrames.has_value());
    EXPECT_EQ(*maxNumReorderFrames, 2u);
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesRejectsTooShortRecord)
{
    // A well-formed avcC header is at least 7 bytes; anything shorter must be rejected outright.
    constexpr uint8_t avcc[] = { 0x01, 0x42, 0x00, 0x1f, 0xff, 0xe1 };
    EXPECT_FALSE(findAVCCMaxNumReorderFrames(std::span { avcc }).has_value());
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesRejectsZeroSequenceParameterSets)
{
    // numOfSequenceParameterSets' low 5 bits are 0, meaning no SPS is present to parse.
    constexpr uint8_t avcc[] = { 0x01, 0x42, 0x00, 0x1f, 0xff, 0xe0, 0x00, 0x0d };
    EXPECT_FALSE(findAVCCMaxNumReorderFrames(std::span { avcc }).has_value());
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesRejectsSpsSizeOfJustTheNaluHeader)
{
    // SPS size of 1 leaves no room for any SPS payload beyond the NAL unit header byte.
    constexpr uint8_t avcc[] = { 0x01, 0x42, 0x00, 0x1f, 0xff, 0xe1, 0x00, 0x01, 0x67 };
    EXPECT_FALSE(findAVCCMaxNumReorderFrames(std::span { avcc }).has_value());
}

TEST(H264Utilities, FindAVCCMaxNumReorderFramesRejectsTruncatedSps)
{
    // The SPS size field claims more bytes than actually follow in the record.
    auto sps = std::span { kH264SpsPps }.subspan(4, 13);
    auto avcc = makeAVCCWithSingleSps(sps);
    avcc[6] = 0xff;
    avcc[7] = 0xff;
    EXPECT_FALSE(findAVCCMaxNumReorderFrames(avcc.span()).has_value());
}

}
