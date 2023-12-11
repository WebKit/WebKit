/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdint.h>

#include "common_video/h265/h265_bitstream_parser.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  H265BitstreamParser h265_bitstream_parser;
  h265_bitstream_parser.ParseBitstream(
      rtc::ArrayView<const uint8_t>(data, size));
  h265_bitstream_parser.GetLastSliceQp();
}
}  // namespace webrtc
