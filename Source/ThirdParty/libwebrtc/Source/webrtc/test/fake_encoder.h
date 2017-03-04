/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FAKE_ENCODER_H_
#define WEBRTC_TEST_FAKE_ENCODER_H_

#include <vector>
#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video_encoder.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder() = default;

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetRateAllocation(const BitrateAllocation& rate_allocation,
                            uint32_t framerate) override;
  const char* ImplementationName() const override;

  static const char* kImplementationName;

 protected:
  Clock* const clock_;
  VideoCodec config_ GUARDED_BY(crit_sect_);
  EncodedImageCallback* callback_ GUARDED_BY(crit_sect_);
  BitrateAllocation target_bitrate_ GUARDED_BY(crit_sect_);
  int max_target_bitrate_kbps_ GUARDED_BY(crit_sect_);
  int64_t last_encode_time_ms_ GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;

  uint8_t encoded_buffer_[100000];
};

class FakeH264Encoder : public FakeEncoder, public EncodedImageCallback {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() = default;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  Result OnEncodedImage(const EncodedImage& encodedImage,
                        const CodecSpecificInfo* codecSpecificInfo,
                        const RTPFragmentationHeader* fragments) override;

 private:
  EncodedImageCallback* callback_ GUARDED_BY(local_crit_sect_);
  int idr_counter_ GUARDED_BY(local_crit_sect_);
  rtc::CriticalSection local_crit_sect_;
};

class DelayedEncoder : public test::FakeEncoder {
 public:
  DelayedEncoder(Clock* clock, int delay_ms);
  virtual ~DelayedEncoder() = default;

  void SetDelay(int delay_ms);
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

 private:
  int delay_ms_ ACCESS_ON(sequence_checker_);
  rtc::SequencedTaskChecker sequence_checker_;
};

// This class implements a multi-threaded fake encoder by posting
// FakeH264Encoder::Encode(.) tasks to |queue1_| and |queue2_|, in an
// alternating fashion. The class itself does not need to be thread safe,
// as it is called from the task queue in ViEEncoder.
class MultithreadedFakeH264Encoder : public test::FakeH264Encoder {
 public:
  explicit MultithreadedFakeH264Encoder(Clock* clock);
  virtual ~MultithreadedFakeH264Encoder() = default;

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;

  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

  int32_t EncodeCallback(const VideoFrame& input_image,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<FrameType>* frame_types);

  int32_t Release() override;

 protected:
  class EncodeTask;

  int current_queue_ ACCESS_ON(sequence_checker_);
  std::unique_ptr<rtc::TaskQueue> queue1_ ACCESS_ON(sequence_checker_);
  std::unique_ptr<rtc::TaskQueue> queue2_ ACCESS_ON(sequence_checker_);
  rtc::SequencedTaskChecker sequence_checker_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_FAKE_ENCODER_H_
