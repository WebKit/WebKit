/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_vps_parser.h"

#include "common_video/h265/h265_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

namespace webrtc {

H265VpsParser::VpsState::VpsState() = default;

// General note: this is based off the 08/2021 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse VPS state from the supplied buffer.
absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseVps(
    const uint8_t* data,
    size_t length) {
  RTC_DCHECK(data);
  return ParseInternal(H265::ParseRbsp(data, length));
}

absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);

  // Now, we need to use a bit buffer to parse through the actual H265 VPS
  // format. See Section 7.3.2.1 ("Video parameter set RBSP syntax") of the
  // H.265 standard for a complete description.
  VpsState vps;

  // vps_video_parameter_set_id: u(4)
  vps.id = reader.ReadBits(4);

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return vps;
}

}  // namespace webrtc
