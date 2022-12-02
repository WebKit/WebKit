/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_video_encoder_factory.h"

#include <string>

#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoEncoderFactory.h"
#import "components/video_codec/RTCCodecSpecificInfoH264+Private.h"
#ifndef DISABLE_H265
#import "components/video_codec/RTCCodecSpecificInfoH265+Private.h"
#endif
#import "sdk/objc/api/peerconnection/RTCEncodedImage+Private.h"
#import "sdk/objc/api/peerconnection/RTCVideoCodecInfo+Private.h"
#import "sdk/objc/api/peerconnection/RTCVideoEncoderSettings+Private.h"
#import "sdk/objc/api/video_codec/RTCVideoCodecConstants.h"
#import "sdk/objc/api/video_codec/RTCWrappedNativeVideoEncoder.h"
#import "sdk/objc/helpers/NSString+StdString.h"

#include "api/video/video_frame.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/logging.h"
#include "sdk/objc/native/src/objc_video_frame.h"

namespace webrtc {

namespace {

class ObjCVideoEncoder : public VideoEncoder {
 public:
  ObjCVideoEncoder(id<RTCVideoEncoder> encoder)
      : encoder_(encoder), implementation_name_([encoder implementationName].rtcStdString) {}

  int32_t InitEncode(const VideoCodec *codec_settings, const Settings &encoder_settings) override {
#if defined(WEBRTC_WEBKIT_BUILD)
    int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*codec_settings);
    if (number_of_streams > 1) {
      return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
    }
#endif
    RTCVideoEncoderSettings *settings =
        [[RTCVideoEncoderSettings alloc] initWithNativeVideoCodec:codec_settings];
    return [encoder_ startEncodeWithSettings:settings
                               numberOfCores:encoder_settings.number_of_cores];
  }

  int32_t RegisterEncodeCompleteCallback(EncodedImageCallback *callback) override {
    [encoder_ setCallback:^BOOL(RTCEncodedImage *_Nonnull frame,
                                id<RTCCodecSpecificInfo> _Nonnull info,
                                RTCRtpFragmentationHeader *_Nonnull header) {
      if (!callback)
        return WEBRTC_VIDEO_CODEC_OK;
      EncodedImage encodedImage = [frame nativeEncodedImage];

      // Handle types that can be converted into one of CodecSpecificInfo's hard coded cases.
      CodecSpecificInfo codecSpecificInfo;
      // Because of symbol conflict, isKindOfClass doesn't work as expected.
      // See https://bugs.webkit.org/show_bug.cgi?id=198782.
      if ([NSStringFromClass([info class]) isEqual:@"WK_RTCCodecSpecificInfoH264"]) {
        // if ([info isKindOfClass:[RTCCodecSpecificInfoH264 class]]) {
        codecSpecificInfo = [(RTCCodecSpecificInfoH264 *)info nativeCodecSpecificInfo];
#ifndef DISABLE_H265
      } else if ([NSStringFromClass([info class]) isEqual:@"WK_RTCCodecSpecificInfoH265"]) {
        // if ([info isKindOfClass:[RTCCodecSpecificInfoH265 class]]) {
        codecSpecificInfo = [(RTCCodecSpecificInfoH265 *)info nativeCodecSpecificInfo];
#endif
      }

      EncodedImageCallback::Result res =
          callback->OnEncodedImage(encodedImage, &codecSpecificInfo);
      return res.error == EncodedImageCallback::Result::OK;
    }];

    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return [encoder_ releaseEncoder]; }

  int32_t Encode(const VideoFrame &frame,
                 const std::vector<VideoFrameType> *frame_types) override {
    NSMutableArray<NSNumber *> *rtcFrameTypes = [NSMutableArray array];
    for (size_t i = 0; i < frame_types->size(); ++i) {
      [rtcFrameTypes addObject:@(RTCFrameType(frame_types->at(i)))];
    }

    return [encoder_ encode:ToObjCVideoFrame(frame)
          codecSpecificInfo:nil
                 frameTypes:rtcFrameTypes];
  }

  void SetRates(const RateControlParameters &parameters) override {
    const uint32_t bitrate = parameters.bitrate.get_sum_kbps();
    const uint32_t framerate = static_cast<uint32_t>(parameters.framerate_fps + 0.5);
    [encoder_ setBitrate:bitrate framerate:framerate];
  }

  VideoEncoder::EncoderInfo GetEncoderInfo() const override {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = implementation_name_;

    RTCVideoEncoderQpThresholds *qp_thresholds = [encoder_ scalingSettings];
    info.scaling_settings = qp_thresholds ? ScalingSettings(qp_thresholds.low, qp_thresholds.high) :
                                            ScalingSettings::kOff;

    info.is_hardware_accelerated = true;
    return info;
  }

 private:
  id<RTCVideoEncoder> encoder_;
  const std::string implementation_name_;
};
}  // namespace

ObjCVideoEncoderFactory::ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory> encoder_factory)
    : encoder_factory_(encoder_factory) {}

ObjCVideoEncoderFactory::~ObjCVideoEncoderFactory() {}

id<RTCVideoEncoderFactory> ObjCVideoEncoderFactory::wrapped_encoder_factory() const {
  return encoder_factory_;
}

std::vector<SdpVideoFormat> ObjCVideoEncoderFactory::GetSupportedFormats() const {
  std::vector<SdpVideoFormat> supported_formats;
  for (RTCVideoCodecInfo *supportedCodec in [encoder_factory_ supportedCodecs]) {
    SdpVideoFormat format = [supportedCodec nativeSdpVideoFormat];
    supported_formats.push_back(format);
  }

  return supported_formats;
}

std::vector<SdpVideoFormat> ObjCVideoEncoderFactory::GetImplementations() const {
  if ([encoder_factory_ respondsToSelector:SEL("implementations")]) {
    std::vector<SdpVideoFormat> supported_formats;
    for (RTCVideoCodecInfo *supportedCodec in [encoder_factory_ implementations]) {
      SdpVideoFormat format = [supportedCodec nativeSdpVideoFormat];
      supported_formats.push_back(format);
    }
    return supported_formats;
  }
  return GetSupportedFormats();
}

std::unique_ptr<VideoEncoder> ObjCVideoEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat &format) {
  RTCVideoCodecInfo *info = [[RTCVideoCodecInfo alloc] initWithNativeSdpVideoFormat:format];
  id<RTCVideoEncoder> encoder = [encoder_factory_ createEncoder:info];
  // Because of symbol conflict, isKindOfClass doesn't work as expected.
  // See https://bugs.webkit.org/show_bug.cgi?id=198782.
  // if ([encoder isKindOfClass:[RTCWrappedNativeVideoEncoder class]]) {
  if ([info.name isEqual:@"VP8"] || [info.name isEqual:@"VP9"] || [info.name isEqual:@"AV1"]) {
    return [(RTCWrappedNativeVideoEncoder *)encoder releaseWrappedEncoder];
  } else {
    return std::unique_ptr<ObjCVideoEncoder>(new ObjCVideoEncoder(encoder));
  }
}

}  // namespace webrtc
