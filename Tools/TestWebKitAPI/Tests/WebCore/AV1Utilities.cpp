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
#include <wtf/Vector.h>

namespace TestWebKitAPI {

// Minimal MSB-first bit writer used to assemble synthetic AV1 OBUs for the parser tests.
class BitWriter {
public:
    void writeBit(bool b)
    {
        if (!m_remainingBits) {
            m_bytes.append(0);
            m_remainingBits = 8;
        }
        if (b)
            m_bytes.last() |= (1u << (m_remainingBits - 1));
        --m_remainingBits;
    }

    void write(uint64_t value, size_t bits)
    {
        for (size_t i = 0; i < bits; ++i)
            writeBit((value >> (bits - 1 - i)) & 1);
    }

    Vector<uint8_t> takeBytes() { return std::exchange(m_bytes, { }); }

private:
    Vector<uint8_t> m_bytes;
    size_t m_remainingBits { 0 };
};

// Builds the simplest valid AV1 sequence header payload: profile 0, 8-bit 4:2:0, single
// operating point at level 2.0, no timing info, no decoder-model info, no order hints.
static Vector<uint8_t> buildSequenceHeaderPayload(uint32_t maxWidth, uint32_t maxHeight, size_t widthBitsMinus1, size_t heightBitsMinus1)
{
    BitWriter w;
    w.write(0, 3);     // seq_profile
    w.writeBit(false); // still_picture
    w.writeBit(false); // reduced_still_picture_header
    w.writeBit(false); // timing_info_present_flag
    w.writeBit(false); // initial_display_delay_present_flag
    w.write(0, 5);     // operating_points_cnt_minus_1
    w.write(0, 12);    // operating_point_idc[0]
    w.write(0, 5);     // seq_level_idx[0] (level 2.0; no tier byte since <= 7)
    w.write(widthBitsMinus1, 4);
    w.write(heightBitsMinus1, 4);
    w.write(maxWidth - 1, widthBitsMinus1 + 1);
    w.write(maxHeight - 1, heightBitsMinus1 + 1);
    w.writeBit(false); // frame_id_numbers_present_flag
    w.writeBit(false); // use_128x128_superblock
    w.writeBit(false); // enable_filter_intra
    w.writeBit(false); // enable_intra_edge_filter
    w.writeBit(false); // enable_interintra_compound
    w.writeBit(false); // enable_masked_compound
    w.writeBit(false); // enable_warped_motion
    w.writeBit(false); // enable_dual_filter
    w.writeBit(false); // enable_order_hint
    w.writeBit(true);  // seq_choose_screen_content_tools (=> seq_force_screen_content_tools = SELECT)
    w.writeBit(true);  // seq_choose_integer_mv (=> seq_force_integer_mv = SELECT)
    w.writeBit(false); // enable_superres
    w.writeBit(false); // enable_cdef
    w.writeBit(false); // enable_restoration
    w.writeBit(false); // high_bitdepth
    w.writeBit(false); // monochrome
    w.writeBit(false); // color_description_present_flag
    w.writeBit(false); // color_range
    w.write(0, 2);     // chroma_sample_position (4:2:0 in profile 0 reads CSP)
    return w.takeBytes();
}

// Builds an uncompressed_header() payload for a KEY_FRAME with show_frame=1 sized to use the
// minimum number of fields we can construct without a full reference-frame model.
static Vector<uint8_t> buildKeyFrameHeaderPayload(uint32_t width, uint32_t height, size_t widthBitsMinus1, size_t heightBitsMinus1, bool sizeOverride)
{
    BitWriter w;
    w.writeBit(false); // show_existing_frame
    w.write(0, 2);     // frame_type = KEY_FRAME
    w.writeBit(true);  // show_frame
    // showable_frame: not read because show_frame == 1
    // error_resilient_mode: not read because (frame_type == KEY && show_frame)
    w.writeBit(false); // disable_cdf_update
    w.writeBit(false); // allow_screen_content_tools (SELECT path)
    // force_integer_mv: not read because allow_screen_content_tools == 0
    // current_frame_id: not read because frame_id_numbers_present_flag == 0
    w.writeBit(sizeOverride); // frame_size_override_flag
    // order_hint: skipped because OrderHintBits == 0
    // primary_ref_frame: skipped because FrameIsIntra && error_resilient_mode
    // decoder_model_info: skipped (not present)
    // refresh_frame_flags: not read because (KEY && show_frame) implies allFrames
    // ref_order_hint loop: not read (allFrames already, FrameIsIntra)
    if (sizeOverride) {
        w.write(width - 1, widthBitsMinus1 + 1);
        w.write(height - 1, heightBitsMinus1 + 1);
    }
    return w.takeBytes();
}

// Wraps `payload` with an OBU header (forbidden=0, ext=0, has_size=1) and a ULEB128 size.
static Vector<uint8_t> wrapInOBU(uint8_t obuType, std::span<const uint8_t> payload)
{
    Vector<uint8_t> out;
    out.append(static_cast<uint8_t>((obuType << 3) | 0x02));
    size_t size = payload.size();
    while (size >= 0x80) {
        out.append(static_cast<uint8_t>((size & 0x7F) | 0x80));
        size >>= 7;
    }
    out.append(static_cast<uint8_t>(size));
    out.append(payload);
    return out;
}

static Vector<uint8_t> buildBitstream(uint32_t maxWidth, uint32_t maxHeight, std::optional<std::pair<uint32_t, uint32_t>> frameSize, uint8_t frameOBUType, size_t widthBitsMinus1 = 12, size_t heightBitsMinus1 = 12)
{
    auto seqPayload = buildSequenceHeaderPayload(maxWidth, maxHeight, widthBitsMinus1, heightBitsMinus1);
    auto seqOBU = wrapInOBU(/* OBU_SEQUENCE_HEADER */ 1, seqPayload.span());

    Vector<uint8_t> bitstream;
    bitstream.append(seqOBU.span());

    if (frameOBUType) {
        auto framePayload = buildKeyFrameHeaderPayload(
            frameSize ? frameSize->first : maxWidth,
            frameSize ? frameSize->second : maxHeight,
            widthBitsMinus1, heightBitsMinus1,
            frameSize.has_value());
        auto frameOBU = wrapInOBU(frameOBUType, framePayload.span());
        bitstream.append(frameOBU.span());
    }
    return bitstream;
}

TEST(AV1Utilities, CreateVideoInfo_FallsBackToMaxWhenNoOverride)
{
    auto bitstream = buildBitstream(8192, 8192, std::nullopt, /* OBU_FRAME_HEADER */ 3);
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(bitstream.span());
    ASSERT_TRUE(videoInfo);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().width()), 8192u);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().height()), 8192u);
}

TEST(AV1Utilities, CreateVideoInfo_HonoursFrameSizeOverride)
{
    auto bitstream = buildBitstream(8192, 8192, { { 1920, 1080 } }, /* OBU_FRAME_HEADER */ 3);
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(bitstream.span());
    ASSERT_TRUE(videoInfo);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().width()), 1920u);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().height()), 1080u);
}

TEST(AV1Utilities, CreateVideoInfo_FrameSizeOverrideInsideOBU_FRAME)
{
    // OBU_FRAME (type 6) carries the same uncompressed_header() bytes; verify the parser
    // honours the override regardless of whether it was a header-only or combined OBU.
    auto bitstream = buildBitstream(4096, 4096, { { 640, 360 } }, /* OBU_FRAME */ 6);
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(bitstream.span());
    ASSERT_TRUE(videoInfo);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().width()), 640u);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().height()), 360u);
}

TEST(AV1Utilities, CreateVideoInfo_RedundantFrameHeaderHonoured)
{
    auto bitstream = buildBitstream(2048, 2048, { { 800, 600 } }, /* OBU_REDUNDANT_FRAME_HEADER */ 7);
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(bitstream.span());
    ASSERT_TRUE(videoInfo);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().width()), 800u);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().height()), 600u);
}

TEST(AV1Utilities, CreateVideoInfo_RejectsFrameSizeAboveMax)
{
    // Per-frame size larger than the sequence-header max should be ignored and the parser
    // should fall back to max — never report a width that exceeds frame_width_bits_minus_1.
    auto bitstream = buildBitstream(1920, 1080, { { 4096, 4096 } }, /* OBU_FRAME_HEADER */ 3, 12, 12);
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(bitstream.span());
    ASSERT_TRUE(videoInfo);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().width()), 1920u);
    EXPECT_EQ(static_cast<uint32_t>(videoInfo->size().height()), 1080u);
}

TEST(AV1Utilities, CreateVideoInfo_NoSequenceHeaderReturnsNull)
{
    // A frame OBU on its own (no sequence header in the same buffer) should fail to produce
    // a VideoInfo because the codec record cannot be reconstructed.
    auto framePayload = buildKeyFrameHeaderPayload(1920, 1080, 12, 12, true);
    auto frameOBU = wrapInOBU(/* OBU_FRAME_HEADER */ 3, framePayload.span());
    auto videoInfo = WebCore::createVideoInfoFromAV1Stream(frameOBU.span());
    EXPECT_FALSE(videoInfo);
}

}
