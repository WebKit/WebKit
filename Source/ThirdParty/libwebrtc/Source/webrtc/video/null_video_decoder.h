/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_NULL_VIDEO_DECODER_H_
#define VIDEO_NULL_VIDEO_DECODER_H_

#include <cstdint>

#include "api/video/encoded_image.h"
#include "api/video_codecs/video_decoder.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// The decoder used when there is no real decoder implementation available.
class RTC_EXPORT NullVideoDecoder : public VideoDecoder {
 public:
  // These are NO-OPs except for RTC_LOG lines.
  bool Configure(const Settings& settings) override;
  int32_t Decode(const EncodedImage& input_image,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;

  // This is exposed in getStats() as "decoderImplementation".
  const char* ImplementationName() const override;
};

}  // namespace webrtc

#endif  // VIDEO_NULL_VIDEO_DECODER_H_
