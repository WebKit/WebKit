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

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/callfactoryinterface.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace jni {

CallFactoryInterface* CreateCallFactory() {
  return webrtc::CreateCallFactory().release();
}

RtcEventLogFactoryInterface* CreateRtcEventLogFactory() {
  return webrtc::CreateRtcEventLogFactory().release();
}

cricket::MediaEngineInterface* CreateMediaEngine(
    AudioDeviceModule* adm,
    const rtc::scoped_refptr<AudioEncoderFactory>& audio_encoder_factory,
    const rtc::scoped_refptr<AudioDecoderFactory>& audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processor) {
  return cricket::WebRtcMediaEngineFactory::Create(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, audio_processor);
}

cricket::MediaEngineInterface* CreateMediaEngine(
    rtc::scoped_refptr<AudioDeviceModule> adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processor) {
  return cricket::WebRtcMediaEngineFactory::Create(
             adm, audio_encoder_factory, audio_decoder_factory,
             std::move(video_encoder_factory), std::move(video_decoder_factory),
             audio_mixer, audio_processor)
      .release();
}

}  // namespace jni
}  // namespace webrtc
