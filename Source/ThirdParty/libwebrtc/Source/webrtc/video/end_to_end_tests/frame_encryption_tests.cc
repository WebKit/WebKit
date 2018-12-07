/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/fake_frame_decryptor.h"
#include "api/test/fake_frame_encryptor.h"
#include "media/engine/internaldecoderfactory.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

class FrameEncryptionEndToEndTest : public test::CallTest {
 public:
  FrameEncryptionEndToEndTest() = default;

 private:
  // GenericDescriptor is required for FrameEncryption to work.
  test::ScopedFieldTrials field_trials_{"WebRTC-GenericDescriptor/Enabled/"};
};

// Validates that payloads cannot be sent without a frame encryptor and frame
// decryptor attached.
TEST_F(FrameEncryptionEndToEndTest, RequireFrameEncryptionEnforced) {
  class DecryptedFrameObserver : public test::EndToEndTest,
                                 public rtc::VideoSinkInterface<VideoFrame> {
   public:
    DecryptedFrameObserver()
        : EndToEndTest(kDefaultTimeoutMs),
          encoder_factory_([]() { return VP8Encoder::Create(); }) {}

   private:
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // Use VP8 instead of FAKE.
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.payload_name = "VP8";
      send_config->rtp.payload_type = kVideoSendPayloadType;
      send_config->frame_encryptor = new FakeFrameEncryptor();
      send_config->crypto_options.sframe.require_frame_encryption = true;
      encoder_config->codec_type = kVideoCodecVP8;
      VideoReceiveStream::Decoder decoder =
          test::CreateMatchingDecoder(*send_config);
      decoder.decoder_factory = &decoder_factory_;
      for (auto& recv_config : *receive_configs) {
        recv_config.decoders.clear();
        recv_config.decoders.push_back(decoder);
        recv_config.renderer = this;
        recv_config.frame_decryptor = new FakeFrameDecryptor();
        recv_config.crypto_options.sframe.require_frame_encryption = true;
      }
    }

    // Validate that rotation is preserved.
    void OnFrame(const VideoFrame& video_frame) override {
      observation_complete_.Set();
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait())
          << "Timed out waiting for decrypted frames to be rendered.";
    }

    std::unique_ptr<VideoEncoder> encoder_;
    test::FunctionVideoEncoderFactory encoder_factory_;
    InternalDecoderFactory decoder_factory_;
  } test;

  RunBaseTest(&test);
}
}  // namespace webrtc
