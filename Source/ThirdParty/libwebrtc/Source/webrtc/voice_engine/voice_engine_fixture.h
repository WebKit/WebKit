/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/mock/mock_voe_observer.h"

namespace webrtc {

class VoiceEngineFixture : public ::testing::Test {
 protected:
  VoiceEngineFixture();
  ~VoiceEngineFixture();

  VoiceEngine* voe_;
  VoEBase* base_;
  VoENetwork* network_;
  MockVoEObserver observer_;
  FakeAudioDeviceModule adm_;
  MockTransport transport_;
};

}  // namespace webrtc
