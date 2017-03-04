/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {

class SimulcastRateAllocator;

class VideoEncoderFactory {
 public:
  virtual VideoEncoder* Create() = 0;
  virtual void Destroy(VideoEncoder* encoder) = 0;
  virtual ~VideoEncoderFactory() {}
};

// SimulcastEncoderAdapter implements simulcast support by creating multiple
// webrtc::VideoEncoder instances with the given VideoEncoderFactory.
// All the public interfaces are expected to be called from the same thread,
// e.g the encoder thread.
class SimulcastEncoderAdapter : public VP8Encoder {
 public:
  explicit SimulcastEncoderAdapter(VideoEncoderFactory* factory);
  virtual ~SimulcastEncoderAdapter();

  // Implements VideoEncoder
  int Release() override;
  int InitEncode(const VideoCodec* inst,
                 int number_of_cores,
                 size_t max_payload_size) override;
  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int SetRateAllocation(const BitrateAllocation& bitrate,
                        uint32_t new_framerate) override;

  // Eventual handler for the contained encoders' EncodedImageCallbacks, but
  // called from an internal helper that also knows the correct stream
  // index.
  EncodedImageCallback::Result OnEncodedImage(
      size_t stream_idx,
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation);

  VideoEncoder::ScalingSettings GetScalingSettings() const override;

  bool SupportsNativeHandle() const override;
  const char* ImplementationName() const override;

 private:
  struct StreamInfo {
    StreamInfo()
        : encoder(NULL),
          callback(NULL),
          width(0),
          height(0),
          key_frame_request(false),
          send_stream(true) {}
    StreamInfo(VideoEncoder* encoder,
               EncodedImageCallback* callback,
               uint16_t width,
               uint16_t height,
               bool send_stream)
        : encoder(encoder),
          callback(callback),
          width(width),
          height(height),
          key_frame_request(false),
          send_stream(send_stream) {}
    // Deleted by SimulcastEncoderAdapter::Release().
    VideoEncoder* encoder;
    EncodedImageCallback* callback;
    uint16_t width;
    uint16_t height;
    bool key_frame_request;
    bool send_stream;
  };

  // Populate the codec settings for each stream.
  void PopulateStreamCodec(const webrtc::VideoCodec* inst,
                           int stream_index,
                           uint32_t start_bitrate_kbps,
                           bool highest_resolution_stream,
                           webrtc::VideoCodec* stream_codec);

  bool Initialized() const;

  std::unique_ptr<VideoEncoderFactory> factory_;
  VideoCodec codec_;
  std::vector<StreamInfo> streaminfos_;
  EncodedImageCallback* encoded_complete_callback_;
  std::string implementation_name_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SIMULCAST_ENCODER_ADAPTER_H_
