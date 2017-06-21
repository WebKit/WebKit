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

#include <string.h>

#include "webrtc/base/array_view.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/sanitizer.h"

namespace webrtc {

namespace {

CodecInst MakeCodecInst(int payload_type,
                        const char* name,
                        int sample_rate,
                        size_t num_channels) {
  // Create a CodecInst with some fields set. The remaining fields are zeroed,
  // but we tell MSan to consider them uninitialized.
  CodecInst ci = {0};
  rtc::MsanMarkUninitialized(rtc::MakeArrayView(&ci, 1));
  ci.pltype = payload_type;
  strncpy(ci.plname, name, sizeof(ci.plname));
  ci.plname[sizeof(ci.plname) - 1] = '\0';
  ci.plfreq = sample_rate;
  ci.channels = num_channels;
  return ci;
}

}  // namespace

SdpAudioFormat CodecInstToSdp(const CodecInst& ci) {
  if (STR_CASE_CMP(ci.plname, "g722") == 0) {
    RTC_CHECK_EQ(16000, ci.plfreq);
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return {"g722", 8000, ci.channels};
  } else if (STR_CASE_CMP(ci.plname, "opus") == 0) {
    RTC_CHECK_EQ(48000, ci.plfreq);
    RTC_CHECK(ci.channels == 1 || ci.channels == 2);
    return ci.channels == 1
               ? SdpAudioFormat("opus", 48000, 2)
               : SdpAudioFormat("opus", 48000, 2, {{"stereo", "1"}});
  } else {
    return {ci.plname, ci.plfreq, ci.channels};
  }
}

CodecInst SdpToCodecInst(int payload_type, const SdpAudioFormat& audio_format) {
  if (STR_CASE_CMP(audio_format.name.c_str(), "g722") == 0) {
    RTC_CHECK_EQ(8000, audio_format.clockrate_hz);
    RTC_CHECK(audio_format.num_channels == 1 || audio_format.num_channels == 2);
    return MakeCodecInst(payload_type, "g722", 16000,
                         audio_format.num_channels);
  } else if (STR_CASE_CMP(audio_format.name.c_str(), "opus") == 0) {
    RTC_CHECK_EQ(48000, audio_format.clockrate_hz);
    RTC_CHECK_EQ(2, audio_format.num_channels);
    const int num_channels = [&] {
      auto stereo = audio_format.parameters.find("stereo");
      if (stereo != audio_format.parameters.end()) {
        if (stereo->second == "0") {
          return 1;
        } else if (stereo->second == "1") {
          return 2;
        } else {
          RTC_CHECK(false);  // Bad stereo parameter.
        }
      }
      return 1;  // Default to mono.
    }();
    return MakeCodecInst(payload_type, "opus", 48000, num_channels);
  } else {
    return MakeCodecInst(payload_type, audio_format.name.c_str(),
                         audio_format.clockrate_hz, audio_format.num_channels);
  }
}

}  // namespace webrtc
