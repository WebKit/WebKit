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
#include "api/task_queue/task_queue_factory.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder() = default;

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  int32_t InitEncode(const VideoCodec* config,
                     const Settings& settings) override;
  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;
  void SetRates(const RateControlParameters& parameters) override;
  int GetConfiguredInputFramerate() const;
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
                      SimulcastStream simulcast_streams[kMaxSimulcastStreams],
                      int framerate);

  // Called before the frame is passed to callback_->OnEncodedImage, to let
  // subclasses fill out codec_specific, possibly modify encodedImage.
  // Returns an RTPFragmentationHeader, if needed by the codec.
  virtual std::unique_ptr<RTPFragmentationHeader> EncodeHook(
      EncodedImage* encoded_image,
      CodecSpecificInfo* codec_specific);

  FrameInfo last_frame_info_ RTC_GUARDED_BY(crit_sect_);
  Clock* const clock_;

  VideoCodec config_ RTC_GUARDED_BY(crit_sect_);
  EncodedImageCallback* callback_ RTC_GUARDED_BY(crit_sect_);
  RateControlParameters current_rate_settings_ RTC_GUARDED_BY(crit_sect_);
  int max_target_bitrate_kbps_ RTC_GUARDED_BY(crit_sect_);
  bool pending_keyframe_ RTC_GUARDED_BY(crit_sect_);
  uint32_t counter_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;
  bool used_layers_[kMaxSimulcastStreams];

  // Current byte debt to be payed over a number of frames.
  // The debt is acquired by keyframes overshooting the bitrate target.
  size_t debt_bytes_;
};

class FakeH264Encoder : public FakeEncoder {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() = default;

 private:
  std::unique_ptr<RTPFragmentationHeader> EncodeHook(
      EncodedImage* encoded_image,
      CodecSpecificInfo* codec_specific) override;

  int idr_counter_ RTC_GUARDED_BY(local_crit_sect_);
  rtc::CriticalSection local_crit_sect_;
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
