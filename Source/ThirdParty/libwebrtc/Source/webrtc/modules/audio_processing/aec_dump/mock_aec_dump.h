/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC_DUMP_MOCK_AEC_DUMP_H_
#define MODULES_AUDIO_PROCESSING_AEC_DUMP_MOCK_AEC_DUMP_H_

#include <memory>

#include "modules/audio_processing/include/aec_dump.h"
#include "test/gmock.h"

namespace webrtc {

namespace test {

class MockAecDump : public AecDump {
 public:
  MockAecDump();
  virtual ~MockAecDump();

  MOCK_METHOD2(WriteInitMessage,
               void(const ProcessingConfig& api_format, int64_t time_now_ms));

  MOCK_METHOD1(AddCaptureStreamInput,
               void(const AudioFrameView<const float>& src));
  MOCK_METHOD1(AddCaptureStreamOutput,
               void(const AudioFrameView<const float>& src));
  MOCK_METHOD1(AddCaptureStreamInput, void(const AudioFrame& frame));
  MOCK_METHOD1(AddCaptureStreamOutput, void(const AudioFrame& frame));
  MOCK_METHOD1(AddAudioProcessingState,
               void(const AudioProcessingState& state));
  MOCK_METHOD0(WriteCaptureStreamMessage, void());

  MOCK_METHOD1(WriteRenderStreamMessage, void(const AudioFrame& frame));
  MOCK_METHOD1(WriteRenderStreamMessage,
               void(const AudioFrameView<const float>& src));

  MOCK_METHOD1(WriteConfig, void(const InternalAPMConfig& config));

  MOCK_METHOD1(WriteRuntimeSetting,
               void(const AudioProcessing::RuntimeSetting& config));
};

}  // namespace test

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC_DUMP_MOCK_AEC_DUMP_H_
