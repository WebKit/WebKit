/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_
#define MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_

#include <string>
#include <vector>

#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockEncodedImageCallback : public EncodedImageCallback {
 public:
  MOCK_METHOD3(OnEncodedImage,
               Result(const EncodedImage& encodedImage,
                      const CodecSpecificInfo* codecSpecificInfo,
                      const RTPFragmentationHeader* fragmentation));
};

class MockVideoEncoder : public VideoEncoder {
 public:
  MOCK_CONST_METHOD2(Version, int32_t(int8_t* version, int32_t length));
  MOCK_METHOD3(InitEncode,
               int32_t(const VideoCodec* codecSettings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize));
  MOCK_METHOD3(Encode,
               int32_t(const VideoFrame& inputImage,
                       const CodecSpecificInfo* codecSpecificInfo,
                       const std::vector<FrameType>* frame_types));
  MOCK_METHOD1(RegisterEncodeCompleteCallback,
               int32_t(EncodedImageCallback* callback));
  MOCK_METHOD0(Release, int32_t());
  MOCK_METHOD0(Reset, int32_t());
  MOCK_METHOD2(SetChannelParameters, int32_t(uint32_t packetLoss, int64_t rtt));
  MOCK_METHOD2(SetRates, int32_t(uint32_t newBitRate, uint32_t frameRate));
  MOCK_METHOD2(SetRateAllocation,
               int32_t(const VideoBitrateAllocation& newBitRate,
                       uint32_t frameRate));
};

class MockDecodedImageCallback : public DecodedImageCallback {
 public:
  MOCK_METHOD1(Decoded, int32_t(VideoFrame& decodedImage));  // NOLINT
  MOCK_METHOD2(Decoded,
               int32_t(VideoFrame& decodedImage,  // NOLINT
                       int64_t decode_time_ms));
  MOCK_METHOD3(Decoded,
               void(VideoFrame& decodedImage,  // NOLINT
                    absl::optional<int32_t> decode_time_ms,
                    absl::optional<uint8_t> qp));
  MOCK_METHOD1(ReceivedDecodedReferenceFrame,
               int32_t(const uint64_t pictureId));
  MOCK_METHOD1(ReceivedDecodedFrame, int32_t(const uint64_t pictureId));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD2(InitDecode,
               int32_t(const VideoCodec* codecSettings, int32_t numberOfCores));
  MOCK_METHOD4(Decode,
               int32_t(const EncodedImage& inputImage,
                       bool missingFrames,
                       const CodecSpecificInfo* codecSpecificInfo,
                       int64_t renderTimeMs));
  MOCK_METHOD1(RegisterDecodeCompleteCallback,
               int32_t(DecodedImageCallback* callback));
  MOCK_METHOD0(Release, int32_t());
  MOCK_METHOD0(Copy, VideoDecoder*());
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VIDEO_CODEC_INTERFACE_H_
