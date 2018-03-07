/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/media_jni.h"

namespace webrtc {
namespace jni {

CallFactoryInterface* CreateCallFactory() {
  return nullptr;
}

RtcEventLogFactoryInterface* CreateRtcEventLogFactory() {
  return nullptr;
}

cricket::MediaEngineInterface* CreateMediaEngine(
    AudioDeviceModule* adm,
    const rtc::scoped_refptr<AudioEncoderFactory>& audio_encoder_factory,
    const rtc::scoped_refptr<AudioDecoderFactory>& audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processor) {
  return nullptr;
}

cricket::MediaEngineInterface* CreateMediaEngine(
    rtc::scoped_refptr<AudioDeviceModule> adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processor) {
  return nullptr;
}

}  // namespace jni
}  // namespace webrtc
