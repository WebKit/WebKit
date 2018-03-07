/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/include/voe_base.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/refcountedobject.h"
#include "test/gtest.h"

namespace webrtc {

class VoEBaseTest : public ::testing::Test {
 protected:
  VoEBaseTest()
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)) {
    EXPECT_NE(nullptr, base_);
    apm_ = new rtc::RefCountedObject<test::MockAudioProcessing>();
  }

  ~VoEBaseTest() {
    base_->Terminate();
    EXPECT_EQ(1, base_->Release());
    EXPECT_TRUE(VoiceEngine::Delete(voe_));
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  FakeAudioDeviceModule adm_;
  rtc::scoped_refptr<AudioProcessing> apm_;
};

TEST_F(VoEBaseTest, InitWithExternalAudioDevice) {
  EXPECT_EQ(0,
            base_->Init(&adm_, apm_.get(), CreateBuiltinAudioDecoderFactory()));
}

TEST_F(VoEBaseTest, CreateChannelAfterInit) {
  EXPECT_EQ(0,
            base_->Init(&adm_, apm_.get(), CreateBuiltinAudioDecoderFactory()));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(0, base_->DeleteChannel(channelID));
}

}  // namespace webrtc
