/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *  Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#pragma once

#ifdef __APPLE__
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>

#if (defined(TARGET_OS_IPHONE)  && TARGET_OS_IPHONE) || (defined(TARGET_IPHONE_SIMULATOR)  && TARGET_IPHONE_SIMULATOR)
// FIXME: Activate VCP for iOS/iOS simulator
#define ENABLE_VCP_ENCODER 0
#elif (defined(TARGET_OS_MAC)  && TARGET_OS_MAC)
#define ENABLE_VCP_ENCODER (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101304)
#endif

#if ENABLE_VCP_ENCODER

#include "webrtc/api/video/video_rotation.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/common_video/include/bitrate_adjuster.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"
#include "webrtc/rtc_base/criticalsection.h"

#include <VideoProcessing/VideoProcessing.h>
#include <vector>

// This file provides a H264 encoder implementation using the VideoProcessing APIs.

namespace webrtc {

class H264VideoToolboxEncoderVCP : public H264Encoder {
 public:
  explicit H264VideoToolboxEncoderVCP(const webrtc::SdpVideoFormat& format, VideoToolboxVideoEncoderFactory&);

  ~H264VideoToolboxEncoderVCP() override;
  void ClearFactory() { factory_ = nullptr; }

  int InitEncode(const VideoCodec* codec_settings,
                 int number_of_cores,
                 size_t max_payload_size) override;

  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;

  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

  int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate) override;

  int Release() override;

  const char* ImplementationName() const override;

  bool SupportsNativeHandle() const override;

  void OnEncodedFrame(OSStatus status,
                      VTEncodeInfoFlags info_flags,
                      CMSampleBufferRef sample_buffer,
                      CodecSpecificInfo codec_specific_info,
                      int32_t width,
                      int32_t height,
                      int64_t render_time_ms,
                      uint32_t timestamp,
                      VideoRotation rotation);

  ScalingSettings GetScalingSettings() const override;

  void SetActive(bool is_active) { is_active_ = is_active; }

 protected:
  virtual int CreateCompressionSession(VCPCompressionSessionRef&, VTCompressionOutputCallback, int32_t width, int32_t height, bool useHardwareEncoder = true);
  void DestroyCompressionSession();

 private:
  int ResetCompressionSession();
  void ConfigureCompressionSession();
  rtc::scoped_refptr<VideoFrameBuffer> GetScaledBufferOnEncode(
      const rtc::scoped_refptr<VideoFrameBuffer>& frame);
  void SetBitrateBps(uint32_t bitrate_bps);
  void SetEncoderBitrateBps(uint32_t bitrate_bps);

  EncodedImageCallback* callback_;
  VCPCompressionSessionRef compression_session_;
  BitrateAdjuster bitrate_adjuster_;
  H264PacketizationMode packetization_mode_;
  uint32_t target_bitrate_bps_;
  uint32_t encoder_bitrate_bps_;
  int32_t width_;
  int32_t height_;
  VideoCodecMode mode_;
  const CFStringRef profile_;

  H264BitstreamParser h264_bitstream_parser_;
  std::vector<uint8_t> nv12_scale_buffer_;
  bool is_active_ { true };
  VideoToolboxVideoEncoderFactory* factory_;
};  // H264VideoToolboxEncoder

H264VideoToolboxEncoderVCP* createH264VideoToolboxEncoderVCP(const cricket::VideoCodec&);
void deleteH264VideoToolboxEncoderVCP(H264VideoToolboxEncoderVCP*);

}  // namespace webrtc
#else

namespace cricket {
struct VideoCodec;
}

namespace webrtc {
class H264VideoToolboxEncoderVCP;
H264VideoToolboxEncoderVCP* createH264VideoToolboxEncoderVCP(const cricket::VideoCodec&);
void deleteH264VideoToolboxEncoderVCP(H264VideoToolboxEncoderVCP*);
}

#endif // ENABLE_VCP_ENCODER

#endif // __APPLE__
