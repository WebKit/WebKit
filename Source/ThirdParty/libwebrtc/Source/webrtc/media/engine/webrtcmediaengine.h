/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
#define MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "call/call.h"
#include "media/base/mediaengine.h"
#include "modules/audio_device/include/audio_device.h"

namespace webrtc {
class AudioDecoderFactory;
class AudioMixer;
class AudioProcessing;
class VideoDecoderFactory;
class VideoEncoderFactory;
}  // namespace webrtc
namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}  // namespace cricket

namespace cricket {

class WebRtcMediaEngineFactory {
 public:
  // These Create methods may be called on any thread, though the engine is
  // only expected to be used on one thread, internally called the "worker
  // thread". This is the thread Init must be called on.
  //
  // TODO(deadbeef): Change these to return an std::unique_ptr<>, to indicate
  // that the caller owns the returned object.
  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
          audio_encoder_factory,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
          audio_decoder_factory,
      WebRtcVideoEncoderFactory* video_encoder_factory,
      WebRtcVideoDecoderFactory* video_decoder_factory);
  static MediaEngineInterface* Create(
      webrtc::AudioDeviceModule* adm,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
          audio_encoder_factory,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
          audio_decoder_factory,
      WebRtcVideoEncoderFactory* video_encoder_factory,
      WebRtcVideoDecoderFactory* video_decoder_factory,
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
      rtc::scoped_refptr<webrtc::AudioProcessing> apm);

  // Create a MediaEngineInterface with optional video codec factories. These
  // video factories represents all video codecs, i.e. no extra internal video
  // codecs will be added.
  static std::unique_ptr<MediaEngineInterface> Create(
      rtc::scoped_refptr<webrtc::AudioDeviceModule> adm,
      rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
      std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
      std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
      rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
      rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing);
};

// Verify that extension IDs are within 1-byte extension range and are not
// overlapping.
bool ValidateRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions);

// Discard any extensions not validated by the 'supported' predicate. Duplicate
// extensions are removed if 'filter_redundant_extensions' is set, and also any
// mutually exclusive extensions (see implementation for details) are removed.
std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions,
    bool (*supported)(const std::string&),
    bool filter_redundant_extensions);

webrtc::BitrateConstraints GetBitrateConfigForCodec(const Codec& codec);

}  // namespace cricket

#endif  // MEDIA_ENGINE_WEBRTCMEDIAENGINE_H_
