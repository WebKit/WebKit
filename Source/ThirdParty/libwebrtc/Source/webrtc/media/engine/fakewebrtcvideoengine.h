/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
#define MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

class FakeWebRtcVideoDecoderFactory;
class FakeWebRtcVideoEncoderFactory;

// Fake class for mocking out webrtc::VideoDecoder
class FakeWebRtcVideoDecoder : public webrtc::VideoDecoder {
 public:
  explicit FakeWebRtcVideoDecoder(FakeWebRtcVideoDecoderFactory* factory);
  ~FakeWebRtcVideoDecoder();

  int32_t InitDecode(const webrtc::VideoCodec*, int32_t) override;
  int32_t Decode(const webrtc::EncodedImage&,
                 bool,
                 const webrtc::CodecSpecificInfo*,
                 int64_t) override;
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback*) override;
  int32_t Release() override;

  int GetNumFramesReceived() const;

 private:
  int num_frames_received_;
  FakeWebRtcVideoDecoderFactory* factory_;
};

// Fake class for mocking out webrtc::VideoDecoderFactory.
class FakeWebRtcVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  FakeWebRtcVideoDecoderFactory();

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;

  void DecoderDestroyed(FakeWebRtcVideoDecoder* decoder);
  void AddSupportedVideoCodecType(const webrtc::SdpVideoFormat& format);
  int GetNumCreatedDecoders();
  const std::vector<FakeWebRtcVideoDecoder*>& decoders();

 private:
  std::vector<webrtc::SdpVideoFormat> supported_codec_formats_;
  std::vector<FakeWebRtcVideoDecoder*> decoders_;
  int num_created_decoders_;
};

// Fake class for mocking out webrtc::VideoEnoder
class FakeWebRtcVideoEncoder : public webrtc::VideoEncoder {
 public:
  explicit FakeWebRtcVideoEncoder(FakeWebRtcVideoEncoderFactory* factory);
  ~FakeWebRtcVideoEncoder();

  int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                     int32_t numberOfCores,
                     size_t maxPayloadSize) override;
  int32_t Encode(const webrtc::VideoFrame& inputImage,
                 const webrtc::CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<webrtc::FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetRateAllocation(const webrtc::VideoBitrateAllocation& allocation,
                            uint32_t framerate) override;

  bool WaitForInitEncode();
  webrtc::VideoCodec GetCodecSettings();
  int GetNumEncodedFrames();

 private:
  rtc::CriticalSection crit_;
  rtc::Event init_encode_event_;
  int num_frames_encoded_ RTC_GUARDED_BY(crit_);
  webrtc::VideoCodec codec_settings_ RTC_GUARDED_BY(crit_);
  FakeWebRtcVideoEncoderFactory* factory_;
};

// Fake class for mocking out webrtc::VideoEncoderFactory.
class FakeWebRtcVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  FakeWebRtcVideoEncoderFactory();

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override;

  bool WaitForCreatedVideoEncoders(int num_encoders);
  void EncoderDestroyed(FakeWebRtcVideoEncoder* encoder);
  void set_encoders_have_internal_sources(bool internal_source);
  void AddSupportedVideoCodec(const webrtc::SdpVideoFormat& format);
  void AddSupportedVideoCodecType(const std::string& name);
  int GetNumCreatedEncoders();
  const std::vector<FakeWebRtcVideoEncoder*> encoders();

 private:
  rtc::CriticalSection crit_;
  rtc::Event created_video_encoder_event_;
  std::vector<webrtc::SdpVideoFormat> formats_;
  std::vector<FakeWebRtcVideoEncoder*> encoders_ RTC_GUARDED_BY(crit_);
  int num_created_encoders_ RTC_GUARDED_BY(crit_);
  bool encoders_have_internal_sources_;
  bool vp8_factory_mode_;
};

}  // namespace cricket

#endif  // MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
