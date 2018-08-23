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

#ifndef MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_
#define MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_

#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {

class SimulcastRateAllocator;
class VideoEncoderFactory;

// SimulcastEncoderAdapter implements simulcast support by creating multiple
// webrtc::VideoEncoder instances with the given VideoEncoderFactory.
// The object is created and destroyed on the worker thread, but all public
// interfaces should be called from the encoder task queue.
class SimulcastEncoderAdapter : public VideoEncoder {
 public:
  explicit SimulcastEncoderAdapter(VideoEncoderFactory* factory,
                                   const SdpVideoFormat& format);
  virtual ~SimulcastEncoderAdapter();

  // Implements VideoEncoder.
  int Release() override;
  int InitEncode(const VideoCodec* inst,
                 int number_of_cores,
                 size_t max_payload_size) override;
  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int SetRateAllocation(const VideoBitrateAllocation& bitrate,
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
    StreamInfo(std::unique_ptr<VideoEncoder> encoder,
               std::unique_ptr<EncodedImageCallback> callback,
               uint16_t width,
               uint16_t height,
               bool send_stream)
        : encoder(std::move(encoder)),
          callback(std::move(callback)),
          width(width),
          height(height),
          key_frame_request(false),
          send_stream(send_stream) {}
    std::unique_ptr<VideoEncoder> encoder;
    std::unique_ptr<EncodedImageCallback> callback;
    uint16_t width;
    uint16_t height;
    bool key_frame_request;
    bool send_stream;
  };

  // Populate the codec settings for each simulcast stream.
  static void PopulateStreamCodec(const webrtc::VideoCodec& inst,
                                  int stream_index,
                                  uint32_t start_bitrate_kbps,
                                  bool highest_resolution_stream,
                                  webrtc::VideoCodec* stream_codec);

  bool Initialized() const;

  void DestroyStoredEncoders();

  volatile int inited_;  // Accessed atomically.
  VideoEncoderFactory* const factory_;
  const SdpVideoFormat video_format_;
  VideoCodec codec_;
  std::vector<StreamInfo> streaminfos_;
  EncodedImageCallback* encoded_complete_callback_;
  std::string implementation_name_;

  // Used for checking the single-threaded access of the encoder interface.
  rtc::SequencedTaskChecker encoder_queue_;

  // Store encoders in between calls to Release and InitEncode, so they don't
  // have to be recreated. Remaining encoders are destroyed by the destructor.
  std::stack<std::unique_ptr<VideoEncoder>> stored_encoders_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_SIMULCAST_ENCODER_ADAPTER_H_
