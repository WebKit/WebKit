/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_DECODER_H_
#define API_TEST_MOCK_VIDEO_DECODER_H_

#include "api/video_codecs/video_decoder.h"
#include "test/gmock.h"

namespace webrtc {

class MockDecodedImageCallback : public DecodedImageCallback {
 public:
  MockDecodedImageCallback();
  ~MockDecodedImageCallback() override;

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
  MockVideoDecoder();
  ~MockVideoDecoder() override;

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

#endif  // API_TEST_MOCK_VIDEO_DECODER_H_
