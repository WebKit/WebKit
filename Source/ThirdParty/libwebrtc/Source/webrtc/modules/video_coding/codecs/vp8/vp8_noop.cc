/*
 *  Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#if !defined(RTC_DISABLE_VP8)
#error
#endif  // !defined(RTC_DISABLE_VP8)

#include "rtc_base/checks.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {

bool VP8Encoder::IsSupported() {
  return false;
}

std::unique_ptr<VP8Encoder> VP8Encoder::Create() {
  RTC_NOTREACHED();
  return nullptr;
}

bool VP8Decoder::IsSupported() {
  return false;
}

std::unique_ptr<VP8Decoder> VP8Decoder::Create() {
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
