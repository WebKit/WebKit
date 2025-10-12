/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "rtc_base/string_to_number.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  std::string number_to_parse(reinterpret_cast<const char*>(data), size);
  webrtc::StringToNumber<int8_t>(number_to_parse);
  webrtc::StringToNumber<int16_t>(number_to_parse);
  webrtc::StringToNumber<int32_t>(number_to_parse);
  webrtc::StringToNumber<int64_t>(number_to_parse);
  webrtc::StringToNumber<uint8_t>(number_to_parse);
  webrtc::StringToNumber<uint16_t>(number_to_parse);
  webrtc::StringToNumber<uint32_t>(number_to_parse);
  webrtc::StringToNumber<uint64_t>(number_to_parse);
  webrtc::StringToNumber<float>(number_to_parse);
  webrtc::StringToNumber<double>(number_to_parse);
  webrtc::StringToNumber<long double>(number_to_parse);
}

}  // namespace webrtc
