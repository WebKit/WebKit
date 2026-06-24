/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/null_video_decoder.h"

#include <cstdint>

#include "api/video/encoded_image.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"

namespace webrtc {

bool NullVideoDecoder::Configure(const Settings& settings) {
  RTC_LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
  return true;
}

int32_t NullVideoDecoder::Decode(const EncodedImage& input_image,
                                 int64_t render_time_ms) {
  RTC_LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t NullVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  RTC_LOG(LS_ERROR)
      << "Can't register decode complete callback on NullVideoDecoder.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t NullVideoDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* NullVideoDecoder::ImplementationName() const {
  return "NullVideoDecoder";
}

}  // namespace webrtc
