/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "sdk/objc/components/video_codec/nalu_rewriter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "common_video/h264/sps_parser.h"

namespace webrtc {

using H264::kAud;
using H264::kSps;
using H264::NaluIndex;
using H264::NaluType;
using H264::ParseNaluType;

const char kAnnexBHeaderBytes[4] = {0, 0, 0, 1};
const size_t kAvccHeaderByteSize = sizeof(uint32_t);

bool H264CMSampleBufferToAnnexBBuffer(CMSampleBufferRef avcc_sample_buffer,
                                      bool is_keyframe,
                                      rtc::Buffer* annexb_buffer) {
  RTC_DCHECK(avcc_sample_buffer);

  // Get format description from the sample buffer.
  CMVideoFormatDescriptionRef description =
      CMSampleBufferGetFormatDescription(avcc_sample_buffer);
  if (description == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's description.";
    return false;
  }

  // Get parameter set information.
  int nalu_header_size = 0;
  size_t param_set_count = 0;
  OSStatus status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
      description, 0, nullptr, nullptr, &param_set_count, &nalu_header_size);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
    return false;
  }
  RTC_CHECK_EQ(nalu_header_size, kAvccHeaderByteSize);
  RTC_DCHECK_EQ(param_set_count, 2);

  // Truncate any previous data in the buffer without changing its capacity.
  annexb_buffer->SetSize(0);

  // Place all parameter sets at the front of buffer.
  if (is_keyframe) {
    size_t param_set_size = 0;
    const uint8_t* param_set = nullptr;
    for (size_t i = 0; i < param_set_count; ++i) {
      status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          description, i, &param_set, &param_set_size, nullptr, nullptr);
      if (status != noErr) {
        RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
        return false;
      }
      // Update buffer.
      annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
      annexb_buffer->AppendData(reinterpret_cast<const char*>(param_set),
                                param_set_size);
    }
  }

  // Get block buffer from the sample buffer.
  CMBlockBufferRef block_buffer =
      CMSampleBufferGetDataBuffer(avcc_sample_buffer);
  if (block_buffer == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's block buffer.";
    return false;
  }
  CMBlockBufferRef contiguous_buffer = nullptr;
  // Make sure block buffer is contiguous.
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(
        nullptr, block_buffer, nullptr, nullptr, 0, 0, 0, &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    // Retain to make cleanup easier.
    CFRetain(contiguous_buffer);
    block_buffer = nullptr;
  }

  // Now copy the actual data.
  char* data_ptr = nullptr;
  size_t block_buffer_size = CMBlockBufferGetDataLength(contiguous_buffer);
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr, nullptr,
                                       &data_ptr);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data.";
    CFRelease(contiguous_buffer);
    return false;
  }
  size_t bytes_remaining = block_buffer_size;
  while (bytes_remaining > 0) {
    // The size type here must match |nalu_header_size|, we expect 4 bytes.
    // Read the length of the next packet of data. Must convert from big endian
    // to host endian.
    RTC_DCHECK_GE(bytes_remaining, (size_t)nalu_header_size);
    uint32_t* uint32_data_ptr = reinterpret_cast<uint32_t*>(data_ptr);
    uint32_t packet_size = CFSwapInt32BigToHost(*uint32_data_ptr);
    // Update buffer.
    annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
    annexb_buffer->AppendData(data_ptr + nalu_header_size, packet_size);

    size_t bytes_written = packet_size + sizeof(kAnnexBHeaderBytes);
    bytes_remaining -= bytes_written;
    data_ptr += bytes_written;
  }
  RTC_DCHECK_EQ(bytes_remaining, (size_t)0);

  CFRelease(contiguous_buffer);
  return true;
}

bool H264AnnexBBufferToCMSampleBuffer(const uint8_t* annexb_buffer,
                                      size_t annexb_buffer_size,
                                      CMVideoFormatDescriptionRef video_format,
                                      CMSampleBufferRef* out_sample_buffer,
                                      CMMemoryPoolRef memory_pool) {
  RTC_DCHECK(annexb_buffer);
  RTC_DCHECK(out_sample_buffer);
  RTC_DCHECK(video_format);
  *out_sample_buffer = nullptr;

  AnnexBBufferReader reader(annexb_buffer, annexb_buffer_size);
  if (reader.SeekToNextNaluOfType(kSps)) {
    // Buffer contains an SPS NALU - skip it and the following PPS
    const uint8_t* data;
    size_t data_len;
    if (!reader.ReadNalu(&data, &data_len)) {
      RTC_LOG(LS_ERROR) << "Failed to read SPS";
      return false;
    }
    if (!reader.ReadNalu(&data, &data_len)) {
      RTC_LOG(LS_ERROR) << "Failed to read PPS";
      return false;
    }
  } else {
    // No SPS NALU - start reading from the first NALU in the buffer
    reader.SeekToStart();
  }

  // Allocate memory as a block buffer.
  CMBlockBufferRef block_buffer = nullptr;
  CFAllocatorRef block_allocator = CMMemoryPoolGetAllocator(memory_pool);
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault, nullptr, reader.BytesRemainingForAVC(), block_allocator,
      nullptr, 0, reader.BytesRemainingForAVC(), kCMBlockBufferAssureMemoryNowFlag,
      &block_buffer);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to create block buffer.";
    return false;
  }

  // Make sure block buffer is contiguous.
  CMBlockBufferRef contiguous_buffer = nullptr;
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(kCFAllocatorDefault, block_buffer,
                                           block_allocator, nullptr, 0, 0, 0,
                                           &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      CFRelease(block_buffer);
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    block_buffer = nullptr;
  }

  // Get a raw pointer into allocated memory.
  size_t block_buffer_size = 0;
  char* data_ptr = nullptr;
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr,
                                       &block_buffer_size, &data_ptr);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data pointer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  RTC_DCHECK(block_buffer_size == reader.BytesRemainingForAVC());

  // Write Avcc NALUs into block buffer memory.
  AvccBufferWriter writer(reinterpret_cast<uint8_t*>(data_ptr),
                          block_buffer_size);
  while (reader.BytesRemaining() > 0) {
    const uint8_t* nalu_data_ptr = nullptr;
    size_t nalu_data_size = 0;
    if (reader.ReadNalu(&nalu_data_ptr, &nalu_data_size)) {
      writer.WriteNalu(nalu_data_ptr, nalu_data_size);
    }
  }

  // Create sample buffer.
  status = CMSampleBufferCreate(kCFAllocatorDefault, contiguous_buffer, true,
                                nullptr, nullptr, video_format, 1, 0, nullptr,
                                0, nullptr, out_sample_buffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create sample buffer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  CFRelease(contiguous_buffer);
  return true;
}

#ifndef DISABLE_H265
bool H265CMSampleBufferToAnnexBBuffer(
    CMSampleBufferRef hvcc_sample_buffer,
    bool is_keyframe,
    rtc::Buffer* annexb_buffer) {
  RTC_DCHECK(hvcc_sample_buffer);

  // Get format description from the sample buffer.
  CMVideoFormatDescriptionRef description =
      CMSampleBufferGetFormatDescription(hvcc_sample_buffer);
  if (description == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's description.";
    return false;
  }

  // Get parameter set information.
  int nalu_header_size = 0;
  size_t param_set_count = 0;
  OSStatus status = CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(
      description, 0, nullptr, nullptr, &param_set_count, &nalu_header_size);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
    return false;
  }
  RTC_CHECK_EQ(nalu_header_size, kAvccHeaderByteSize);
  RTC_DCHECK_EQ(param_set_count, 3);

  // Truncate any previous data in the buffer without changing its capacity.
  annexb_buffer->SetSize(0);

  size_t nalu_offset = 0;
  std::vector<size_t> frag_offsets;
  std::vector<size_t> frag_lengths;

  // Place all parameter sets at the front of buffer.
  if (is_keyframe) {
    size_t param_set_size = 0;
    const uint8_t* param_set = nullptr;
    for (size_t i = 0; i < param_set_count; ++i) {
      status = CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(
          description, i, &param_set, &param_set_size, nullptr, nullptr);
      if (status != noErr) {
        RTC_LOG(LS_ERROR) << "Failed to get parameter set.";
        return false;
      }
      // Update buffer.
      annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
      annexb_buffer->AppendData(reinterpret_cast<const char*>(param_set),
                                param_set_size);
      // Update fragmentation.
      frag_offsets.push_back(nalu_offset + sizeof(kAnnexBHeaderBytes));
      frag_lengths.push_back(param_set_size);
      nalu_offset += sizeof(kAnnexBHeaderBytes) + param_set_size;
    }
  }

  // Get block buffer from the sample buffer.
  CMBlockBufferRef block_buffer =
      CMSampleBufferGetDataBuffer(hvcc_sample_buffer);
  if (block_buffer == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to get sample buffer's block buffer.";
    return false;
  }
  CMBlockBufferRef contiguous_buffer = nullptr;
  // Make sure block buffer is contiguous.
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(
        nullptr, block_buffer, nullptr, nullptr, 0, 0, 0, &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    // Retain to make cleanup easier.
    CFRetain(contiguous_buffer);
    block_buffer = nullptr;
  }

  // Now copy the actual data.
  char* data_ptr = nullptr;
  size_t block_buffer_size = CMBlockBufferGetDataLength(contiguous_buffer);
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr, nullptr,
                                       &data_ptr);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data.";
    CFRelease(contiguous_buffer);
    return false;
  }
  size_t bytes_remaining = block_buffer_size;
  while (bytes_remaining > 0) {
    // The size type here must match |nalu_header_size|, we expect 4 bytes.
    // Read the length of the next packet of data. Must convert from big endian
    // to host endian.
    RTC_DCHECK_GE(bytes_remaining, (size_t)nalu_header_size);
    uint32_t* uint32_data_ptr = reinterpret_cast<uint32_t*>(data_ptr);
    uint32_t packet_size = CFSwapInt32BigToHost(*uint32_data_ptr);
    // Update buffer.
    annexb_buffer->AppendData(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
    annexb_buffer->AppendData(data_ptr + nalu_header_size, packet_size);
    // Update fragmentation.
    frag_offsets.push_back(nalu_offset + sizeof(kAnnexBHeaderBytes));
    frag_lengths.push_back(packet_size);
    nalu_offset += sizeof(kAnnexBHeaderBytes) + packet_size;

    size_t bytes_written = packet_size + sizeof(kAnnexBHeaderBytes);
    bytes_remaining -= bytes_written;
    data_ptr += bytes_written;
  }
  RTC_DCHECK_EQ(bytes_remaining, (size_t)0);

  CFRelease(contiguous_buffer);

  return true;
}

bool H265AnnexBBufferToCMSampleBuffer(const uint8_t* annexb_buffer,
                                      size_t annexb_buffer_size,
                                      CMVideoFormatDescriptionRef video_format,
                                      CMSampleBufferRef* out_sample_buffer) {
  RTC_DCHECK(annexb_buffer);
  RTC_DCHECK(out_sample_buffer);
  RTC_DCHECK(video_format);
  *out_sample_buffer = nullptr;

  AnnexBBufferReader reader(annexb_buffer, annexb_buffer_size, false);
  if (reader.SeekToNextNaluOfType(H265::kVps)) {
    // Buffer contains an SPS NALU - skip it and the following PPS
    const uint8_t* data;
    size_t data_len;
    if (!reader.ReadNalu(&data, &data_len)) {
      RTC_LOG(LS_ERROR) << "Failed to read VPS";
      return false;
    }
    if (!reader.ReadNalu(&data, &data_len)) {
      RTC_LOG(LS_ERROR) << "Failed to read SPS";
      return false;
    }
    if (!reader.ReadNalu(&data, &data_len)) {
      RTC_LOG(LS_ERROR) << "Failed to read PPS";
      return false;
    }
  } else {
    // No SPS NALU - start reading from the first NALU in the buffer
    reader.SeekToStart();
  }

  // Allocate memory as a block buffer.
  // TODO(tkchin): figure out how to use a pool.
  CMBlockBufferRef block_buffer = nullptr;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      nullptr, nullptr, reader.BytesRemainingForAVC(), nullptr, nullptr, 0,
      reader.BytesRemainingForAVC(), kCMBlockBufferAssureMemoryNowFlag,
      &block_buffer);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to create block buffer.";
    return false;
  }

  // Make sure block buffer is contiguous.
  CMBlockBufferRef contiguous_buffer = nullptr;
  if (!CMBlockBufferIsRangeContiguous(block_buffer, 0, 0)) {
    status = CMBlockBufferCreateContiguous(
        nullptr, block_buffer, nullptr, nullptr, 0, 0, 0, &contiguous_buffer);
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to flatten non-contiguous block buffer: "
                        << status;
      CFRelease(block_buffer);
      return false;
    }
  } else {
    contiguous_buffer = block_buffer;
    block_buffer = nullptr;
  }

  // Get a raw pointer into allocated memory.
  size_t block_buffer_size = 0;
  char* data_ptr = nullptr;
  status = CMBlockBufferGetDataPointer(contiguous_buffer, 0, nullptr,
                                       &block_buffer_size, &data_ptr);
  if (status != kCMBlockBufferNoErr) {
    RTC_LOG(LS_ERROR) << "Failed to get block buffer data pointer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  RTC_DCHECK(block_buffer_size == reader.BytesRemainingForAVC());

  // Write Hvcc NALUs into block buffer memory.
  AvccBufferWriter writer(reinterpret_cast<uint8_t*>(data_ptr),
                          block_buffer_size);
  while (reader.BytesRemaining() > 0) {
    const uint8_t* nalu_data_ptr = nullptr;
    size_t nalu_data_size = 0;
    if (reader.ReadNalu(&nalu_data_ptr, &nalu_data_size)) {
      writer.WriteNalu(nalu_data_ptr, nalu_data_size);
    }
  }

  // Create sample buffer.
  status = CMSampleBufferCreate(nullptr, contiguous_buffer, true, nullptr,
                                nullptr, video_format, 1, 0, nullptr, 0,
                                nullptr, out_sample_buffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create sample buffer.";
    CFRelease(contiguous_buffer);
    return false;
  }
  CFRelease(contiguous_buffer);
  return true;
}
#endif

CMVideoFormatDescriptionRef CreateVideoFormatDescription(
    const uint8_t* annexb_buffer,
    size_t annexb_buffer_size) {
  const uint8_t* param_set_ptrs[2] = {};
  size_t param_set_sizes[2] = {};
  AnnexBBufferReader reader(annexb_buffer, annexb_buffer_size);
  // Skip everyting before the SPS, then read the SPS and PPS
  if (!reader.SeekToNextNaluOfType(kSps)) {
    return nullptr;
  }
  if (!reader.ReadNalu(&param_set_ptrs[0], &param_set_sizes[0])) {
    RTC_LOG(LS_ERROR) << "Failed to read SPS";
    return nullptr;
  }
  if (!reader.ReadNalu(&param_set_ptrs[1], &param_set_sizes[1])) {
    RTC_LOG(LS_ERROR) << "Failed to read PPS";
    return nullptr;
  }

  // Parse the SPS and PPS into a CMVideoFormatDescription.
  CMVideoFormatDescriptionRef description = nullptr;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault, 2, param_set_ptrs, param_set_sizes, 4, &description);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create video format description.";
    return nullptr;
  }
  return description;
}

class SpsAndVuiParser : private SpsParser {
public:
    struct State : SpsState {
      explicit State(const SpsState& spsState)
        : SpsState(spsState)
      {
      }

      uint8_t profile_idc { 0 };
      uint8_t level_idc { 0 };
      bool constraint_set3_flag { false };
      bool bitstream_restriction_flag { false };
      uint64_t max_num_reorder_frames { 0 };
    };
    static absl::optional<State> Parse(const std::vector<uint8_t>& unpacked_buffer)
    {
      BitstreamReader reader(unpacked_buffer);
      auto spsState = ParseSpsUpToVui(reader);
      if (!spsState) {
          return { };
      }
      State result { *spsState };

      {
        // We are restarting parsing for some values we need and that ParseSpsUpToVui is not giving us.
        BitstreamReader reader2(unpacked_buffer);
        result.profile_idc = reader2.Read<uint8_t>();
        // constraint_set0_flag, constraint_set1_flag, constraint_set2_flag
        reader2.ConsumeBits(3);
        result.constraint_set3_flag = reader2.Read<bool>();
        // constraint_set4_flag, constraint_set5_flag and reserved bits (2)
        reader2.ConsumeBits(4);
        result.level_idc = reader2.Read<uint8_t>();
        if (!reader2.Ok()) {
          return { };
        }
      }

      if (!spsState->vui_params_present) {
        return result;
      }
      // Based on ANNEX VUI parameters syntax.

      // aspect_ratio_info_present_flag
      if (reader.Read<bool>()) {
        // aspect_ratio_idc
        auto aspect_ratio_idc = reader.Read<uint8_t>();
        // FIXME Extended_SAR
        constexpr uint64_t extendedSar = 255;
        if (aspect_ratio_idc == extendedSar) {
          // sar_width
          reader.ConsumeBits(16);
          // sar_height
          reader.ConsumeBits(16);
        }
      }
      // overscan_info_present_flag
      if (reader.Read<bool>()) {
        // overscan_appropriate_flag
        reader.ConsumeBits(1);
      }
      // video_signal_type_present_flag
      if (reader.Read<bool>()) {
        // video_format
        reader.ConsumeBits(3);
        // video_full_range_flag
        reader.ConsumeBits(1);
        // colour_description_present_flag
        if (reader.Read<bool>()) {
          // colour_primaries
          reader.ConsumeBits(8);
          // transfer_characteristics
          reader.ConsumeBits(8);
          // matrix_coefficients
          reader.ConsumeBits(8);
        }
      }
      // chroma_loc_info_present_flag
      if (reader.Read<bool>()) {
          // chroma_sample_loc_type_top_field
          reader.ReadExponentialGolomb();
          // chroma_sample_loc_type_bottom_field
          reader.ReadExponentialGolomb();
      }
      // timing_info_present_flag
      if (reader.Read<bool>()) {
        // num_units_in_tick
        reader.ConsumeBits(32);
        // time_scale
        reader.ConsumeBits(32);
        // fixed_frame_rate_flag
        reader.ConsumeBits(1);
      }
      // nal_hrd_parameters_present_flag
      bool nal_hrd_parameters_present_flag = reader.Read<bool>();
      if (nal_hrd_parameters_present_flag) {
        // hrd_parameters
        skipHRDParameters(reader);
      }
      // vcl_hrd_parameters_present_flag
      bool vcl_hrd_parameters_present_flag = reader.Read<bool>();
      if (vcl_hrd_parameters_present_flag) {
        // hrd_parameters
        skipHRDParameters(reader);
      }
      if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        // low_delay_hrd_flag
        reader.ConsumeBits(1);
      }
      // pic_struct_present_flag
      reader.ConsumeBits(1);
      // bitstream_restriction_flag
      result.bitstream_restriction_flag = reader.Read<bool>();
      if (result.bitstream_restriction_flag) {
        // motion_vectors_over_pic_boundaries_flag
        reader.ConsumeBits(1);
        // max_bytes_per_pic_denom
        reader.ReadExponentialGolomb();
        // max_bits_per_mb_denom
        reader.ReadExponentialGolomb();
        // log2_max_mv_length_horizontal
        reader.ReadExponentialGolomb();
        // log2_max_mv_length_vertical
        reader.ReadExponentialGolomb();
        // max_num_reorder_frames
        result.max_num_reorder_frames = reader.ReadExponentialGolomb();
        // max_dec_frame_buffering
        reader.ReadExponentialGolomb();
      }

      if (!reader.Ok()) {
          return { };
      }
      return result;
    }

    static void skipHRDParameters(BitstreamReader& reader)
    {
        // cpb_cnt_minus1
        auto cpb_cnt_minus1 = reader.ReadExponentialGolomb();
        // bit_rate_scale
        // cpb_size_scale
        reader.ConsumeBits(8);
        for (size_t cptr = 0; cptr <= cpb_cnt_minus1; ++cptr) {
            // bit_rate_value_minus1
            reader.ReadExponentialGolomb();
            // cpb_size_value_minus1
            reader.ReadExponentialGolomb();
            // cbr_flag
            reader.ConsumeBits(1);
        }
        // initial_cpb_removal_delay_length_minus1
        // cpb_removal_delay_length_minus1
        // dpb_output_delay_length_minus1
        // time_offset_length
        reader.ConsumeBits(20);
    }
};

// Table A-1 of H.264 spec
static size_t maxDpbMbsFromLevelNumber(uint8_t profile_idc, uint8_t level_idc, bool constraint_set3_flag)
{
  if ((profile_idc == 66 || profile_idc == 77) && level_idc == 11 && constraint_set3_flag) {
    // level1b
    return 396;
  }
  H264Level level_casted = static_cast<H264Level>(level_idc);

  switch (level_casted) {
  case H264Level::kLevel1:
    return 396;
  case H264Level::kLevel1_1:
    return 900;
  case H264Level::kLevel1_2:
  case H264Level::kLevel1_3:
  case H264Level::kLevel2:
    return 2376;
  case H264Level::kLevel2_1:
    return 4752;
  case H264Level::kLevel2_2:
  case H264Level::kLevel3:
    return 8100;
  case H264Level::kLevel3_1:
    return 18000;
  case H264Level::kLevel3_2:
    return 20480;
  case H264Level::kLevel4:
    return 32768;
  case H264Level::kLevel4_1:
    return 32768;
  case H264Level::kLevel4_2:
    return 34816;
  case H264Level::kLevel5:
    return 110400;
  case H264Level::kLevel5_1:
    return 184320;
  case H264Level::kLevel5_2:
    return 184320;
  default:
    RTC_LOG(LS_ERROR) << "Wrong maxDpbMbsFromLevelNumber";
    return 0;
  }
}

static uint8_t ComputeH264ReorderSizeFromSPS(const SpsAndVuiParser::State& state) {
  if (state.pic_order_cnt_type == 2) {
    return 0;
  }

  uint64_t max_dpb_mbs = maxDpbMbsFromLevelNumber(state.profile_idc, state.level_idc, state.constraint_set3_flag);
  uint64_t pic_width_in_mbs = state.pic_width_in_mbs_minus1 + 1;
  uint64_t frame_height_in_mbs = (2 - state.frame_mbs_only_flag) * (state.pic_height_in_map_units_minus1 + 1);
  uint64_t max_dpb_frames_from_sps = max_dpb_mbs / (pic_width_in_mbs * frame_height_in_mbs);
  // We use a max value of 16.
  auto max_dpb_frames = static_cast<uint8_t>(std::min(max_dpb_frames_from_sps, 16ull));

  if (state.bitstream_restriction_flag) {
    if (state.max_num_reorder_frames < max_dpb_frames) {
      return static_cast<uint8_t>(state.max_num_reorder_frames);
    } else {
      return max_dpb_frames;
    }
  }
  if (state.constraint_set3_flag && (state.profile_idc == 44 || state.profile_idc == 86 || state.profile_idc == 100 || state.profile_idc == 110 || state.profile_idc == 122 || state.profile_idc == 244)) {
    return 0;
  }
  return max_dpb_frames;
}

uint8_t ComputeH264ReorderSizeFromAnnexB(const uint8_t* annexb_buffer, size_t annexb_buffer_size) {
  AnnexBBufferReader reader(annexb_buffer, annexb_buffer_size);
  if (!reader.SeekToNextNaluOfType(kSps)) {
    return 0;
  }
  const uint8_t* spsData;
  size_t spsDataSize;

  if (!reader.ReadNalu(&spsData, &spsDataSize) || spsDataSize <= H264::kNaluTypeSize) {
    return 0;
  }

  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(spsData + H264::kNaluTypeSize, spsDataSize - H264::kNaluTypeSize);
  auto spsAndVui = SpsAndVuiParser::Parse(unpacked_buffer);
  if (!spsAndVui) {
    RTC_LOG(LS_ERROR) << "Failed to parse sps.";
    return 0;
  }

  return ComputeH264ReorderSizeFromSPS(*spsAndVui);
}

uint8_t ComputeH264ReorderSizeFromAVC(const uint8_t* avcData, size_t avcDataSize) {
  std::vector<uint8_t> unpacked_buffer { avcData, avcData + avcDataSize };
  BitstreamReader reader(unpacked_buffer);

  // configurationVersion
  reader.ConsumeBits(8);
  // AVCProfileIndication;
  reader.ConsumeBits(8);
  // profile_compatibility;
  reader.ConsumeBits(8);
  // AVCLevelIndication;
  reader.ConsumeBits(8);
  // bit(6) reserved = '111111'b;
  // unsigned int(2) lengthSizeMinusOne;
  reader.ConsumeBits(8);
  // bit(3) reserved = '111'b;
  // unsigned int(5) numOfSequenceParameterSets;
  auto numOfSequenceParameterSets = 0x1F & reader.Read<uint8_t>();

  if (!reader.Ok()) {
    return 0;
  }

  size_t offset = 6;
  if (numOfSequenceParameterSets) {
    auto size = reader.Read<uint16_t>();
    offset += 2;

    reader.ConsumeBits(8 * (size + H264::kNaluTypeSize));
    if (!reader.Ok()) {
      return 0;
    }

    auto spsAndVui = SpsAndVuiParser::Parse({ avcData + offset + H264::kNaluTypeSize, avcData + offset + size });
    if (spsAndVui) {
      return ComputeH264ReorderSizeFromSPS(*spsAndVui);
    }
  }
  return 0;
}

#ifndef DISABLE_H265
CMVideoFormatDescriptionRef CreateH265VideoFormatDescription(
    const uint8_t* annexb_buffer,
    size_t annexb_buffer_size) {
  const uint8_t* param_set_ptrs[3] = {};
  size_t param_set_sizes[3] = {};
  AnnexBBufferReader reader(annexb_buffer, annexb_buffer_size, false);
  // Skip everyting before the VPS, then read the VPS, SPS and PPS
  if (!reader.SeekToNextNaluOfType(H265::kVps)) {
    return nullptr;
  }
  if (!reader.ReadNalu(&param_set_ptrs[0], &param_set_sizes[0])) {
    RTC_LOG(LS_ERROR) << "Failed to read VPS";
    return nullptr;
  }
  if (!reader.ReadNalu(&param_set_ptrs[1], &param_set_sizes[1])) {
    RTC_LOG(LS_ERROR) << "Failed to read SPS";
    return nullptr;
  }
  if (!reader.ReadNalu(&param_set_ptrs[2], &param_set_sizes[2])) {
    RTC_LOG(LS_ERROR) << "Failed to read PPS";
    return nullptr;
  }

  // Parse the SPS and PPS into a CMVideoFormatDescription.
  CMVideoFormatDescriptionRef description = nullptr;
  OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
      kCFAllocatorDefault, 3, param_set_ptrs, param_set_sizes, 4, nullptr,
      &description);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create video format description.";
    return nullptr;
  }
  return description;
}
#endif

AnnexBBufferReader::AnnexBBufferReader(const uint8_t* annexb_buffer,
                                       size_t length, bool isH264)
    : start_(annexb_buffer), length_(length) {
  RTC_DCHECK(annexb_buffer);

  offsets_ = H264::FindNaluIndices(annexb_buffer, length);
  offset_ = offsets_.begin();
}

AnnexBBufferReader::~AnnexBBufferReader() = default;

bool AnnexBBufferReader::ReadNalu(const uint8_t** out_nalu,
                                  size_t* out_length) {
  RTC_DCHECK(out_nalu);
  RTC_DCHECK(out_length);
  *out_nalu = nullptr;
  *out_length = 0;

  if (offset_ == offsets_.end()) {
    return false;
  }
  *out_nalu = start_ + offset_->payload_start_offset;
  *out_length = offset_->payload_size;
  ++offset_;
  return true;
}

size_t AnnexBBufferReader::BytesRemaining() const {
  if (offset_ == offsets_.end()) {
    return 0;
  }
  return length_ - offset_->start_offset;
}

size_t AnnexBBufferReader::BytesRemainingForAVC() const {
  if (offset_ == offsets_.end()) {
    return 0;
  }
  auto iterator = offset_;
  size_t size = 0;
  while (iterator != offsets_.end()) {
    size += kAvccHeaderByteSize + iterator->payload_size;
    iterator++;
  }
  return size;
}

void AnnexBBufferReader::SeekToStart() {
  offset_ = offsets_.begin();
}

bool AnnexBBufferReader::SeekToNextNaluOfType(NaluType type) {
  for (; offset_ != offsets_.end(); ++offset_) {
    if (offset_->payload_size < 1)
      continue;
    if (ParseNaluType(*(start_ + offset_->payload_start_offset)) == type)
      return true;
  }
  return false;
}

#ifndef DISABLE_H265
bool AnnexBBufferReader::SeekToNextNaluOfType(H265::NaluType type) {
  for (; offset_ != offsets_.end(); ++offset_) {
    if (offset_->payload_size < 1)
      continue;
    if (H265::ParseNaluType(*(start_ + offset_->payload_start_offset)) == type)
      return true;
  }
  return false;
}
#endif

AvccBufferWriter::AvccBufferWriter(uint8_t* const avcc_buffer, size_t length)
    : start_(avcc_buffer), offset_(0), length_(length) {
  RTC_DCHECK(avcc_buffer);
}

bool AvccBufferWriter::WriteNalu(const uint8_t* data, size_t data_size) {
  // Check if we can write this length of data.
  if (data_size + kAvccHeaderByteSize > BytesRemaining()) {
    RTC_LOG(LS_ERROR) << "AvccBufferWriter::WriteNalu failed.";
    return false;
  }
  // Write length header, which needs to be big endian.
  uint32_t big_endian_length = CFSwapInt32HostToBig(data_size);
  memcpy(start_ + offset_, &big_endian_length, sizeof(big_endian_length));
  offset_ += sizeof(big_endian_length);
  // Write data.
  memcpy(start_ + offset_, data, data_size);
  offset_ += data_size;
  return true;
}

size_t AvccBufferWriter::BytesRemaining() const {
  return length_ - offset_;
}

}  // namespace webrtc
