/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtcmediaengine.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/engine/webrtcvoiceengine.h"

#ifdef HAVE_WEBRTC_VIDEO
#include "media/engine/webrtcvideoengine.h"
#else
#include "media/engine/nullwebrtcvideoengine.h"
#endif

namespace cricket {

#if defined(USE_BUILTIN_SW_CODECS)
namespace {

MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
        video_bitrate_allocator_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  std::unique_ptr<VideoEngineInterface> video_engine;
#ifdef HAVE_WEBRTC_VIDEO
  video_engine = absl::make_unique<WebRtcVideoEngine>(
      std::unique_ptr<WebRtcVideoEncoderFactory>(video_encoder_factory),
      std::unique_ptr<WebRtcVideoDecoderFactory>(video_decoder_factory),
      std::move(video_bitrate_allocator_factory));
#else
  video_engine = absl::make_unique<NullWebRtcVideoEngine>();
#endif
  return new CompositeMediaEngine(
      absl::make_unique<WebRtcVoiceEngine>(adm, audio_encoder_factory,
                                           audio_decoder_factory, audio_mixer,
                                           audio_processing),
      std::move(video_engine));
}

}  // namespace

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory) {
  return WebRtcMediaEngineFactory::Create(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory,
      webrtc::CreateBuiltinVideoBitrateAllocatorFactory());
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
        video_bitrate_allocator_factory) {
  return CreateWebRtcMediaEngine(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, std::move(video_bitrate_allocator_factory),
      nullptr, webrtc::AudioProcessingBuilder().Create());
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  return WebRtcMediaEngineFactory::Create(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory,
      webrtc::CreateBuiltinVideoBitrateAllocatorFactory(), audio_mixer,
      audio_processing);
}

MediaEngineInterface* WebRtcMediaEngineFactory::Create(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>&
        audio_encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    WebRtcVideoEncoderFactory* video_encoder_factory,
    WebRtcVideoDecoderFactory* video_decoder_factory,
    std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
        video_bitrate_allocator_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  return CreateWebRtcMediaEngine(
      adm, audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
      video_decoder_factory, std::move(video_bitrate_allocator_factory),
      audio_mixer, audio_processing);
}
#endif

std::unique_ptr<MediaEngineInterface> WebRtcMediaEngineFactory::Create(
    rtc::scoped_refptr<webrtc::AudioDeviceModule> adm,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
  return WebRtcMediaEngineFactory::Create(
      adm, audio_encoder_factory, audio_decoder_factory,
      std::move(video_encoder_factory), std::move(video_decoder_factory),
      webrtc::CreateBuiltinVideoBitrateAllocatorFactory(), audio_mixer,
      audio_processing);
}

std::unique_ptr<MediaEngineInterface> WebRtcMediaEngineFactory::Create(
    rtc::scoped_refptr<webrtc::AudioDeviceModule> adm,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
    std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
        video_bitrate_allocator_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing) {
#ifdef HAVE_WEBRTC_VIDEO
  auto video_engine = absl::make_unique<WebRtcVideoEngine>(
      std::move(video_encoder_factory), std::move(video_decoder_factory),
      std::move(video_bitrate_allocator_factory));
#else
  auto video_engine = absl::make_unique<NullWebRtcVideoEngine>();
#endif
  return std::unique_ptr<MediaEngineInterface>(new CompositeMediaEngine(
      absl::make_unique<WebRtcVoiceEngine>(adm, audio_encoder_factory,
                                           audio_decoder_factory, audio_mixer,
                                           audio_processing),
      std::move(video_engine)));
}

namespace {
// Remove mutually exclusive extensions with lower priority.
void DiscardRedundantExtensions(
    std::vector<webrtc::RtpExtension>* extensions,
    rtc::ArrayView<const char* const> extensions_decreasing_prio) {
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
  bool id_used[1 + webrtc::RtpExtension::kMaxId] = {false};
  for (const auto& extension : extensions) {
    if (extension.id < webrtc::RtpExtension::kMinId ||
        extension.id > webrtc::RtpExtension::kMaxId) {
      RTC_LOG(LS_ERROR) << "Bad RTP extension ID: " << extension.ToString();
      return false;
    }
    if (id_used[extension.id]) {
      RTC_LOG(LS_ERROR) << "Duplicate RTP extension ID: "
                        << extension.ToString();
      return false;
    }
    id_used[extension.id] = true;
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
      RTC_LOG(LS_WARNING) << "Unsupported RTP extension: "
                          << extension.ToString();
    }
  }

  // Sort by name, ascending (prioritise encryption), so that we don't reset
  // extensions if they were specified in a different order (also allows us
  // to use std::unique below).
  std::sort(
      result.begin(), result.end(),
      [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
        return rhs.encrypt == lhs.encrypt ? rhs.uri < lhs.uri
                                          : rhs.encrypt > lhs.encrypt;
      });

  // Remove unnecessary extensions (used on send side).
  if (filter_redundant_extensions) {
    auto it = std::unique(
        result.begin(), result.end(),
        [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
          return rhs.uri == lhs.uri && rhs.encrypt == lhs.encrypt;
        });
    result.erase(it, result.end());

    // Keep just the highest priority extension of any in the following list.
    static const char* const kBweExtensionPriorities[] = {
        webrtc::RtpExtension::kTransportSequenceNumberUri,
        webrtc::RtpExtension::kAbsSendTimeUri,
        webrtc::RtpExtension::kTimestampOffsetUri};
    DiscardRedundantExtensions(&result, kBweExtensionPriorities);
  }

  return result;
}

webrtc::BitrateConstraints GetBitrateConfigForCodec(const Codec& codec) {
  webrtc::BitrateConstraints config;
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
