/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_ENCODER_SETTINGS_H_
#define WEBRTC_TEST_ENCODER_SETTINGS_H_

#include <vector>

#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {

class DefaultVideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  DefaultVideoStreamFactory();

  static const size_t kMaxNumberOfStreams = 3;
  // Defined as {150000, 450000, 1500000};
  static const int kMaxBitratePerStream[];
  // Defined as {50000, 200000, 700000};
  static const int kDefaultMinBitratePerStream[];

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override;
};

// Creates |encoder_config.number_of_streams| VideoStreams where index
// |encoder_config.number_of_streams -1| have width = |width|, height =
// |height|. The total max bitrate of all VideoStreams is
// |encoder_config.max_bitrate_bps|.
std::vector<VideoStream> CreateVideoStreams(
    int width,
    int height,
    const webrtc::VideoEncoderConfig& encoder_config);

void FillEncoderConfiguration(size_t num_streams,
                              VideoEncoderConfig* configuration);

VideoReceiveStream::Decoder CreateMatchingDecoder(
    const VideoSendStream::Config::EncoderSettings& encoder_settings);
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_ENCODER_SETTINGS_H_
