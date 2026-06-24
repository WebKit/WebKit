/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdint.h>

#include <cstddef>

#include "common_video/h264/h264_bitstream_parser.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  H264BitstreamParser h264_bitstream_parser;
  h264_bitstream_parser.ParseBitstream(fuzz_data.ReadRemaining());
  h264_bitstream_parser.GetLastSliceQp();
}
}  // namespace webrtc
