/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_TEST_MOCK_AUDIO_SEND_STREAM_H_
#define CALL_TEST_MOCK_AUDIO_SEND_STREAM_H_

#include <memory>

#include "call/audio_send_stream.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockAudioSendStream : public AudioSendStream {
 public:
  MOCK_CONST_METHOD0(GetConfig, const webrtc::AudioSendStream::Config&());
  MOCK_METHOD1(Reconfigure, void(const Config& config));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  // GMock doesn't like move-only types, such as std::unique_ptr.
  virtual void SendAudioData(
      std::unique_ptr<webrtc::AudioFrame> audio_frame) {
    SendAudioDataForMock(audio_frame.get());
  }
  MOCK_METHOD1(SendAudioDataForMock,
               void(webrtc::AudioFrame* audio_frame));
  MOCK_METHOD4(SendTelephoneEvent,
               bool(int payload_type, int payload_frequency, int event,
                    int duration_ms));
  MOCK_METHOD1(SetMuted, void(bool muted));
  MOCK_CONST_METHOD0(GetStats, Stats());
  MOCK_CONST_METHOD1(GetStats, Stats(bool has_remote_tracks));
};
}  // namespace test
}  // namespace webrtc

#endif  // CALL_TEST_MOCK_AUDIO_SEND_STREAM_H_
