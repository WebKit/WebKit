/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/encoder_settings.h"

#include <algorithm>
#include <string>

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/test/fake_decoder.h"

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
    stream_settings[i].target_bitrate_bps = stream_settings[i].max_bitrate_bps =
        std::min(bitrate_left_bps,
                 DefaultVideoStreamFactory::kMaxBitratePerStream[i]);
    stream_settings[i].max_qp = 56;
    bitrate_left_bps -= stream_settings[i].target_bitrate_bps;
  }

  stream_settings[encoder_config.number_of_streams - 1].max_bitrate_bps +=
      bitrate_left_bps;

  return stream_settings;
}

DefaultVideoStreamFactory::DefaultVideoStreamFactory() {}

std::vector<VideoStream> DefaultVideoStreamFactory::CreateEncoderStreams(
    int width,
    int height,
    const webrtc::VideoEncoderConfig& encoder_config) {
  return CreateVideoStreams(width, height, encoder_config);
}

void FillEncoderConfiguration(size_t num_streams,
                              VideoEncoderConfig* configuration) {
  RTC_DCHECK_LE(num_streams, DefaultVideoStreamFactory::kMaxNumberOfStreams);

  configuration->number_of_streams = num_streams;
  configuration->video_stream_factory =
      new rtc::RefCountedObject<DefaultVideoStreamFactory>();
  configuration->max_bitrate_bps = 0;
  for (size_t i = 0; i < num_streams; ++i) {
    configuration->max_bitrate_bps +=
        DefaultVideoStreamFactory::kMaxBitratePerStream[i];
  }
}

VideoReceiveStream::Decoder CreateMatchingDecoder(
    const VideoSendStream::Config::EncoderSettings& encoder_settings) {
  VideoReceiveStream::Decoder decoder;
  decoder.payload_type = encoder_settings.payload_type;
  decoder.payload_name = encoder_settings.payload_name;
  if (encoder_settings.payload_name == "H264") {
    decoder.decoder = H264Decoder::Create();
  } else if (encoder_settings.payload_name == "VP8") {
    decoder.decoder = VP8Decoder::Create();
  } else if (encoder_settings.payload_name == "VP9") {
    decoder.decoder = VP9Decoder::Create();
  } else {
    decoder.decoder = new FakeDecoder();
  }
  return decoder;
}
}  // namespace test
}  // namespace webrtc
