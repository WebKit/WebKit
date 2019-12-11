/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_MEDIA_H_
#define SDK_ANDROID_SRC_JNI_PC_MEDIA_H_

#include <memory>

#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {
class AudioDeviceModule;
class CallFactoryInterface;
class AudioEncoderFactory;
class AudioDecoderFactory;
class RtcEventLogFactoryInterface;
class AudioMixer;
class AudioProcessing;
class VideoEncoderFactory;
class VideoDecoderFactory;
}  // namespace webrtc

namespace cricket {
class MediaEngineInterface;
}  // namespace cricket

namespace webrtc {
namespace jni {

CallFactoryInterface* CreateCallFactory();
RtcEventLogFactoryInterface* CreateRtcEventLogFactory();

cricket::MediaEngineInterface* CreateMediaEngine(
    rtc::scoped_refptr<AudioDeviceModule> adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processor);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_MEDIA_H_
