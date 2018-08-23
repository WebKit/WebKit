/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_
#define VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_

#include "api/video/video_stream_encoder_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoStreamEncoder : public VideoStreamEncoderInterface {
 public:
  MOCK_METHOD2(SetSource,
               void(rtc::VideoSourceInterface<VideoFrame>*,
                    const DegradationPreference&));
  MOCK_METHOD2(SetSink, void(EncoderSink*, bool));
  MOCK_METHOD1(SetStartBitrate, void(int));
  MOCK_METHOD0(SendKeyFrame, void());
  MOCK_METHOD3(OnBitrateUpdated, void(uint32_t, uint8_t, int64_t));
  MOCK_METHOD1(OnFrame, void(const VideoFrame&));
  MOCK_METHOD1(SetBitrateAllocationObserver,
               void(VideoBitrateAllocationObserver*));
  MOCK_METHOD0(Stop, void());

  MOCK_METHOD2(MockedConfigureEncoder, void(const VideoEncoderConfig&, size_t));
  // gtest generates implicit copy which is not allowed on VideoEncoderConfig,
  // so we can't mock ConfigureEncoder directly.
  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length) {
    MockedConfigureEncoder(config, max_data_payload_length);
  }
};

}  // namespace webrtc

#endif  // VIDEO_TEST_MOCK_VIDEO_STREAM_ENCODER_H_
