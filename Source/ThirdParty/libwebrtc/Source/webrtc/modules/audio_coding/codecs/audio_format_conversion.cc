/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/audio_format_conversion.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"

namespace webrtc {

SdpAudioFormat CodecInstToSdp(const CodecInst& ci) {
  if (STR_CASE_CMP(ci.plname, "g722") == 0 && ci.plfreq == 16000) {
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return {"g722", 8000, static_cast<int>(ci.channels)};
  } else if (STR_CASE_CMP(ci.plname, "opus") == 0 && ci.plfreq == 48000) {
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return {"opus", 48000, 2, {{"stereo", ci.channels == 1 ? "0" : "1"}}};
  } else {
    return {ci.plname, ci.plfreq, rtc::checked_cast<int>(ci.channels)};
  }
}

}  // namespace webrtc
