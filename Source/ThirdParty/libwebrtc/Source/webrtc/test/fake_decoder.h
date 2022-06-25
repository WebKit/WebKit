/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_DECODER_H_
#define TEST_FAKE_DECODER_H_

#include <stdint.h>

#include "api/task_queue/task_queue_factory.h"
#include "api/video/encoded_image.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/task_queue.h"

namespace webrtc {
namespace test {

class FakeDecoder : public VideoDecoder {
 public:
  enum { kDefaultWidth = 320, kDefaultHeight = 180 };

  FakeDecoder();
  explicit FakeDecoder(TaskQueueFactory* task_queue_factory);
  virtual ~FakeDecoder() {}

  int32_t InitDecode(const VideoCodec* config,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input,
                 bool missing_frames,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;

  DecoderInfo GetDecoderInfo() const override;
  const char* ImplementationName() const override;

  static const char* kImplementationName;

  void SetDelayedDecoding(int decode_delay_ms);

 private:
  DecodedImageCallback* callback_;
  int width_;
  int height_;
  std::unique_ptr<rtc::TaskQueue> task_queue_;
  TaskQueueFactory* task_queue_factory_;
  int decode_delay_ms_;
};

class FakeH264Decoder : public FakeDecoder {
 public:
  virtual ~FakeH264Decoder() {}

  int32_t Decode(const EncodedImage& input,
                 bool missing_frames,
                 int64_t render_time_ms) override;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_DECODER_H_
