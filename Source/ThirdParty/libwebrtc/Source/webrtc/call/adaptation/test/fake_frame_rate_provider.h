/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_
#define CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_

#include <string>
#include <vector>

#include "api/video/video_stream_encoder_observer.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoStreamEncoderObserver : public VideoStreamEncoderObserver {
 public:
  MOCK_METHOD(void, OnEncodedFrameTimeMeasured, (int, int), (override));
  MOCK_METHOD(void, OnIncomingFrame, (int, int), (override));
  MOCK_METHOD(void,
              OnSendEncodedImage,
              (const EncodedImage&, const CodecSpecificInfo*),
              (override));
  MOCK_METHOD(void,
              OnEncoderImplementationChanged,
              (const std::string&),
              (override));
  MOCK_METHOD(void, OnFrameDropped, (DropReason), (override));
  MOCK_METHOD(void,
              OnEncoderReconfigured,
              (const VideoEncoderConfig&, const std::vector<VideoStream>&),
              (override));
  MOCK_METHOD(void,
              OnAdaptationChanged,
              (VideoAdaptationReason,
               const VideoAdaptationCounters&,
               const VideoAdaptationCounters&),
              (override));
  MOCK_METHOD(void, ClearAdaptationStats, (), (override));
  MOCK_METHOD(void,
              UpdateAdaptationSettings,
              (AdaptationSettings, AdaptationSettings),
              (override));
  MOCK_METHOD(void, OnMinPixelLimitReached, (), (override));
  MOCK_METHOD(void, OnInitialQualityResolutionAdaptDown, (), (override));
  MOCK_METHOD(void, OnSuspendChange, (bool), (override));
  MOCK_METHOD(void,
              OnBitrateAllocationUpdated,
              (const VideoCodec&, const VideoBitrateAllocation&),
              (override));
  MOCK_METHOD(void, OnEncoderInternalScalerUpdate, (bool), (override));
  MOCK_METHOD(int, GetInputFrameRate, (), (const, override));
};

class FakeFrameRateProvider : public MockVideoStreamEncoderObserver {
 public:
  FakeFrameRateProvider();
  void set_fps(int fps);
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_TEST_FAKE_FRAME_RATE_PROVIDER_H_
