/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/call/callfactoryinterface.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "webrtc/media/engine/webrtcmediaengine.h"

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory) {
  return CreatePeerConnectionFactoryWithAudioMixer(
      nullptr /*network_thread*/, nullptr /*worker_thread*/,
      nullptr /*signaling_thread*/, nullptr /*default_adm*/,
      audio_encoder_factory, audio_decoder_factory,
      nullptr /*video_encoder_factory*/, nullptr /*video_decoder_factory*/,
      nullptr /*audio_mixer*/);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  return CreatePeerConnectionFactory(CreateBuiltinAudioEncoderFactory(),
                                     CreateBuiltinAudioDecoderFactory());
}

// Note: all the other CreatePeerConnectionFactory variants just end up calling
// this, ultimately.
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer) {
  std::unique_ptr<cricket::MediaEngineInterface> media_engine(
      cricket::WebRtcMediaEngineFactory::Create(
          default_adm, audio_encoder_factory, audio_decoder_factory,
          video_encoder_factory, video_decoder_factory, audio_mixer));

  std::unique_ptr<CallFactoryInterface> call_factory = CreateCallFactory();

  std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory =
      CreateRtcEventLogFactory();

  return CreateModularPeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer, std::move(media_engine),
      std::move(call_factory), std::move(event_log_factory));
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer) {
  return CreatePeerConnectionFactoryWithAudioMixer(
      network_thread, worker_thread, signaling_thread, default_adm,
      CreateBuiltinAudioEncoderFactory(), CreateBuiltinAudioDecoderFactory(),
      encoder_factory, decoder_factory, audio_mixer);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  return CreatePeerConnectionFactoryWithAudioMixer(
      network_thread, worker_thread, signaling_thread, default_adm,
      encoder_factory, decoder_factory, nullptr);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreatePeerConnectionFactoryWithAudioMixer(
      network_thread, worker_thread, signaling_thread, default_adm,
      audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, nullptr);
}

}  // namespace webrtc
