/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/Framework/Classes/VideoToolbox/objc_video_decoder_factory.h"

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "RTCWrappedNativeVideoDecoder.h"
#import "WebRTC/RTCVideoCodec.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#import "WebRTC/RTCVideoCodecH264.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"

namespace webrtc {

namespace {
class ObjCVideoDecoder : public VideoDecoder {
 public:
  ObjCVideoDecoder(id<RTCVideoDecoder> decoder)
      : decoder_(decoder), implementation_name_([decoder implementationName].stdString) {}

  int32_t InitDecode(const VideoCodec *codec_settings, int32_t number_of_cores) {
    RTCVideoEncoderSettings *settings =
        [[RTCVideoEncoderSettings alloc] initWithNativeVideoCodec:codec_settings];
    return [decoder_ startDecodeWithSettings:settings numberOfCores:number_of_cores];
  }

  int32_t Decode(const EncodedImage &input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader *fragmentation,
                 const CodecSpecificInfo *codec_specific_info = NULL,
                 int64_t render_time_ms = -1) {
    RTCEncodedImage *encodedImage =
        [[RTCEncodedImage alloc] initWithNativeEncodedImage:input_image];
    RTCRtpFragmentationHeader *header =
        [[RTCRtpFragmentationHeader alloc] initWithNativeFragmentationHeader:fragmentation];

    // webrtc::CodecSpecificInfo only handles a hard coded list of codecs
    id<RTCCodecSpecificInfo> rtcCodecSpecificInfo = nil;
    if (codec_specific_info) {
      if (codec_specific_info->codecType == kVideoCodecH264) {
        RTCCodecSpecificInfoH264 *h264Info = [[RTCCodecSpecificInfoH264 alloc] init];
        h264Info.packetizationMode =
            (RTCH264PacketizationMode)codec_specific_info->codecSpecific.H264.packetization_mode;
        rtcCodecSpecificInfo = h264Info;
      }
    }

    return [decoder_ decode:encodedImage
              missingFrames:missing_frames
        fragmentationHeader:header
          codecSpecificInfo:rtcCodecSpecificInfo
               renderTimeMs:render_time_ms];
  }

  int32_t RegisterDecodeCompleteCallback(DecodedImageCallback *callback) {
    [decoder_ setCallback:^(RTCVideoFrame *frame) {
      const rtc::scoped_refptr<VideoFrameBuffer> buffer =
          new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
      VideoFrame videoFrame(buffer,
                            (uint32_t)(frame.timeStampNs / rtc::kNumNanosecsPerMicrosec),
                            0,
                            (VideoRotation)frame.rotation);
      videoFrame.set_timestamp(frame.timeStamp);

      callback->Decoded(videoFrame);
    }];

    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() { return [decoder_ releaseDecoder]; }

  const char *ImplementationName() const { return implementation_name_.c_str(); }

 private:
  id<RTCVideoDecoder> decoder_;
  const std::string implementation_name_;
};
}  // namespace

ObjCVideoDecoderFactory::ObjCVideoDecoderFactory(id<RTCVideoDecoderFactory> decoder_factory)
    : decoder_factory_(decoder_factory) {}

ObjCVideoDecoderFactory::~ObjCVideoDecoderFactory() {}

id<RTCVideoDecoderFactory> ObjCVideoDecoderFactory::wrapped_decoder_factory() const {
  return decoder_factory_;
}

std::unique_ptr<VideoDecoder> ObjCVideoDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat &format) {
  NSString *codecName = [NSString stringWithUTF8String:format.name.c_str()];
  for (RTCVideoCodecInfo *codecInfo in decoder_factory_.supportedCodecs) {
    if ([codecName isEqualToString:codecInfo.name]) {
      id<RTCVideoDecoder> decoder = [decoder_factory_ createDecoder:codecInfo];

      if ([decoder isKindOfClass:[RTCWrappedNativeVideoDecoder class]]) {
        return [(RTCWrappedNativeVideoDecoder *)decoder releaseWrappedDecoder];
      } else {
        return std::unique_ptr<ObjCVideoDecoder>(new ObjCVideoDecoder(decoder));
      }
    }
  }

  return nullptr;
}

std::vector<SdpVideoFormat> ObjCVideoDecoderFactory::GetSupportedFormats() const {
  std::vector<SdpVideoFormat> supported_formats;
  for (RTCVideoCodecInfo *supportedCodec in decoder_factory_.supportedCodecs) {
    SdpVideoFormat format = [supportedCodec nativeSdpVideoFormat];
    supported_formats.push_back(format);
  }

  return supported_formats;
}

// WebRtcVideoDecoderFactory

VideoDecoder *ObjCVideoDecoderFactory::CreateVideoDecoderWithParams(
    const cricket::VideoCodec &codec, cricket::VideoDecoderParams params) {
  return CreateVideoDecoder(SdpVideoFormat(codec.name, codec.params)).release();
}

VideoDecoder *ObjCVideoDecoderFactory::CreateVideoDecoder(VideoCodecType type) {
  // This is implemented to avoid hiding an overloaded virtual function
  RTC_NOTREACHED();
  return nullptr;
}

void ObjCVideoDecoderFactory::DestroyVideoDecoder(VideoDecoder *decoder) {
  delete decoder;
  decoder = nullptr;
}

}  // namespace webrtc
