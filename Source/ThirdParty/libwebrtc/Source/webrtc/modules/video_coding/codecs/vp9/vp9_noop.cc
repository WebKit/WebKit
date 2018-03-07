/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#if !defined(RTC_DISABLE_VP9)
#error
#endif  // !defined(RTC_DISABLE_VP9)

#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/checks.h"

namespace webrtc {

bool VP9Encoder::IsSupported() {
  return false;
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create() {
  RTC_NOTREACHED();
  return nullptr;
}

bool VP9Decoder::IsSupported() {
  return false;
}

std::unique_ptr<VP9Decoder> VP9Decoder::Create() {
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
