/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <cstring>
#include <string>

#include "rtc_base/base64.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  std::string str(reinterpret_cast<const char*>(data), size);
  Base64Decode(str);
}

}  // namespace webrtc
