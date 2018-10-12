/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_VP8_ENCODER_H_
#define TEST_FAKE_VP8_ENCODER_H_

#include <memory>
#include <vector>

#include "modules/video_coding/codecs/vp8/include/vp8_temporal_layers.h"
#include "test/fake_encoder.h"

#include "rtc_base/criticalsection.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {
namespace test {

class FakeVP8Encoder : public FakeEncoder, public EncodedImageCallback {
 public:
  explicit FakeVP8Encoder(Clock* clock);
  virtual ~FakeVP8Encoder() = default;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;

  int32_t Release() override;

  const char* ImplementationName() const override { return "FakeVp8Encoder"; }

  Result OnEncodedImage(const EncodedImage& encodedImage,
                        const CodecSpecificInfo* codecSpecificInfo,
                        const RTPFragmentationHeader* fragments) override;

 private:
  void SetupTemporalLayers(const VideoCodec& codec);
  void PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             size_t size_bytes,
                             FrameType frame_type,
                             int stream_idx,
                             uint32_t timestamp);

  rtc::SequencedTaskChecker sequence_checker_;
  EncodedImageCallback* callback_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<std::unique_ptr<TemporalLayers>> temporal_layers_
      RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_VP8_ENCODER_H_
