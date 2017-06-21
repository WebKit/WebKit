/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/webrtcmediaengine.h"

#include <algorithm>

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/media/engine/webrtcvoiceengine.h"

#ifdef HAVE_WEBRTC_VIDEO
#include "webrtc/media/engine/webrtcvideoengine.h"
#else
#include "webrtc/media/engine/nullwebrtcvideoengine.h"
#endif

namespace cricket {

class WebRtcMediaEngine2
#ifdef HAVE_WEBRTC_VIDEO
    : public CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine> {
#else
    : public CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine> {
#endif
 public:
  WebRtcMediaEngine2(webrtc::AudioDeviceModule* adm,
                     const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
                         audio_encoder_factory,
                     const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
                         audio_decoder_factory,
                     WebRtcVideoEncoderFactory* video_encoder_factory,
                     WebRtcVideoDecoderFactory* video_decoder_factory,
                     rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer)
#ifdef HAVE_WEBRTC_VIDEO
      : CompositeMediaEngine<WebRtcVoiceEngine, WebRtcVideoEngine>(
            adm,
            audio_encoder_factory,
            audio_decoder_factory,
            audio_mixer){
#else
      : CompositeMediaEngine<WebRtcVoiceEngine, NullWebRtcVideoEngine>(
            adm,
            audio_encoder_factory,
            audio_decoder_factory,
            audio_mixer) {
#endif
            video_.SetExternalDecoderFactory(video_decoder_factory);
  video_.SetExternalEncoderFactory(video_encoder_factory);
  }
};

}  // namespace cricket

cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  return new cricket::WebRtcMediaEngine2(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, audio_mixer);
}

void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
  delete media_engine;
}

namespace cricket {

// TODO(ossu): Backwards-compatible interface. Will be deprecated once the
// audio decoder factory is fully plumbed and used throughout WebRTC.
// See: crbug.com/webrtc/6000
MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreateWebRtcMediaEngine(
      adm, webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(), video_encoder_factory,
      video_decoder_factory, nullptr);
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreateWebRtcMediaEngine(
      adm, webrtc::CreateBuiltinAudioEncoderFactory(), audio_decoder_factory,
      video_encoder_factory, video_decoder_factory, nullptr);
}

// Used by PeerConnectionFactory to create a media engine passed into
// ChannelManager.
MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  return CreateWebRtcMediaEngine(
      adm, webrtc::CreateBuiltinAudioEncoderFactory(), audio_decoder_factory,
      video_encoder_factory, video_decoder_factory, audio_mixer);
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory) {
  return CreateWebRtcMediaEngine(adm, audio_encoder_factory,
                                 audio_decoder_factory, video_encoder_factory,
                                 video_decoder_factory, nullptr);
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  return CreateWebRtcMediaEngine(adm, audio_encoder_factory,
                                 audio_decoder_factory, video_encoder_factory,
                                 video_decoder_factory, audio_mixer);
}

namespace {
// Remove mutually exclusive extensions with lower priority.
void DiscardRedundantExtensions(
    std::vector<webrtc::RtpExtension>* extensions,
    rtc::ArrayView<const char*> extensions_decreasing_prio) {
  RTC_DCHECK(extensions);
  bool found = false;
  for (const char* uri : extensions_decreasing_prio) {
    auto it = std::find_if(
        extensions->begin(), extensions->end(),
        [uri](const webrtc::RtpExtension& rhs) { return rhs.uri == uri; });
    if (it != extensions->end()) {
      if (found) {
        extensions->erase(it);
      }
      found = true;
    }
  }
}
}  // namespace

bool ValidateRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  bool id_used[14] = {false};
  for (const auto& extension : extensions) {
    if (extension.id <= 0 || extension.id >= 15) {
      LOG(LS_ERROR) << "Bad RTP extension ID: " << extension.ToString();
      return false;
    }
    if (id_used[extension.id - 1]) {
      LOG(LS_ERROR) << "Duplicate RTP extension ID: " << extension.ToString();
      return false;
    }
    id_used[extension.id - 1] = true;
  }
  return true;
}

std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions,
    bool (*supported)(const std::string&),
    bool filter_redundant_extensions) {
  RTC_DCHECK(ValidateRtpExtensions(extensions));
  RTC_DCHECK(supported);
  std::vector<webrtc::RtpExtension> result;

  // Ignore any extensions that we don't recognize.
  for (const auto& extension : extensions) {
    if (supported(extension.uri)) {
      result.push_back(extension);
    } else {
      LOG(LS_WARNING) << "Unsupported RTP extension: " << extension.ToString();
    }
  }

  // Sort by name, ascending, so that we don't reset extensions if they were
  // specified in a different order (also allows us to use std::unique below).
  std::sort(result.begin(), result.end(),
            [](const webrtc::RtpExtension& rhs,
               const webrtc::RtpExtension& lhs) { return rhs.uri < lhs.uri; });

  // Remove unnecessary extensions (used on send side).
  if (filter_redundant_extensions) {
    auto it = std::unique(
        result.begin(), result.end(),
        [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
          return rhs.uri == lhs.uri;
        });
    result.erase(it, result.end());

    // Keep just the highest priority extension of any in the following list.
    static const char* kBweExtensionPriorities[] = {
        webrtc::RtpExtension::kTransportSequenceNumberUri,
        webrtc::RtpExtension::kAbsSendTimeUri,
        webrtc::RtpExtension::kTimestampOffsetUri};
    DiscardRedundantExtensions(&result, kBweExtensionPriorities);
  }

  return result;
}

webrtc::Call::Config::BitrateConfig GetBitrateConfigForCodec(
    const Codec& codec) {
  webrtc::Call::Config::BitrateConfig config;
  int bitrate_kbps = 0;
  if (codec.GetParam(kCodecParamMinBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.min_bitrate_bps = bitrate_kbps * 1000;
  } else {
    config.min_bitrate_bps = 0;
  }
  if (codec.GetParam(kCodecParamStartBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.start_bitrate_bps = bitrate_kbps * 1000;
  } else {
    // Do not reconfigure start bitrate unless it's specified and positive.
    config.start_bitrate_bps = -1;
  }
  if (codec.GetParam(kCodecParamMaxBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.max_bitrate_bps = bitrate_kbps * 1000;
  } else {
    config.max_bitrate_bps = -1;
  }
  return config;
}
}  // namespace cricket
