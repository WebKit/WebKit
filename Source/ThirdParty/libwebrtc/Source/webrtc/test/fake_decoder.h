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

#include <vector>

#include "modules/video_coding/include/video_codec_interface.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class FakeDecoder : public VideoDecoder {
 public:
  FakeDecoder();
  virtual ~FakeDecoder() {}

  int32_t InitDecode(const VideoCodec* config,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;

  const char* ImplementationName() const override;

  static const char* kImplementationName;

 private:
  VideoCodec config_;
  DecodedImageCallback* callback_;
};

class FakeH264Decoder : public FakeDecoder {
 public:
  virtual ~FakeH264Decoder() {}

  int32_t Decode(const EncodedImage& input,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;
};

class FakeNullDecoder : public FakeDecoder {
 public:
  virtual ~FakeNullDecoder() {}

  int32_t Decode(const EncodedImage& input,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    return 0;
  }
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_DECODER_H_
