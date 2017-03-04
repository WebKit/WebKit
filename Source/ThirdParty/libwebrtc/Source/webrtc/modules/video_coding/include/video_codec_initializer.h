/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INITIALIZER_H_
#define WEBRTC_MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INITIALIZER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/video_send_stream.h"

namespace webrtc {

class TemporalLayersFactory;
class VideoBitrateAllocator;
class VideoCodec;
class VideoEncoderConfig;

class VideoCodecInitializer {
 public:
  // Takes an EncoderSettings, a VideoEncoderConfig and the VideoStream
  // configuration and translated them into the old school VideoCodec type.
  // It also creates a VideoBitrateAllocator instance, suitable for the codec
  // type used. For instance, VP8 will create an allocator than can handle
  // simulcast and temporal layering.
  // GetBitrateAllocator is called implicitly from here, no need to call again.
  static bool SetupCodec(
      const VideoEncoderConfig& config,
      const VideoSendStream::Config::EncoderSettings settings,
      const std::vector<VideoStream>& streams,
      bool nack_enabled,
      VideoCodec* codec,
      std::unique_ptr<VideoBitrateAllocator>* bitrate_allocator);

  // Create a bitrate allocator for the specified codec. |tl_factory| is
  // optional, if it is populated, ownership of that instance will be
  // transferred to the VideoBitrateAllocator instance.
  static std::unique_ptr<VideoBitrateAllocator> CreateBitrateAllocator(
      const VideoCodec& codec,
      std::unique_ptr<TemporalLayersFactory> tl_factory);

 private:
  static VideoCodec VideoEncoderConfigToVideoCodec(
      const VideoEncoderConfig& config,
      const std::vector<VideoStream>& streams,
      const std::string& payload_name,
      int payload_type,
      bool nack_enabled);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INITIALIZER_H_
