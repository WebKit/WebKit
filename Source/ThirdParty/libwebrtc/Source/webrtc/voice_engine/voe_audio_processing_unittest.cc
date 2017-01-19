/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_audio_processing.h"

#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace webrtc {
namespace voe {
namespace {

class VoEAudioProcessingTest : public ::testing::Test {
 protected:
  VoEAudioProcessingTest()
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        audioproc_(VoEAudioProcessing::GetInterface(voe_)) {}

  virtual ~VoEAudioProcessingTest() {
    base_->Terminate();
    audioproc_->Release();
    base_->Release();
    VoiceEngine::Delete(voe_);
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  VoEAudioProcessing* audioproc_;
};

TEST_F(VoEAudioProcessingTest, FailureIfNotInitialized) {
  EXPECT_EQ(-1, audioproc_->EnableDriftCompensation(true));
  EXPECT_EQ(-1, audioproc_->EnableDriftCompensation(false));
  EXPECT_FALSE(audioproc_->DriftCompensationEnabled());
}

// TODO(andrew): Investigate race conditions triggered by this test:
// https://code.google.com/p/webrtc/issues/detail?id=788
TEST_F(VoEAudioProcessingTest, DISABLED_DriftCompensationIsEnabledIfSupported) {
  ASSERT_EQ(0, base_->Init());
  // TODO(andrew): Ideally, DriftCompensationSupported() would be mocked.
  bool supported = VoEAudioProcessing::DriftCompensationSupported();
  if (supported) {
    EXPECT_EQ(0, audioproc_->EnableDriftCompensation(true));
    EXPECT_TRUE(audioproc_->DriftCompensationEnabled());
    EXPECT_EQ(0, audioproc_->EnableDriftCompensation(false));
    EXPECT_FALSE(audioproc_->DriftCompensationEnabled());
  } else {
    EXPECT_EQ(-1, audioproc_->EnableDriftCompensation(true));
    EXPECT_FALSE(audioproc_->DriftCompensationEnabled());
    EXPECT_EQ(-1, audioproc_->EnableDriftCompensation(false));
    EXPECT_FALSE(audioproc_->DriftCompensationEnabled());
  }
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
