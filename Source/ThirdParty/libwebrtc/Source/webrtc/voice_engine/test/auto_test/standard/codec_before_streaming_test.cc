/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fixtures/after_initialization_fixture.h"

class CodecBeforeStreamingTest : public AfterInitializationFixture {
 protected:
  void SetUp() {
    memset(&codec_instance_, 0, sizeof(codec_instance_));
    codec_instance_.channels = 1;
    codec_instance_.plfreq = 16000;
    codec_instance_.pacsize = 480;

    channel_ = voe_base_->CreateChannel();
  }

  void TearDown() {
    voe_base_->DeleteChannel(channel_);
  }

  webrtc::CodecInst codec_instance_;
  int channel_;
};

// TODO(phoglund): add test which verifies default pltypes for various codecs.

TEST_F(CodecBeforeStreamingTest, GetRecPayloadTypeFailsForInvalidCodecName) {
  strcpy(codec_instance_.plname, "SomeInvalidCodecName");

  // Should fail since the codec name is invalid.
  EXPECT_NE(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));
}

TEST_F(CodecBeforeStreamingTest, GetRecPayloadTypeRecognizesISAC) {
  strcpy(codec_instance_.plname, "iSAC");
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));
  strcpy(codec_instance_.plname, "ISAC");
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));
}

TEST_F(CodecBeforeStreamingTest, SetRecPayloadTypeCanChangeISACPayloadType) {
  strcpy(codec_instance_.plname, "ISAC");
  codec_instance_.rate = 32000;

  codec_instance_.pltype = 123;
  EXPECT_EQ(0, voe_codec_->SetRecPayloadType(channel_, codec_instance_));
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));
  EXPECT_EQ(123, codec_instance_.pltype);

  codec_instance_.pltype = 104;
  EXPECT_EQ(0, voe_codec_->SetRecPayloadType(channel_, codec_instance_));
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));

  EXPECT_EQ(104, codec_instance_.pltype);
}

TEST_F(CodecBeforeStreamingTest, SetRecPayloadTypeCanChangeILBCPayloadType) {
  strcpy(codec_instance_.plname, "iLBC");
  codec_instance_.plfreq = 8000;
  codec_instance_.pacsize = 240;
  codec_instance_.rate = 13300;

  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));
  int original_pltype = codec_instance_.pltype;
  codec_instance_.pltype = 123;
  EXPECT_EQ(0, voe_codec_->SetRecPayloadType(channel_, codec_instance_));
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));

  EXPECT_EQ(123, codec_instance_.pltype);

  codec_instance_.pltype = original_pltype;
  EXPECT_EQ(0, voe_codec_->SetRecPayloadType(channel_, codec_instance_));
  EXPECT_EQ(0, voe_codec_->GetRecPayloadType(channel_, codec_instance_));

  EXPECT_EQ(original_pltype, codec_instance_.pltype);
}
