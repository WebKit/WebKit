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
  rtc::StringToNumber<int8_t>(number_to_parse);
  rtc::StringToNumber<int16_t>(number_to_parse);
  rtc::StringToNumber<int32_t>(number_to_parse);
  rtc::StringToNumber<int64_t>(number_to_parse);
  rtc::StringToNumber<uint8_t>(number_to_parse);
  rtc::StringToNumber<uint16_t>(number_to_parse);
  rtc::StringToNumber<uint32_t>(number_to_parse);
  rtc::StringToNumber<uint64_t>(number_to_parse);
  rtc::StringToNumber<float>(number_to_parse);
  rtc::StringToNumber<double>(number_to_parse);
  rtc::StringToNumber<long double>(number_to_parse);
}

}  // namespace webrtc
