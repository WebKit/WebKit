/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voice_engine_fixture.h"

namespace webrtc {

VoiceEngineFixture::VoiceEngineFixture()
    : voe_(VoiceEngine::Create()),
      base_(VoEBase::GetInterface(voe_)),
      network_(VoENetwork::GetInterface(voe_)) {
  EXPECT_NE(nullptr, base_);
  EXPECT_NE(nullptr, network_);
  EXPECT_EQ(0, base_->RegisterVoiceEngineObserver(observer_));
}

VoiceEngineFixture::~VoiceEngineFixture() {
  EXPECT_EQ(2, network_->Release());
  EXPECT_EQ(0, base_->DeRegisterVoiceEngineObserver());
  EXPECT_EQ(0, base_->Terminate());
  EXPECT_EQ(1, base_->Release());
  EXPECT_TRUE(VoiceEngine::Delete(voe_));
}

}  // namespace webrtc
