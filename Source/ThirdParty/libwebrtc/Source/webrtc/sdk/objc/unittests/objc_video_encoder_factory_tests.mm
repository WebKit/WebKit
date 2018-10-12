/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>

#include "sdk/objc/native/src/objc_video_encoder_factory.h"

#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoEncoderFactory.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#include "api/video_codecs/sdp_video_format.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/gunit.h"
#include "sdk/objc/native/src/objc_frame_buffer.h"

id<RTCVideoEncoderFactory> CreateEncoderFactoryReturning(int return_code) {
  id encoderMock = OCMProtocolMock(@protocol(RTCVideoEncoder));
  OCMStub([encoderMock startEncodeWithSettings:[OCMArg any] numberOfCores:1])
      .andReturn(return_code);
  OCMStub([encoderMock encode:[OCMArg any] codecSpecificInfo:[OCMArg any] frameTypes:[OCMArg any]])
      .andReturn(return_code);
  OCMStub([encoderMock releaseEncoder]).andReturn(return_code);
  OCMStub([encoderMock setBitrate:0 framerate:0]).andReturn(return_code);

  id encoderFactoryMock = OCMProtocolMock(@protocol(RTCVideoEncoderFactory));
  RTCVideoCodecInfo *supported = [[RTCVideoCodecInfo alloc] initWithName:@"H264" parameters:nil];
  OCMStub([encoderFactoryMock supportedCodecs]).andReturn(@[ supported ]);
  OCMStub([encoderFactoryMock createEncoder:[OCMArg any]]).andReturn(encoderMock);
  return encoderFactoryMock;
}

id<RTCVideoEncoderFactory> CreateOKEncoderFactory() {
  return CreateEncoderFactoryReturning(WEBRTC_VIDEO_CODEC_OK);
}

id<RTCVideoEncoderFactory> CreateErrorEncoderFactory() {
  return CreateEncoderFactoryReturning(WEBRTC_VIDEO_CODEC_ERROR);
}

std::unique_ptr<webrtc::VideoEncoder> GetObjCEncoder(id<RTCVideoEncoderFactory> factory) {
  webrtc::ObjCVideoEncoderFactory encoder_factory(factory);
  webrtc::SdpVideoFormat format("H264");
  return encoder_factory.CreateVideoEncoder(format);
}

#pragma mark -

TEST(ObjCVideoEncoderFactoryTest, InitEncodeReturnsOKOnSuccess) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateOKEncoderFactory());

  auto* settings = new webrtc::VideoCodec();
  EXPECT_EQ(encoder->InitEncode(settings, 1, 0), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoEncoderFactoryTest, InitEncodeReturnsErrorOnFail) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateErrorEncoderFactory());

  auto* settings = new webrtc::VideoCodec();
  EXPECT_EQ(encoder->InitEncode(settings, 1, 0), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST(ObjCVideoEncoderFactoryTest, EncodeReturnsOKOnSuccess) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateOKEncoderFactory());

  CVPixelBufferRef pixel_buffer;
  CVPixelBufferCreate(kCFAllocatorDefault, 640, 480, kCVPixelFormatType_32ARGB, nil, &pixel_buffer);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      new rtc::RefCountedObject<webrtc::ObjCFrameBuffer>(
          [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer]);
  webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, 0);
  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecH264;
  info.codec_name = "H264";
  std::vector<webrtc::FrameType> frame_types;

  EXPECT_EQ(encoder->Encode(frame, &info, &frame_types), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoEncoderFactoryTest, EncodeReturnsErrorOnFail) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateErrorEncoderFactory());

  CVPixelBufferRef pixel_buffer;
  CVPixelBufferCreate(kCFAllocatorDefault, 640, 480, kCVPixelFormatType_32ARGB, nil, &pixel_buffer);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      new rtc::RefCountedObject<webrtc::ObjCFrameBuffer>(
          [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixel_buffer]);
  webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, 0);
  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecH264;
  info.codec_name = "H264";
  std::vector<webrtc::FrameType> frame_types;

  EXPECT_EQ(encoder->Encode(frame, &info, &frame_types), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST(ObjCVideoEncoderFactoryTest, ReleaseEncodeReturnsOKOnSuccess) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateOKEncoderFactory());

  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoEncoderFactoryTest, ReleaseEncodeReturnsErrorOnFail) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateErrorEncoderFactory());

  EXPECT_EQ(encoder->Release(), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST(ObjCVideoEncoderFactoryTest, SetChannelParametersAlwaysReturnsOK) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateErrorEncoderFactory());

  EXPECT_EQ(encoder->SetChannelParameters(1, 1), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoEncoderFactoryTest, SetRatesReturnsOKOnSuccess) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateOKEncoderFactory());

  EXPECT_EQ(encoder->SetRates(0, 0), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoEncoderFactoryTest, SetRatesReturnsErrorOnFail) {
  std::unique_ptr<webrtc::VideoEncoder> encoder = GetObjCEncoder(CreateErrorEncoderFactory());

  EXPECT_EQ(encoder->SetRates(0, 0), WEBRTC_VIDEO_CODEC_ERROR);
}
