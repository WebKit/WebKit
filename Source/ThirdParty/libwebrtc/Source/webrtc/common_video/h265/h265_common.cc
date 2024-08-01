/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_common.h"

#include "common_video/h264/h264_common.h"

namespace webrtc {
namespace H265 {

constexpr uint8_t kNaluTypeMask = 0x7E;

std::vector<NaluIndex> FindNaluIndices(rtc::ArrayView<const uint8_t> buffer) {
  std::vector<H264::NaluIndex> indices = H264::FindNaluIndices(buffer);
  std::vector<NaluIndex> results;
  for (auto& index : indices) {
    results.push_back(
        {index.start_offset, index.payload_start_offset, index.payload_size});
  }
  return results;
}

NaluType ParseNaluType(uint8_t data) {
  return static_cast<NaluType>((data & kNaluTypeMask) >> 1);
}

std::vector<uint8_t> ParseRbsp(rtc::ArrayView<const uint8_t> data) {
  return H264::ParseRbsp(data);
}

void WriteRbsp(rtc::ArrayView<const uint8_t> bytes, rtc::Buffer* destination) {
  H264::WriteRbsp(bytes, destination);
}

uint32_t Log2Ceiling(uint32_t value) {
  // When n == 0, we want the function to return -1.
  // When n == 0, (n - 1) will underflow to 0xFFFFFFFF, which is
  // why the statement below starts with (n ? 32 : -1).
  return (value ? 32 : -1) - WebRtcVideo_CountLeadingZeros32(value - 1);
}

}  // namespace H265
}  // namespace webrtc
