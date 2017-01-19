/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_initialization_fixture.h"

using namespace webrtc;

class HardwareBeforeInitializingTest : public BeforeInitializationFixture {
};

TEST_F(HardwareBeforeInitializingTest,
       SetAudioDeviceLayerAcceptsPlatformDefaultBeforeInitializing) {
  AudioLayers wanted_layer = kAudioPlatformDefault;
  AudioLayers given_layer;
  EXPECT_EQ(0, voe_hardware_->SetAudioDeviceLayer(wanted_layer));
  EXPECT_EQ(0, voe_hardware_->GetAudioDeviceLayer(given_layer));
  EXPECT_EQ(wanted_layer, given_layer) <<
      "These should be the same before initializing.";
}
