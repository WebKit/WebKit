/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "common_video/test/utilities.h"

#include <utility>

namespace webrtc {

HdrMetadata CreateTestHdrMetadata() {
  // Random but reasonable (in the sense of a valid range) HDR metadata.
  HdrMetadata hdr_metadata;
  hdr_metadata.mastering_metadata.luminance_max = 2000.0;
  hdr_metadata.mastering_metadata.luminance_min = 2.0001;
  hdr_metadata.mastering_metadata.primary_r.x = 0.3003;
  hdr_metadata.mastering_metadata.primary_r.y = 0.4004;
  hdr_metadata.mastering_metadata.primary_g.x = 0.3201;
  hdr_metadata.mastering_metadata.primary_g.y = 0.4604;
  hdr_metadata.mastering_metadata.primary_b.x = 0.3409;
  hdr_metadata.mastering_metadata.primary_b.y = 0.4907;
  hdr_metadata.mastering_metadata.white_point.x = 0.4103;
  hdr_metadata.mastering_metadata.white_point.y = 0.4806;
  hdr_metadata.max_content_light_level = 2345;
  hdr_metadata.max_frame_average_light_level = 1789;
  return hdr_metadata;
}

ColorSpace CreateTestColorSpace(bool with_hdr_metadata) {
  HdrMetadata hdr_metadata = CreateTestHdrMetadata();
  return ColorSpace(
      ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kGAMMA22,
      ColorSpace::MatrixID::kSMPTE2085, ColorSpace::RangeID::kFull,
      ColorSpace::ChromaSiting::kCollocated,
      ColorSpace::ChromaSiting::kCollocated,
      with_hdr_metadata ? &hdr_metadata : nullptr);
}

RtpPacketInfos CreatePacketInfos(size_t count) {
  return RtpPacketInfos(RtpPacketInfos::vector_type(count));
}

}  // namespace webrtc
