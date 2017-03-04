/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/nullwebrtcvideoengine.h"
#include "webrtc/media/engine/webrtcvoiceengine.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/test/gtest.h"

namespace cricket {

class WebRtcMediaEngineNullVideo
    : public CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine> {
 public:
  WebRtcMediaEngineNullVideo(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
          audio_decoder_factory,
      WebRtcVideoEncoderFactory* video_encoder_factory,
      WebRtcVideoDecoderFactory* video_decoder_factory)
      : CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine>(
            adm,
            audio_decoder_factory,
            nullptr) {
    video_.SetExternalDecoderFactory(video_decoder_factory);
    video_.SetExternalEncoderFactory(video_encoder_factory);
  }
};

// Simple test to check if NullWebRtcVideoEngine implements the methods
// required by CompositeMediaEngine.
TEST(NullWebRtcVideoEngineTest, CheckInterface) {
  WebRtcMediaEngineNullVideo engine(
      nullptr, webrtc::MockAudioDecoderFactory::CreateUnusedFactory(), nullptr,
      nullptr);
  EXPECT_TRUE(engine.Init());
}

}  // namespace cricket
