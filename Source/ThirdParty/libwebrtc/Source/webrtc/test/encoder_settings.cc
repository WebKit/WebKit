/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/encoder_settings.h"

#include <algorithm>
#include <string>

#include "rtc_base/refcountedobject.h"

namespace webrtc {
namespace test {

const size_t DefaultVideoStreamFactory::kMaxNumberOfStreams;
const int DefaultVideoStreamFactory::kMaxBitratePerStream[] = {150000, 450000,
                                                               1500000};
const int DefaultVideoStreamFactory::kDefaultMinBitratePerStream[] = {
    30000, 200000, 700000};

// static
std::vector<VideoStream> CreateVideoStreams(
    int width,
    int height,
    const webrtc::VideoEncoderConfig& encoder_config) {
  RTC_DCHECK(encoder_config.number_of_streams <=
             DefaultVideoStreamFactory::kMaxNumberOfStreams);

  std::vector<VideoStream> stream_settings(encoder_config.number_of_streams);
  int bitrate_left_bps = encoder_config.max_bitrate_bps;

  for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
    stream_settings[i].width =
        (i + 1) * width / encoder_config.number_of_streams;
    stream_settings[i].height =
        (i + 1) * height / encoder_config.number_of_streams;
    stream_settings[i].max_framerate = 30;
    stream_settings[i].min_bitrate_bps =
        DefaultVideoStreamFactory::kDefaultMinBitratePerStream[i];

    int target_bitrate_bps = -1;
    int max_bitrate_bps = -1;
    // Use configured values instead of default values if values has been
    // configured.
    if (i < encoder_config.simulcast_layers.size()) {
      const VideoStream& stream = encoder_config.simulcast_layers[i];

      max_bitrate_bps =
          stream.max_bitrate_bps > 0
              ? stream.max_bitrate_bps
              : DefaultVideoStreamFactory::kMaxBitratePerStream[i];
      max_bitrate_bps = std::min(bitrate_left_bps, max_bitrate_bps);

      target_bitrate_bps =
          stream.target_bitrate_bps > 0
              ? stream.target_bitrate_bps
              : DefaultVideoStreamFactory::kMaxBitratePerStream[i];
      target_bitrate_bps = std::min(max_bitrate_bps, target_bitrate_bps);

      if (stream.max_framerate > 0) {
        stream_settings[i].max_framerate = stream.max_framerate;
      }
      if (stream.num_temporal_layers) {
        RTC_DCHECK_GE(*stream.num_temporal_layers, 1);
        stream_settings[i].num_temporal_layers = stream.num_temporal_layers;
      }
    } else {
      max_bitrate_bps = std::min(
          bitrate_left_bps, DefaultVideoStreamFactory::kMaxBitratePerStream[i]);
      target_bitrate_bps = max_bitrate_bps;
    }

    RTC_DCHECK_NE(target_bitrate_bps, -1);
    RTC_DCHECK_NE(max_bitrate_bps, -1);
    stream_settings[i].target_bitrate_bps = target_bitrate_bps;
    stream_settings[i].max_bitrate_bps = max_bitrate_bps;
    stream_settings[i].max_qp = 56;

    if (i < encoder_config.simulcast_layers.size()) {
      // Higher level controls are setting the active configuration for the
      // VideoStream.
      stream_settings[i].active = encoder_config.simulcast_layers[i].active;
    } else {
      stream_settings[i].active = true;
    }
    bitrate_left_bps -= stream_settings[i].target_bitrate_bps;
  }

  stream_settings[encoder_config.number_of_streams - 1].max_bitrate_bps +=
      bitrate_left_bps;
  stream_settings[0].bitrate_priority = encoder_config.bitrate_priority;

  return stream_settings;
}

DefaultVideoStreamFactory::DefaultVideoStreamFactory() {}

std::vector<VideoStream> DefaultVideoStreamFactory::CreateEncoderStreams(
    int width,
    int height,
    const webrtc::VideoEncoderConfig& encoder_config) {
  return CreateVideoStreams(width, height, encoder_config);
}

void FillEncoderConfiguration(VideoCodecType codec_type,
                              size_t num_streams,
                              VideoEncoderConfig* configuration) {
  RTC_DCHECK_LE(num_streams, DefaultVideoStreamFactory::kMaxNumberOfStreams);

  configuration->codec_type = codec_type;
  configuration->number_of_streams = num_streams;
  configuration->video_stream_factory =
      new rtc::RefCountedObject<DefaultVideoStreamFactory>();
  configuration->max_bitrate_bps = 0;
  configuration->simulcast_layers = std::vector<VideoStream>(num_streams);
  for (size_t i = 0; i < num_streams; ++i) {
    configuration->max_bitrate_bps +=
        DefaultVideoStreamFactory::kMaxBitratePerStream[i];
  }
}

VideoReceiveStream::Decoder CreateMatchingDecoder(
    int payload_type,
    const std::string& payload_name) {
  VideoReceiveStream::Decoder decoder;
  decoder.payload_type = payload_type;
  decoder.video_format = SdpVideoFormat(payload_name);
  return decoder;
}

VideoReceiveStream::Decoder CreateMatchingDecoder(
    const VideoSendStream::Config& config) {
  return CreateMatchingDecoder(config.rtp.payload_type,
                               config.rtp.payload_name);
}

}  // namespace test
}  // namespace webrtc
