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

#include "sdk/objc/Framework/Classes/VideoToolbox/objc_video_decoder_factory.h"

#import "WebRTC/RTCVideoCodec.h"
#import "WebRTC/RTCVideoCodecFactory.h"
#include "media/base/codec.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/gunit.h"

id<RTCVideoDecoderFactory> CreateDecoderFactoryReturning(int return_code) {
  id decoderMock = OCMProtocolMock(@protocol(RTCVideoDecoder));
  OCMStub([decoderMock startDecodeWithSettings:[OCMArg any] numberOfCores:1])
      .andReturn(return_code);
  OCMStub([decoderMock decode:[OCMArg any]
                    missingFrames:NO
              fragmentationHeader:[OCMArg any]
                codecSpecificInfo:[OCMArg any]
                     renderTimeMs:0])
      .andReturn(return_code);
  OCMStub([decoderMock releaseDecoder]).andReturn(return_code);

  id decoderFactoryMock = OCMProtocolMock(@protocol(RTCVideoDecoderFactory));
  RTCVideoCodecInfo *supported = [[RTCVideoCodecInfo alloc] initWithName:@"H264" parameters:nil];
  OCMStub([decoderFactoryMock supportedCodecs]).andReturn(@[ supported ]);
  OCMStub([decoderFactoryMock createDecoder:[OCMArg any]]).andReturn(decoderMock);
  return decoderFactoryMock;
}

id<RTCVideoDecoderFactory> CreateOKDecoderFactory() {
  return CreateDecoderFactoryReturning(WEBRTC_VIDEO_CODEC_OK);
}

id<RTCVideoDecoderFactory> CreateErrorDecoderFactory() {
  return CreateDecoderFactoryReturning(WEBRTC_VIDEO_CODEC_ERROR);
}

webrtc::VideoDecoder *GetObjCDecoder(id<RTCVideoDecoderFactory> factory) {
  webrtc::ObjCVideoDecoderFactory decoder_factory(factory);
  return decoder_factory.CreateVideoDecoderWithParams(cricket::VideoCodec(cricket::kH264CodecName),
                                                      {});
}

#pragma mark -

TEST(ObjCVideoDecoderFactoryTest, InitDecodeReturnsOKOnSuccess) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateOKDecoderFactory());

  auto settings = new webrtc::VideoCodec();
  EXPECT_EQ(decoder->InitDecode(settings, 1), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoDecoderFactoryTest, InitDecodeReturnsErrorOnFail) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateErrorDecoderFactory());

  auto settings = new webrtc::VideoCodec();
  EXPECT_EQ(decoder->InitDecode(settings, 1), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST(ObjCVideoDecoderFactoryTest, DecodeReturnsOKOnSuccess) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateOKDecoderFactory());

  webrtc::EncodedImage encoded_image;
  webrtc::RTPFragmentationHeader header;
  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecH264;

  EXPECT_EQ(decoder->Decode(encoded_image, false, &header, &info, 0), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoDecoderFactoryTest, DecodeReturnsErrorOnFail) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateErrorDecoderFactory());

  webrtc::EncodedImage encoded_image;
  webrtc::RTPFragmentationHeader header;
  webrtc::CodecSpecificInfo info;
  info.codecType = webrtc::kVideoCodecH264;

  EXPECT_EQ(decoder->Decode(encoded_image, false, &header, &info, 0), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST(ObjCVideoDecoderFactoryTest, ReleaseDecodeReturnsOKOnSuccess) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateOKDecoderFactory());

  EXPECT_EQ(decoder->Release(), WEBRTC_VIDEO_CODEC_OK);
}

TEST(ObjCVideoDecoderFactoryTest, ReleaseDecodeReturnsErrorOnFail) {
  webrtc::VideoDecoder *decoder = GetObjCDecoder(CreateErrorDecoderFactory());

  EXPECT_EQ(decoder->Release(), WEBRTC_VIDEO_CODEC_ERROR);
}
