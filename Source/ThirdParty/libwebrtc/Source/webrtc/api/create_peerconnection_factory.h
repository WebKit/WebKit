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
// IWYU pragma: no_include "rtc_base/thread.h"

#include <memory>

#include "api/audio/audio_device.h"
#include "api/audio/audio_mixer.h"
#include "api/audio/audio_processing.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/field_trials_view.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {
// TODO(bugs.webrtc.org/9987): Move rtc::Thread to api/ or expose a better
// type. At the moment, rtc::Thread is not part of api/ so it cannot be
// included in order to avoid to leak internal types.
class Thread;  // IWYU pragma: keep
}  // namespace rtc
namespace webrtc {
class AudioFrameProcessor;

// Create a new instance of PeerConnectionFactoryInterface with optional video
// codec factories. These video factories represents all video codecs, i.e. no
// extra internal video codecs will be added.
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
    rtc::scoped_refptr<AudioProcessing> audio_processing,
    std::unique_ptr<AudioFrameProcessor> audio_frame_processor = nullptr,
    std::unique_ptr<FieldTrialsView> field_trials = nullptr
#if defined(WEBRTC_WEBKIT_BUILD)
    , std::unique_ptr<TaskQueueFactory> task_queue_factory = nullptr
#endif
    );

}  // namespace webrtc

#endif  // API_CREATE_PEERCONNECTION_FACTORY_H_
