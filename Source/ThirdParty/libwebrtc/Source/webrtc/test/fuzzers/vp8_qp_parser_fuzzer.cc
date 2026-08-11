/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstddef>
#include <cstdint>
#include <span>

#include "modules/video_coding/utility/vp8_header_parser.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  int qp;
  std::span<const uint8_t> raw = fuzz_data.ReadRemaining();
  vp8::GetQp(raw.data(), raw.size(), &qp);
}
}  // namespace webrtc
