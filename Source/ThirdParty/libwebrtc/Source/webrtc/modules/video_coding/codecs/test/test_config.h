/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_

#include <string>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/test/packet_manipulator.h"

namespace webrtc {
namespace test {

// Defines which frame types shall be excluded from packet loss and when.
enum ExcludeFrameTypes {
  // Will exclude the first keyframe in the video sequence from packet loss.
  // Following keyframes will be targeted for packet loss.
  kExcludeOnlyFirstKeyFrame,
  // Exclude all keyframes from packet loss, no matter where in the video
  // sequence they occur.
  kExcludeAllKeyFrames
};

// Test configuration for a test run.
struct TestConfig {
  class EncodedFrameChecker {
   public:
    virtual ~EncodedFrameChecker() = default;

    virtual void CheckEncodedFrame(webrtc::VideoCodecType codec,
                                   const EncodedImage& encoded_frame) const = 0;
  };

  void SetCodecSettings(VideoCodecType codec_type,
                        int num_temporal_layers,
                        bool error_concealment_on,
                        bool denoising_on,
                        bool frame_dropper_on,
                        bool spatial_resize_on,
                        bool resilience_on,
                        int width,
                        int height);

  int NumberOfCores() const;
  int NumberOfTemporalLayers() const;
  int TemporalLayerForFrame(int frame_idx) const;
  std::vector<FrameType> FrameTypeForFrame(int frame_idx) const;
  std::string ToString() const;
  std::string CodecName() const;
  std::string FilenameWithParams() const;

  // Plain name of YUV file to process without file extension.
  std::string filename;

  // File to process. This must be a video file in the YUV format.
  std::string input_filename;

  // File to write to during processing for the test. Will be a video file in
  // the YUV format.
  std::string output_filename;

  // Number of frames to process.
  int num_frames = 0;

  // Configurations related to networking.
  NetworkingConfig networking_config;

  // Decides how the packet loss simulations shall exclude certain frames from
  // packet loss.
  ExcludeFrameTypes exclude_frame_types = kExcludeOnlyFirstKeyFrame;

  // Force the encoder and decoder to use a single core for processing.
  // Using a single core is necessary to get a deterministic behavior for the
  // encoded frames - using multiple cores will produce different encoded frames
  // since multiple cores are competing to consume the byte budget for each
  // frame in parallel.
  // If set to false, the maximum number of available cores will be used.
  bool use_single_core = false;

  // Should cpu usage be measured?
  // If set to true, the encoding will run in real-time.
  bool measure_cpu = false;

  // If > 0: forces the encoder to create a keyframe every Nth frame.
  // Note that the encoder may create a keyframe in other locations in addition
  // to this setting. Forcing key frames may also affect encoder planning
  // optimizations in a negative way, since it will suddenly be forced to
  // produce an expensive key frame.
  int keyframe_interval = 0;

  // Codec settings to use.
  webrtc::VideoCodec codec_settings;

  // H.264 specific settings.
  struct H264CodecSettings {
    H264::Profile profile = H264::kProfileConstrainedBaseline;
    H264PacketizationMode packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;
  } h264_codec_settings;

  // Should hardware accelerated codecs be used?
  bool hw_encoder = false;
  bool hw_decoder = false;

  // Should the hardware codecs be wrapped in software fallbacks?
  bool sw_fallback_encoder = false;
  bool sw_fallback_decoder = false;

  // Custom checker that will be called for each frame.
  const EncodedFrameChecker* encoded_frame_checker = nullptr;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_TEST_CONFIG_H_
