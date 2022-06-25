/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_ENCODER_H_
#define TEST_FAKE_ENCODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/fec_controller_override.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder() = default;

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps) RTC_LOCKS_EXCLUDED(mutex_);
  void SetQp(int qp) RTC_LOCKS_EXCLUDED(mutex_);

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  int32_t InitEncode(const VideoCodec* config, const Settings& settings)
      RTC_LOCKS_EXCLUDED(mutex_) override;
  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types)
      RTC_LOCKS_EXCLUDED(mutex_) override;
  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
      RTC_LOCKS_EXCLUDED(mutex_) override;
  int32_t Release() override;
  void SetRates(const RateControlParameters& parameters)
      RTC_LOCKS_EXCLUDED(mutex_) override;
  int GetConfiguredInputFramerate() const RTC_LOCKS_EXCLUDED(mutex_);
  EncoderInfo GetEncoderInfo() const override;

  static const char* kImplementationName;

 protected:
  struct FrameInfo {
    bool keyframe;
    struct SpatialLayer {
      SpatialLayer() = default;
      SpatialLayer(int size, int temporal_id)
          : size(size), temporal_id(temporal_id) {}
      // Size of a current frame in the layer.
      int size = 0;
      // Temporal index of a current frame in the layer.
      int temporal_id = 0;
    };
    std::vector<SpatialLayer> layers;
  };

  FrameInfo NextFrame(const std::vector<VideoFrameType>* frame_types,
                      bool keyframe,
                      uint8_t num_simulcast_streams,
                      const VideoBitrateAllocation& target_bitrate,
                      SpatialLayer simulcast_streams[kMaxSimulcastStreams],
                      int framerate) RTC_LOCKS_EXCLUDED(mutex_);

  // Called before the frame is passed to callback_->OnEncodedImage, to let
  // subclasses fill out CodecSpecificInfo, possibly modify |encoded_image| or
  // |buffer|.
  virtual CodecSpecificInfo EncodeHook(
      EncodedImage& encoded_image,
      rtc::scoped_refptr<EncodedImageBuffer> buffer);

  void SetRatesLocked(const RateControlParameters& parameters)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  FrameInfo last_frame_info_ RTC_GUARDED_BY(mutex_);
  Clock* const clock_;

  VideoCodec config_ RTC_GUARDED_BY(mutex_);
  EncodedImageCallback* callback_ RTC_GUARDED_BY(mutex_);
  RateControlParameters current_rate_settings_ RTC_GUARDED_BY(mutex_);
  int max_target_bitrate_kbps_ RTC_GUARDED_BY(mutex_);
  bool pending_keyframe_ RTC_GUARDED_BY(mutex_);
  uint32_t counter_ RTC_GUARDED_BY(mutex_);
  mutable Mutex mutex_;
  bool used_layers_[kMaxSimulcastStreams];
  absl::optional<int> qp_ RTC_GUARDED_BY(mutex_);

  // Current byte debt to be payed over a number of frames.
  // The debt is acquired by keyframes overshooting the bitrate target.
  size_t debt_bytes_;
};

class FakeH264Encoder : public FakeEncoder {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() = default;

 private:
  CodecSpecificInfo EncodeHook(
      EncodedImage& encoded_image,
      rtc::scoped_refptr<EncodedImageBuffer> buffer) override;

  int idr_counter_ RTC_GUARDED_BY(local_mutex_);
  Mutex local_mutex_;
};

class DelayedEncoder : public test::FakeEncoder {
 public:
  DelayedEncoder(Clock* clock, int delay_ms);
  virtual ~DelayedEncoder() = default;

  void SetDelay(int delay_ms);
  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;

 private:
  int delay_ms_ RTC_GUARDED_BY(sequence_checker_);
  SequenceChecker sequence_checker_;
};

// This class implements a multi-threaded fake encoder by posting
// FakeH264Encoder::Encode(.) tasks to |queue1_| and |queue2_|, in an
// alternating fashion. The class itself does not need to be thread safe,
// as it is called from the task queue in VideoStreamEncoder.
class MultithreadedFakeH264Encoder : public test::FakeH264Encoder {
 public:
  MultithreadedFakeH264Encoder(Clock* clock,
                               TaskQueueFactory* task_queue_factory);
  virtual ~MultithreadedFakeH264Encoder() = default;

  int32_t InitEncode(const VideoCodec* config,
                     const Settings& settings) override;

  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;

  int32_t EncodeCallback(const VideoFrame& input_image,
                         const std::vector<VideoFrameType>* frame_types);

  int32_t Release() override;

 protected:
  class EncodeTask;

  TaskQueueFactory* const task_queue_factory_;
  int current_queue_ RTC_GUARDED_BY(sequence_checker_);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> queue1_
      RTC_GUARDED_BY(sequence_checker_);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> queue2_
      RTC_GUARDED_BY(sequence_checker_);
  SequenceChecker sequence_checker_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_ENCODER_H_
