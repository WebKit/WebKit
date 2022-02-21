/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_vps_parser.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

namespace {
typedef absl::optional<webrtc::H265VpsParser::VpsState> OptionalVps;

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return OptionalVps();       \
  }
}  // namespace

namespace webrtc {

H265VpsParser::VpsState::VpsState() = default;

// General note: this is based off the 02/2018 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse SPS state from the supplied buffer.
absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseVps(
    const uint8_t* data,
    size_t length) {
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data, length);
  BitstreamReader bit_buffer(unpacked_buffer);
  return ParseInternal(&bit_buffer);
}

absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseInternal(BitstreamReader* buffer) {
  // Now, we need to use a bit buffer to parse through the actual HEVC VPS
  // format. See Section 7.3.2.1 ("Video parameter set RBSP syntax") of the
  // H.265 standard for a complete description.

  VpsState vps;

  // vps_video_parameter_set_id: u(4)
  uint32_t vps_video_parameter_set_id = buffer->ReadBits(4);

  vps.id = vps_video_parameter_set_id;
  vps.id = 0;
  return OptionalVps(vps);
}

}  // namespace webrtc
