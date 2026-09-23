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
#include "H264Utilities.h"

#include "AnnexBUtilities.h"
#include "BitReader.h"
#include "Logging.h"
#include <bit>
#include <cstdlib>
#include <limits>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

// H264BitstreamParser is ported from libwebrtc common_video/h264/pps_parser.cc.
WTF_MAKE_TZONE_ALLOCATED_IMPL(H264BitstreamParser);

H264NaluType h264NaluType(uint8_t data)
{
    constexpr uint8_t naluTypeMask = 0x1F;
    return static_cast<H264NaluType>(data & naluTypeMask);
}

auto H264BitstreamParser::parseSps(std::span<const uint8_t> data) -> std::optional<SpsState>
{
    auto rbsp = parseRbsp(data);
    BitReader reader(rbsp.span());

    SpsState sps;

    // profile_idc: u(8)
    uint32_t profileIDC = reader.readBits(8);
    // constraint_set0_flag..constraint_set5_flag + reserved_zero_2bits + level_idc: u(16)
    reader.consumeBits(16);
    // seq_parameter_set_id: ue(v)
    sps.id = reader.readExpGolomb();

    if (profileIDC == 100 || profileIDC == 110 || profileIDC == 122 || profileIDC == 244 || profileIDC == 44
        || profileIDC == 83 || profileIDC == 86 || profileIDC == 118 || profileIDC == 128 || profileIDC == 138
        || profileIDC == 139 || profileIDC == 134) {
        // chroma_format_idc: ue(v)
        sps.chromaFormatIDC = reader.readExpGolomb();
        if (sps.chromaFormatIDC == 3) {
            // separate_colour_plane_flag: u(1)
            sps.separateColourPlaneFlag = reader.readFlag();
        }
        // bit_depth_luma_minus8: ue(v)
        reader.readExpGolomb();
        // bit_depth_chroma_minus8: ue(v)
        reader.readExpGolomb();
        // qpprime_y_zero_transform_bypass_flag: u(1)
        reader.consumeBits(1);
        // seq_scaling_matrix_present_flag: u(1)
        if (reader.readFlag()) {
            int scalingListCount = sps.chromaFormatIDC == 3 ? 12 : 8;
            for (int i = 0; i < scalingListCount; ++i) {
                // seq_scaling_list_present_flag[i]: u(1)
                if (!reader.readFlag())
                    continue;
                int lastScale = 8;
                int nextScale = 8;
                int sizeOfScalingList = i < 6 ? 16 : 64;
                for (int j = 0; j < sizeOfScalingList; ++j) {
                    if (nextScale) {
                        // delta_scale: se(v)
                        int deltaScale = reader.readSignedExpGolomb();
                        if (!reader.ok() || deltaScale < -128 || deltaScale > 127)
                            return { };
                        nextScale = (lastScale + deltaScale + 256) % 256;
                    }
                    if (nextScale)
                        lastScale = nextScale;
                }
            }
        }
    }

    constexpr uint32_t maxLog2Minus4 = 12;

    // log2_max_frame_num_minus4: ue(v)
    uint32_t log2MaxFrameNumMinus4 = reader.readExpGolomb();
    if (!reader.ok() || log2MaxFrameNumMinus4 > maxLog2Minus4)
        return { };
    sps.log2MaxFrameNum = log2MaxFrameNumMinus4 + 4;

    // pic_order_cnt_type: ue(v)
    sps.picOrderCntType = reader.readExpGolomb();
    if (sps.picOrderCntType == 0) {
        // log2_max_pic_order_cnt_lsb_minus4: ue(v)
        uint32_t log2MaxPicOrderCntLsbMinus4 = reader.readExpGolomb();
        if (!reader.ok() || log2MaxPicOrderCntLsbMinus4 > maxLog2Minus4)
            return { };
        sps.log2MaxPicOrderCntLsb = log2MaxPicOrderCntLsbMinus4 + 4;
    } else if (sps.picOrderCntType == 1) {
        // delta_pic_order_always_zero_flag: u(1)
        sps.deltaPicOrderAlwaysZeroFlag = reader.readFlag();
        // offset_for_non_ref_pic: se(v)
        reader.readSignedExpGolomb();
        // offset_for_top_to_bottom_field: se(v)
        reader.readSignedExpGolomb();
        // num_ref_frames_in_pic_order_cnt_cycle: ue(v)
        uint32_t numRefFramesInPicOrderCntCycle = reader.readExpGolomb();
        for (uint32_t i = 0; i < numRefFramesInPicOrderCntCycle; ++i) {
            // offset_for_ref_frame[i]: se(v)
            reader.readSignedExpGolomb();
            if (!reader.ok())
                return { };
        }
    }
    // max_num_ref_frames: ue(v)
    sps.maxNumRefFrames = reader.readExpGolomb();
    // gaps_in_frame_num_value_allowed_flag: u(1)
    reader.consumeBits(1);
    // pic_width_in_mbs_minus1: ue(v)
    reader.readExpGolomb();
    // pic_height_in_map_units_minus1: ue(v)
    reader.readExpGolomb();
    // frame_mbs_only_flag: u(1)
    sps.frameMbsOnlyFlag = reader.readFlag();

    if (!reader.ok())
        return { };

    return sps;
}

auto H264BitstreamParser::parsePps(std::span<const uint8_t> data) -> std::optional<PpsState>
{
    auto rbsp = parseRbsp(data);
    BitReader reader(rbsp.span());

    PpsState pps;
    pps.id = reader.readExpGolomb();
    pps.spsId = reader.readExpGolomb();

    // entropy_coding_mode_flag: u(1)
    pps.entropyCodingModeFlag = reader.readFlag();
    // bottom_field_pic_order_in_frame_present_flag: u(1)
    pps.bottomFieldPicOrderInFramePresentFlag = reader.readFlag();

    // num_slice_groups_minus1: ue(v)
    uint32_t numSliceGroupsMinus1 = reader.readExpGolomb();
    if (numSliceGroupsMinus1 > 0) {
        // slice_group_map_type: ue(v)
        uint32_t sliceGroupMapType = reader.readExpGolomb();
        if (!sliceGroupMapType) {
            for (uint32_t i = 0; i <= numSliceGroupsMinus1 && reader.ok(); ++i) {
                // run_length_minus1[i]: ue(v)
                reader.readExpGolomb();
            }
        } else if (sliceGroupMapType == 2) {
            for (uint32_t i = 0; i <= numSliceGroupsMinus1 && reader.ok(); ++i) {
                // top_left[i]: ue(v)
                reader.readExpGolomb();
                // bottom_right[i]: ue(v)
                reader.readExpGolomb();
            }
        } else if (sliceGroupMapType == 3 || sliceGroupMapType == 4 || sliceGroupMapType == 5) {
            // slice_group_change_direction_flag: u(1)
            reader.consumeBits(1);
            // slice_group_change_rate_minus1: ue(v)
            reader.readExpGolomb();
        } else if (sliceGroupMapType == 6) {
            // pic_size_in_map_units_minus1: ue(v)
            uint32_t picSizeInMapUnits = reader.readExpGolomb() + 1;
            int sliceGroupIdBits = 1 + std::bit_width(numSliceGroupsMinus1);
            int64_t bitsToConsume = static_cast<int64_t>(sliceGroupIdBits) * picSizeInMapUnits;
            if (!reader.ok() || bitsToConsume > std::numeric_limits<int>::max())
                return { };
            reader.consumeBits(bitsToConsume);
        }
        // slice_group_map_type == 1 (dispersed) is intentionally unsupported, matching upstream.
    }
    // num_ref_idx_l0_default_active_minus1: ue(v)
    pps.numRefIdxL0DefaultActiveMinus1 = reader.readExpGolomb();
    // num_ref_idx_l1_default_active_minus1: ue(v)
    pps.numRefIdxL1DefaultActiveMinus1 = reader.readExpGolomb();
    if (!reader.ok() || pps.numRefIdxL0DefaultActiveMinus1 > 31 || pps.numRefIdxL1DefaultActiveMinus1 > 31)
        return { };

    // weighted_pred_flag: u(1)
    pps.weightedPredFlag = reader.readFlag();
    // weighted_bipred_idc: u(2)
    pps.weightedBipredIDC = reader.readBits(2);

    // pic_init_qp_minus26: se(v)
    pps.picInitQPMinus26 = reader.readSignedExpGolomb();
    if (!reader.ok() || pps.picInitQPMinus26 > 25 || pps.picInitQPMinus26 < -26)
        return { };
    // pic_init_qs_minus26: se(v)
    reader.readExpGolomb();
    // chroma_qp_index_offset: se(v)
    reader.readExpGolomb();
    // deblocking_filter_control_present_flag: u(1)
    // constrained_intra_pred_flag: u(1)
    reader.consumeBits(2);
    // redundant_pic_cnt_present_flag: u(1)
    pps.redundantPicCntPresentFlag = reader.readFlag();

    if (!reader.ok())
        return { };

    return pps;
}

auto H264BitstreamParser::parseNonParameterSetNalu(std::span<const uint8_t> source, uint8_t naluType) -> ParseResult
{
    if (!m_sps || !m_pps)
        return ParseResult::InvalidStream;

    m_lastSliceQPDelta = { };

    auto sliceRbsp = parseRbsp(source);
    if (sliceRbsp.size() < h264NaluHeaderSize)
        return ParseResult::InvalidStream;

    BitReader reader(sliceRbsp.span());
    reader.consumeBits(h264NaluHeaderSize * 8);

    bool isIdr = naluType == static_cast<uint8_t>(H264NaluType::Idr);
    uint8_t nalRefIdc = (source[0] & 0x60) >> 5;

    uint32_t numRefIdxL0ActiveMinus1 = m_pps->numRefIdxL0DefaultActiveMinus1;
    uint32_t numRefIdxL1ActiveMinus1 = m_pps->numRefIdxL1DefaultActiveMinus1;

    // first_mb_in_slice: ue(v)
    reader.readExpGolomb();
    // slice_type: ue(v)
    uint32_t sliceType = reader.readExpGolomb() % 5;
    // pic_parameter_set_id: ue(v)
    reader.readExpGolomb();
    if (m_sps->separateColourPlaneFlag) {
        // colour_plane_id: u(2)
        reader.consumeBits(2);
    }
    // frame_num: u(v)
    reader.consumeBits(m_sps->log2MaxFrameNum);
    bool fieldPicFlag = false;
    if (!m_sps->frameMbsOnlyFlag) {
        // field_pic_flag: u(1)
        fieldPicFlag = reader.readFlag();
        if (fieldPicFlag) {
            // bottom_field_flag: u(1)
            reader.consumeBits(1);
        }
    }
    if (isIdr) {
        // idr_pic_id: ue(v)
        reader.readExpGolomb();
    }
    if (!m_sps->picOrderCntType) {
        // pic_order_cnt_lsb: u(v)
        reader.consumeBits(m_sps->log2MaxPicOrderCntLsb);
        if (m_pps->bottomFieldPicOrderInFramePresentFlag && !fieldPicFlag) {
            // delta_pic_order_cnt_bottom: se(v)
            reader.readSignedExpGolomb();
        }
    }
    if (m_sps->picOrderCntType == 1 && !m_sps->deltaPicOrderAlwaysZeroFlag) {
        // delta_pic_order_cnt[0]: se(v)
        reader.readSignedExpGolomb();
        if (m_pps->bottomFieldPicOrderInFramePresentFlag && !fieldPicFlag) {
            // delta_pic_order_cnt[1]: se(v)
            reader.readSignedExpGolomb();
        }
    }
    if (m_pps->redundantPicCntPresentFlag) {
        // redundant_pic_cnt: ue(v)
        reader.readExpGolomb();
    }
    if (sliceType == static_cast<uint32_t>(H264SliceType::B)) {
        // direct_spatial_mv_pred_flag: u(1)
        reader.consumeBits(1);
    }
    switch (static_cast<H264SliceType>(sliceType)) {
    case H264SliceType::P:
    case H264SliceType::B:
    case H264SliceType::Sp:
        // num_ref_idx_active_override_flag: u(1)
        if (reader.readFlag()) {
            // num_ref_idx_l0_active_minus1: ue(v)
            numRefIdxL0ActiveMinus1 = reader.readExpGolomb();
            if (!reader.ok() || numRefIdxL0ActiveMinus1 > 31)
                return ParseResult::InvalidStream;
            if (sliceType == static_cast<uint32_t>(H264SliceType::B)) {
                // num_ref_idx_l1_active_minus1: ue(v)
                numRefIdxL1ActiveMinus1 = reader.readExpGolomb();
                if (!reader.ok() || numRefIdxL1ActiveMinus1 > 31)
                    return ParseResult::InvalidStream;
            }
        }
        break;
    default:
        break;
    }
    if (!reader.ok())
        return ParseResult::InvalidStream;

    if (naluType == 20 || naluType == 21) {
        RELEASE_LOG_ERROR(WebRTC, "H264BitstreamParser: unsupported nal unit type");
        return ParseResult::UnsupportedStream;
    }

    // ref_pic_list_modification():
    if (sliceType != 2 && sliceType != 4) {
        // ref_pic_list_modification_flag_l0: u(1)
        if (reader.readFlag()) {
            uint32_t modificationOfPicNumsIDC;
            do {
                // modification_of_pic_nums_idc: ue(v)
                modificationOfPicNumsIDC = reader.readExpGolomb();
                if (!modificationOfPicNumsIDC || modificationOfPicNumsIDC == 1) {
                    // abs_diff_pic_num_minus1: ue(v)
                    reader.readExpGolomb();
                } else if (modificationOfPicNumsIDC == 2) {
                    // long_term_pic_num: ue(v)
                    reader.readExpGolomb();
                }
            } while (modificationOfPicNumsIDC != 3 && reader.ok());
        }
    }
    if (sliceType == 1) {
        // ref_pic_list_modification_flag_l1: u(1)
        if (reader.readFlag()) {
            uint32_t modificationOfPicNumsIDC;
            do {
                // modification_of_pic_nums_idc: ue(v)
                modificationOfPicNumsIDC = reader.readExpGolomb();
                if (!modificationOfPicNumsIDC || modificationOfPicNumsIDC == 1) {
                    // abs_diff_pic_num_minus1: ue(v)
                    reader.readExpGolomb();
                } else if (modificationOfPicNumsIDC == 2) {
                    // long_term_pic_num: ue(v)
                    reader.readExpGolomb();
                }
            } while (modificationOfPicNumsIDC != 3 && reader.ok());
        }
    }
    if (!reader.ok())
        return ParseResult::InvalidStream;

    if ((m_pps->weightedPredFlag && (sliceType == static_cast<uint32_t>(H264SliceType::P) || sliceType == static_cast<uint32_t>(H264SliceType::Sp)))
        || (m_pps->weightedBipredIDC == 1 && sliceType == static_cast<uint32_t>(H264SliceType::B))) {
        // pred_weight_table()
        // luma_log2_weight_denom: ue(v)
        reader.readExpGolomb();

        uint8_t chromaArrayType = m_sps->separateColourPlaneFlag ? 0 : m_sps->chromaFormatIDC;
        if (chromaArrayType) {
            // chroma_log2_weight_denom: ue(v)
            reader.readExpGolomb();
        }

        for (uint32_t i = 0; i <= numRefIdxL0ActiveMinus1; ++i) {
            // luma_weight_l0_flag: u(1)
            if (reader.readFlag()) {
                // luma_weight_l0[i]: se(v)
                reader.readSignedExpGolomb();
                // luma_offset_l0[i]: se(v)
                reader.readSignedExpGolomb();
            }
            if (chromaArrayType) {
                // chroma_weight_l0_flag: u(1)
                if (reader.readFlag()) {
                    for (uint8_t j = 0; j < 2; ++j) {
                        // chroma_weight_l0[i][j]: se(v)
                        reader.readSignedExpGolomb();
                        // chroma_offset_l0[i][j]: se(v)
                        reader.readSignedExpGolomb();
                    }
                }
            }
        }
        if (sliceType == 1) {
            for (uint32_t i = 0; i <= numRefIdxL1ActiveMinus1; ++i) {
                // luma_weight_l1_flag: u(1)
                if (reader.readFlag()) {
                    // luma_weight_l1[i]: se(v)
                    reader.readSignedExpGolomb();
                    // luma_offset_l1[i]: se(v)
                    reader.readSignedExpGolomb();
                }
                if (chromaArrayType) {
                    // chroma_weight_l1_flag: u(1)
                    if (reader.readFlag()) {
                        for (uint8_t j = 0; j < 2; ++j) {
                            // chroma_weight_l1[i][j]: se(v)
                            reader.readSignedExpGolomb();
                            // chroma_offset_l1[i][j]: se(v)
                            reader.readSignedExpGolomb();
                        }
                    }
                }
            }
        }
    }
    if (nalRefIdc) {
        // dec_ref_pic_marking():
        if (isIdr) {
            // no_output_of_prior_pics_flag: u(1)
            // long_term_reference_flag: u(1)
            reader.consumeBits(2);
        } else if (reader.readFlag()) {
            // adaptive_ref_pic_marking_mode_flag: u(1)
            uint32_t memoryManagementControlOperation;
            do {
                // memory_management_control_operation: ue(v)
                memoryManagementControlOperation = reader.readExpGolomb();
                if (memoryManagementControlOperation == 1 || memoryManagementControlOperation == 3) {
                    // difference_of_pic_nums_minus1: ue(v)
                    reader.readExpGolomb();
                }
                if (memoryManagementControlOperation == 2) {
                    // long_term_pic_num: ue(v)
                    reader.readExpGolomb();
                }
                if (memoryManagementControlOperation == 3 || memoryManagementControlOperation == 6) {
                    // long_term_frame_idx: ue(v)
                    reader.readExpGolomb();
                }
                if (memoryManagementControlOperation == 4) {
                    // max_long_term_frame_idx_plus1: ue(v)
                    reader.readExpGolomb();
                }
            } while (memoryManagementControlOperation && reader.ok());
        }
    }
    if (m_pps->entropyCodingModeFlag && sliceType != static_cast<uint32_t>(H264SliceType::I) && sliceType != static_cast<uint32_t>(H264SliceType::Si)) {
        // cabac_init_idc: ue(v)
        reader.readExpGolomb();
    }

    int32_t lastSliceQPDelta = reader.readSignedExpGolomb();
    if (!reader.ok())
        return ParseResult::InvalidStream;
    if (std::abs(lastSliceQPDelta) > 51) {
        RELEASE_LOG_ERROR(WebRTC, "H264BitstreamParser: parsed QP delta out of range");
        return ParseResult::InvalidStream;
    }

    m_lastSliceQPDelta = lastSliceQPDelta;
    return ParseResult::Ok;
}

void H264BitstreamParser::parseSlice(std::span<const uint8_t> slice)
{
    if (slice.empty())
        return;

    auto naluType = h264NaluType(slice[0]);
    switch (naluType) {
    case H264NaluType::Sps:
        if (slice.size() >= h264NaluHeaderSize) {
            if (auto spsState = parseSps(slice.subspan(h264NaluHeaderSize)))
                m_sps = WTF::move(spsState);
        }
        break;
    case H264NaluType::Pps:
        if (slice.size() >= h264NaluHeaderSize) {
            if (auto ppsState = parsePps(slice.subspan(h264NaluHeaderSize)))
                m_pps = WTF::move(ppsState);
        }
        break;
    case H264NaluType::Aud:
    case H264NaluType::Filler:
    case H264NaluType::Sei:
    case H264NaluType::Prefix:
        break;
    default:
        parseNonParameterSetNalu(slice, static_cast<uint8_t>(naluType));
        break;
    }
}

void H264BitstreamParser::parseBitstream(std::span<const uint8_t> bitstream)
{
    for (auto& index : findNaluIndices(bitstream))
        parseSlice(bitstream.subspan(index.payloadStartOffset, index.payloadSize));
}

std::optional<int> H264BitstreamParser::lastSliceQP() const
{
    if (!m_lastSliceQPDelta || !m_pps) {
        RELEASE_LOG_ERROR(WebRTC, "H264BitstreamParser: unable to get qp - missing info");
        return { };
    }
    int qp = 26 + m_pps->picInitQPMinus26 + *m_lastSliceQPDelta;
    if (qp < 0 || qp > 51) {
        RELEASE_LOG_ERROR(WebRTC, "H264BitstreamParser: parsed invalid QP from bitstream");
        return { };
    }
    return qp;
}

}
