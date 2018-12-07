/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CREATE_PEERCONNECTION_FACTORY_H_
#define API_CREATE_PEERCONNECTION_FACTORY_H_

#include <memory>

#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/fec_controller.h"
#include "api/peerconnectioninterface.h"
#include "api/transport/network_control.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace rtc {
// TODO(bugs.webrtc.org/9987): Move rtc::Thread to api/ or expose a better
// type. At the moment, rtc::Thread is not part of api/ so it cannot be
// included in order to avoid to leak internal types.
class Thread;
}  // namespace rtc

namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}  // namespace cricket

namespace webrtc {

class AudioDeviceModule;
class AudioProcessing;

#if defined(USE_BUILTIN_SW_CODECS)
// Create a new instance of PeerConnectionFactoryInterface.
//
// This method relies on the thread it's called on as the "signaling thread"
// for the PeerConnectionFactory it creates.
//
// As such, if the current thread is not already running an rtc::Thread message
// loop, an application using this method must eventually either call
// rtc::Thread::Current()->Run(), or call
// rtc::Thread::Current()->ProcessMessages() within the application's own
// message loop.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory);

// Create a new instance of PeerConnectionFactoryInterface.
//
// |network_thread|, |worker_thread| and |signaling_thread| are
// the only mandatory parameters.
//
// If non-null, a reference is added to |default_adm|, and ownership of
// |video_encoder_factory| and |video_decoder_factory| is transferred to the
// returned factory.
// TODO(deadbeef): Use rtc::scoped_refptr<> and std::unique_ptr<> to make this
// ownership transfer and ref counting more obvious.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory);

// Create a new instance of PeerConnectionFactoryInterface with optional
// external audio mixed and audio processing modules.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
// If |audio_processing| is null, an internal audio processing module will be
// created and used.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing);

// Create a new instance of PeerConnectionFactoryInterface with optional
// external audio mixer, audio processing, and fec controller modules.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
// If |audio_processing| is null, an internal audio processing module will be
// created and used.
// If |fec_controller_factory| is null, an internal fec controller module will
// be created and used.
// If |network_controller_factory| is provided, it will be used if enabled via
// field trial.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory = nullptr);
#endif  // defined(USE_BUILTIN_SW_CODECS)

// Create a new instance of PeerConnectionFactoryInterface with optional video
// codec factories. These video factories represents all video codecs, i.e. no
// extra internal video codecs will be added.
// When building WebRTC with rtc_use_builtin_sw_codecs = false, this is the
// only available CreatePeerConnectionFactory overload.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing);

#if defined(USE_BUILTIN_SW_CODECS)
// Create a new instance of PeerConnectionFactoryInterface with external audio
// mixer.
//
// If |audio_mixer| is null, an internal audio mixer will be created and used.
RTC_EXPORT rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactoryWithAudioMixer(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer);

// Create a new instance of PeerConnectionFactoryInterface.
// Same thread is used as worker and network thread.
RTC_EXPORT inline rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_and_network_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreatePeerConnectionFactory(
      worker_and_network_thread, worker_and_network_thread, signaling_thread,
      default_adm, audio_encoder_factory, audio_decoder_factory,
      video_encoder_factory, video_decoder_factory);
}
#endif  // defined(USE_BUILTIN_SW_CODECS)

}  // namespace webrtc

#endif  // API_CREATE_PEERCONNECTION_FACTORY_H_
