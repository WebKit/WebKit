/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/include/video_codec_initializer.h"

#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/video_bitrate_allocator.h"
#include "modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "modules/video_coding/codecs/vp8/simulcast_rate_allocator.h"
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "rtc_base/basictypes.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
bool TemporalLayersConfigured(const std::vector<VideoStream>& streams) {
  for (const VideoStream& stream : streams) {
    if (stream.temporal_layer_thresholds_bps.size() > 0)
      return true;
  }
  return false;
}
}  // namespace

bool VideoCodecInitializer::SetupCodec(
    const VideoEncoderConfig& config,
    const VideoSendStream::Config::EncoderSettings settings,
    const std::vector<VideoStream>& streams,
    bool nack_enabled,
    VideoCodec* codec,
    std::unique_ptr<VideoBitrateAllocator>* bitrate_allocator) {
  if (PayloadStringToCodecType(settings.payload_name) == kVideoCodecStereo) {
    VideoSendStream::Config::EncoderSettings associated_codec_settings =
        settings;
    associated_codec_settings.payload_name =
        CodecTypeToPayloadString(kVideoCodecVP9);
    if (!SetupCodec(config, associated_codec_settings, streams, nack_enabled,
                    codec, bitrate_allocator)) {
      RTC_LOG(LS_ERROR) << "Failed to create stereo encoder configuration.";
      return false;
    }
    codec->codecType = kVideoCodecStereo;
    strncpy(codec->plName, settings.payload_name.c_str(),
            sizeof(codec->plName));
    return true;
  }

  *codec =
      VideoEncoderConfigToVideoCodec(config, streams, settings.payload_name,
                                     settings.payload_type, nack_enabled);
#if !defined(RTC_DISABLE_VP8)
  std::unique_ptr<TemporalLayersFactory> tl_factory;
  switch (codec->codecType) {
    case kVideoCodecVP8: {
      if (!codec->VP8()->tl_factory) {
        if (codec->mode == kScreensharing &&
            (codec->numberOfSimulcastStreams > 1 ||
             (codec->numberOfSimulcastStreams == 1 &&
              codec->VP8()->numberOfTemporalLayers == 2))) {
          // Conference mode temporal layering for screen content.
          tl_factory.reset(new ScreenshareTemporalLayersFactory());
        } else {
          // Standard video temporal layers.
          tl_factory.reset(new TemporalLayersFactory());
        }
        codec->VP8()->tl_factory = tl_factory.get();
      }
      break;
    }
    default: {
      // TODO(sprang): Warn, once we have specific allocators for all supported
      //               codec types.
      break;
    }
  }
  *bitrate_allocator = CreateBitrateAllocator(*codec, std::move(tl_factory));
#endif
  return true;
}

std::unique_ptr<VideoBitrateAllocator>
VideoCodecInitializer::CreateBitrateAllocator(
    const VideoCodec& codec,
    std::unique_ptr<TemporalLayersFactory> tl_factory) {
  std::unique_ptr<VideoBitrateAllocator> rate_allocator;

  switch (codec.codecType) {
    case kVideoCodecVP8: {
#if !defined(RTC_DISABLE_VP8)
      // Set up default VP8 temporal layer factory, if not provided.
      rate_allocator.reset(
          new SimulcastRateAllocator(codec, std::move(tl_factory)));
#endif
    } break;
    default:
      rate_allocator.reset(new DefaultVideoBitrateAllocator(codec));
  }

  return rate_allocator;
}

// TODO(sprang): Split this up and separate the codec specific parts.
VideoCodec VideoCodecInitializer::VideoEncoderConfigToVideoCodec(
    const VideoEncoderConfig& config,
    const std::vector<VideoStream>& streams,
    const std::string& payload_name,
    int payload_type,
    bool nack_enabled) {
  static const int kEncoderMinBitrateKbps = 30;
  RTC_DCHECK(!streams.empty());
  RTC_DCHECK_GE(config.min_transmit_bitrate_bps, 0);

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType = PayloadStringToCodecType(payload_name);

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (!streams.empty() &&
          streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  if (config.encoder_specific_settings)
    config.encoder_specific_settings->FillEncoderSpecificSettings(&video_codec);

  switch (video_codec.codecType) {
    case kVideoCodecVP8: {
      if (!config.encoder_specific_settings)
        *video_codec.VP8() = VideoEncoder::GetDefaultVp8Settings();
      video_codec.VP8()->numberOfTemporalLayers = static_cast<unsigned char>(
          streams.back().temporal_layer_thresholds_bps.size() + 1);

      if (nack_enabled && !TemporalLayersConfigured(streams)) {
        RTC_LOG(LS_INFO)
            << "No temporal layers and nack enabled -> resilience off";
        video_codec.VP8()->resilience = kResilienceOff;
      }
      break;
    }
    case kVideoCodecVP9: {
      if (!config.encoder_specific_settings)
        *video_codec.VP9() = VideoEncoder::GetDefaultVp9Settings();
      if (video_codec.mode == kScreensharing &&
          config.encoder_specific_settings) {
        video_codec.VP9()->flexibleMode = true;
        // For now VP9 screensharing use 1 temporal and 2 spatial layers.
        RTC_DCHECK_EQ(1, video_codec.VP9()->numberOfTemporalLayers);
        RTC_DCHECK_EQ(2, video_codec.VP9()->numberOfSpatialLayers);
      }
      video_codec.VP9()->numberOfTemporalLayers = static_cast<unsigned char>(
          streams.back().temporal_layer_thresholds_bps.size() + 1);

      if (nack_enabled && !TemporalLayersConfigured(streams) &&
          video_codec.VP9()->numberOfSpatialLayers == 1) {
        RTC_LOG(LS_INFO) << "No temporal or spatial layers and nack enabled -> "
                         << "resilience off";
        video_codec.VP9()->resilienceOn = false;
      }
      break;
    }
    case kVideoCodecH264: {
      if (!config.encoder_specific_settings)
        *video_codec.H264() = VideoEncoder::GetDefaultH264Settings();
      break;
    }
    default:
      // TODO(pbos): Support encoder_settings codec-agnostically.
      RTC_DCHECK(!config.encoder_specific_settings)
          << "Encoder-specific settings for codec type not wired up.";
      break;
  }

  strncpy(video_codec.plName, payload_name.c_str(), kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  if (video_codec.minBitrate < kEncoderMinBitrateKbps)
    video_codec.minBitrate = kEncoderMinBitrateKbps;
  video_codec.timing_frame_thresholds = {kDefaultTimingFramesDelayMs,
                                         kDefaultOutlierFrameSizePercent};
  RTC_DCHECK_LE(streams.size(), kMaxSimulcastStreams);
  if (video_codec.codecType == kVideoCodecVP9) {
    // If the vector is empty, bitrates will be configured automatically.
    RTC_DCHECK(config.spatial_layers.empty() ||
               config.spatial_layers.size() ==
                   video_codec.VP9()->numberOfSpatialLayers);
    RTC_DCHECK_LE(video_codec.VP9()->numberOfSpatialLayers,
                  kMaxSimulcastStreams);
    for (size_t i = 0; i < config.spatial_layers.size(); ++i)
      video_codec.spatialLayers[i] = config.spatial_layers[i];
  }
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    RTC_DCHECK_GT(streams[i].width, 0);
    RTC_DCHECK_GT(streams[i].height, 0);
    RTC_DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment, unless it's
    // screenshare where there is an exception and a simulcast encoder adapter,
    // which supports different framerates, is used instead.
    if (config.content_type != VideoEncoderConfig::ContentType::kScreen) {
      RTC_DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    }
    RTC_DCHECK_GE(streams[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<uint16_t>(streams[i].width);
    sim_stream->height = static_cast<uint16_t>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width =
        std::max(video_codec.width, static_cast<uint16_t>(streams[i].width));
    video_codec.height =
        std::max(video_codec.height, static_cast<uint16_t>(streams[i].height));
    video_codec.minBitrate =
        std::min(static_cast<uint16_t>(video_codec.minBitrate),
                 static_cast<uint16_t>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }
  if (video_codec.maxBitrate < kEncoderMinBitrateKbps)
    video_codec.maxBitrate = kEncoderMinBitrateKbps;

  RTC_DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;
  return video_codec;
}

}  // namespace webrtc
