/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_ENCODER_H_
#define API_TEST_MOCK_VIDEO_ENCODER_H_

#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockEncodedImageCallback : public EncodedImageCallback {
 public:
  MockEncodedImageCallback();
  ~MockEncodedImageCallback();
  MOCK_METHOD3(OnEncodedImage,
               Result(const EncodedImage& encodedImage,
                      const CodecSpecificInfo* codecSpecificInfo,
                      const RTPFragmentationHeader* fragmentation));
};

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder();
  ~MockVideoEncoder();
  MOCK_METHOD1(SetFecControllerOverride,
               void(FecControllerOverride* fec_controller_override));
  MOCK_CONST_METHOD2(Version, int32_t(int8_t* version, int32_t length));
  MOCK_METHOD3(InitEncode,
               int32_t(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize));
  MOCK_METHOD2(InitEncode,
               int32_t(const VideoCodec* codecSettings,
                       const VideoEncoder::Settings& settings));

  MOCK_METHOD2(Encode,
               int32_t(const VideoFrame& inputImage,
                       const std::vector<VideoFrameType>* frame_types));
  MOCK_METHOD1(RegisterEncodeCompleteCallback,
               int32_t(EncodedImageCallback* callback));
  MOCK_METHOD0(Release, int32_t());
  MOCK_METHOD0(Reset, int32_t());
  MOCK_METHOD1(SetRates, void(const RateControlParameters& parameters));
  MOCK_CONST_METHOD0(GetEncoderInfo, EncoderInfo(void));
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_VIDEO_ENCODER_H_
