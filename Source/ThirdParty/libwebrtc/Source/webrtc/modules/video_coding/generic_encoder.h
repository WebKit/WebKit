/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_

#include <stdio.h>
#include <vector>

#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/race_checker.h"

namespace webrtc {
class CriticalSectionWrapper;

namespace media_optimization {
class MediaOptimization;
}  // namespace media_optimization

struct EncoderParameters {
  BitrateAllocation target_bitrate;
  uint8_t loss_rate;
  int64_t rtt;
  uint32_t input_frame_rate;
};

class VCMEncodedFrameCallback : public EncodedImageCallback {
 public:
  VCMEncodedFrameCallback(EncodedImageCallback* post_encode_callback,
                          media_optimization::MediaOptimization* media_opt);
  virtual ~VCMEncodedFrameCallback();

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;
  void SetInternalSource(bool internal_source) {
    internal_source_ = internal_source;
  }

 private:
  bool internal_source_;
  EncodedImageCallback* const post_encode_callback_;
  media_optimization::MediaOptimization* const media_opt_;
};

class VCMGenericEncoder {
  friend class VCMCodecDataBase;

 public:
  VCMGenericEncoder(VideoEncoder* encoder,
                    VCMEncodedFrameCallback* encoded_frame_callback,
                    bool internal_source);
  ~VCMGenericEncoder();
  int32_t Release();
  int32_t InitEncode(const VideoCodec* settings,
                     int32_t number_of_cores,
                     size_t max_payload_size);
  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific,
                 const std::vector<FrameType>& frame_types);

  void SetEncoderParameters(const EncoderParameters& params);
  EncoderParameters GetEncoderParameters() const;

  int32_t SetPeriodicKeyFrames(bool enable);
  int32_t RequestFrame(const std::vector<FrameType>& frame_types);
  bool InternalSource() const;
  void OnDroppedFrame();
  bool SupportsNativeHandle() const;

 private:
  rtc::RaceChecker race_checker_;

  VideoEncoder* const encoder_ GUARDED_BY(race_checker_);
  VCMEncodedFrameCallback* const vcm_encoded_frame_callback_;
  const bool internal_source_;
  rtc::CriticalSection params_lock_;
  EncoderParameters encoder_params_ GUARDED_BY(params_lock_);
  bool is_screenshare_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
