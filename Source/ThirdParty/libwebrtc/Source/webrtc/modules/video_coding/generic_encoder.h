/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
#define MODULES_VIDEO_CODING_GENERIC_ENCODER_H_

#include <stdio.h>
#include <list>
#include <vector>

#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"

#include "rtc_base/criticalsection.h"
#include "rtc_base/race_checker.h"

namespace webrtc {

namespace media_optimization {
class MediaOptimization;
}  // namespace media_optimization

struct EncoderParameters {
  VideoBitrateAllocation target_bitrate;
  uint8_t loss_rate;
  int64_t rtt;
  uint32_t input_frame_rate;
};

class VCMEncodedFrameCallback : public EncodedImageCallback {
 public:
  VCMEncodedFrameCallback(EncodedImageCallback* post_encode_callback,
                          media_optimization::MediaOptimization* media_opt);
  ~VCMEncodedFrameCallback() override;

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void SetInternalSource(bool internal_source) {
    internal_source_ = internal_source;
  }

  // Timing frames configuration methods. These 4 should be called before
  // |OnEncodedImage| at least once.
  void OnTargetBitrateChanged(size_t bitrate_bytes_per_sec,
                              size_t simulcast_svc_idx);

  void OnFrameRateChanged(size_t framerate);

  void OnEncodeStarted(uint32_t rtp_timestamps,
                       int64_t capture_time_ms,
                       size_t simulcast_svc_idx);

  void SetTimingFramesThresholds(
      const VideoCodec::TimingFrameTriggerThresholds& thresholds) {
    rtc::CritScope crit(&timing_params_lock_);
    timing_frames_thresholds_ = thresholds;
  }

  // Clears all data stored by OnEncodeStarted().
  void Reset() {
    rtc::CritScope crit(&timing_params_lock_);
    timing_frames_info_.clear();
    last_timing_frame_time_ms_ = -1;
    reordered_frames_logged_messages_ = 0;
    stalled_encoder_logged_messages_ = 0;
  }

 private:
  // For non-internal-source encoders, returns encode started time and fixes
  // capture timestamp for the frame, if corrupted by the encoder.
  absl::optional<int64_t> ExtractEncodeStartTime(size_t simulcast_svc_idx,
                                                 EncodedImage* encoded_image)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(timing_params_lock_);

  void FillTimingInfo(size_t simulcast_svc_idx, EncodedImage* encoded_image);

  rtc::CriticalSection timing_params_lock_;
  bool internal_source_;
  EncodedImageCallback* const post_encode_callback_;
  media_optimization::MediaOptimization* const media_opt_;

  struct EncodeStartTimeRecord {
    EncodeStartTimeRecord(uint32_t timestamp,
                          int64_t capture_time,
                          int64_t encode_start_time)
        : rtp_timestamp(timestamp),
          capture_time_ms(capture_time),
          encode_start_time_ms(encode_start_time) {}
    uint32_t rtp_timestamp;
    int64_t capture_time_ms;
    int64_t encode_start_time_ms;
  };
  struct TimingFramesLayerInfo {
    TimingFramesLayerInfo();
    ~TimingFramesLayerInfo();
    size_t target_bitrate_bytes_per_sec = 0;
    std::list<EncodeStartTimeRecord> encode_start_list;
  };
  // Separate instance for each simulcast stream or spatial layer.
  std::vector<TimingFramesLayerInfo> timing_frames_info_
      RTC_GUARDED_BY(timing_params_lock_);
  size_t framerate_ RTC_GUARDED_BY(timing_params_lock_);
  int64_t last_timing_frame_time_ms_ RTC_GUARDED_BY(timing_params_lock_);
  VideoCodec::TimingFrameTriggerThresholds timing_frames_thresholds_
      RTC_GUARDED_BY(timing_params_lock_);
  size_t incorrect_capture_time_logged_messages_
      RTC_GUARDED_BY(timing_params_lock_);
  size_t reordered_frames_logged_messages_ RTC_GUARDED_BY(timing_params_lock_);
  size_t stalled_encoder_logged_messages_ RTC_GUARDED_BY(timing_params_lock_);

  // Experiment groups parsed from field trials for realtime video ([0]) and
  // screenshare ([1]). 0 means no group specified. Positive values are
  // experiment group numbers incremented by 1.
  uint8_t experiment_groups_[2];
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

  int32_t RequestFrame(const std::vector<FrameType>& frame_types);
  bool InternalSource() const;
  bool SupportsNativeHandle() const;

 private:
  rtc::RaceChecker race_checker_;

  VideoEncoder* const encoder_ RTC_GUARDED_BY(race_checker_);
  VCMEncodedFrameCallback* const vcm_encoded_frame_callback_;
  const bool internal_source_;
  rtc::CriticalSection params_lock_;
  EncoderParameters encoder_params_ RTC_GUARDED_BY(params_lock_);
  size_t streams_or_svc_num_ RTC_GUARDED_BY(race_checker_);
  VideoCodecType codec_type_ RTC_GUARDED_BY(race_checker_);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
