/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_MOCK_H_
#define MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_MOCK_H_

#include "api/audio/echo_control.h"
#include "test/gmock.h"

namespace webrtc {

class AudioBuffer;

class MockEchoControl : public EchoControl {
 public:
  MOCK_METHOD1(AnalyzeRender, void(AudioBuffer* render));
  MOCK_METHOD1(AnalyzeCapture, void(AudioBuffer* capture));
  MOCK_METHOD2(ProcessCapture,
               void(AudioBuffer* capture, bool echo_path_change));
  MOCK_METHOD3(ProcessCapture,
               void(AudioBuffer* capture,
                    AudioBuffer* linear_output,
                    bool echo_path_change));
  MOCK_CONST_METHOD0(GetMetrics, EchoControl::Metrics());
  MOCK_METHOD1(SetAudioBufferDelay, void(int delay_ms));
  MOCK_CONST_METHOD0(ActiveProcessing, bool());
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_ECHO_CONTROL_MOCK_H_
