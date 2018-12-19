/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/include/vp9.h"

#include "absl/memory/memory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/vp9/vp9_impl.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::vector<SdpVideoFormat> SupportedVP9Codecs() {
#ifdef RTC_ENABLE_VP9
  // TODO(emircan): Add Profile 2 support after fixing browser_tests.
  std::vector<SdpVideoFormat> supported_formats{SdpVideoFormat(
      cricket::kVp9CodecName,
      {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}})};
  return supported_formats;
#else
  return std::vector<SdpVideoFormat>();
#endif
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create() {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9EncoderImpl>(cricket::VideoCodec());
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create(
    const cricket::VideoCodec& codec) {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9EncoderImpl>(codec);
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

std::unique_ptr<VP9Decoder> VP9Decoder::Create() {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9DecoderImpl>();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

}  // namespace webrtc
