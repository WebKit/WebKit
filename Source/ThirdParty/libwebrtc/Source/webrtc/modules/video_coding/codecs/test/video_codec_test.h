/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TEST_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TEST_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/thread_annotations.h"
#include "test/gtest.h"

namespace webrtc {

class VideoCodecTest : public ::testing::Test {
 public:
  VideoCodecTest()
      : encode_complete_callback_(this),
        decode_complete_callback_(this),
        encoded_frame_event_(false /* manual reset */,
                             false /* initially signaled */),
        wait_for_encoded_frames_threshold_(1),
        decoded_frame_event_(false /* manual reset */,
                             false /* initially signaled */) {}

 protected:
  class FakeEncodeCompleteCallback : public webrtc::EncodedImageCallback {
   public:
    explicit FakeEncodeCompleteCallback(VideoCodecTest* test) : test_(test) {}

    Result OnEncodedImage(const EncodedImage& frame,
                          const CodecSpecificInfo* codec_specific_info,
                          const RTPFragmentationHeader* fragmentation);

   private:
    VideoCodecTest* const test_;
  };

  class FakeDecodeCompleteCallback : public webrtc::DecodedImageCallback {
   public:
    explicit FakeDecodeCompleteCallback(VideoCodecTest* test) : test_(test) {}

    int32_t Decoded(VideoFrame& frame) override {
      RTC_NOTREACHED();
      return -1;
    }
    int32_t Decoded(VideoFrame& frame, int64_t decode_time_ms) override {
      RTC_NOTREACHED();
      return -1;
    }
    void Decoded(VideoFrame& frame,
                 rtc::Optional<int32_t> decode_time_ms,
                 rtc::Optional<uint8_t> qp) override;

   private:
    VideoCodecTest* const test_;
  };

  virtual std::unique_ptr<VideoEncoder> CreateEncoder() = 0;
  virtual std::unique_ptr<VideoDecoder> CreateDecoder() = 0;
  virtual VideoCodec codec_settings() = 0;

  void SetUp() override;

  // Helper method for waiting a single encoded frame.
  bool WaitForEncodedFrame(EncodedImage* frame,
                           CodecSpecificInfo* codec_specific_info);

  // Helper methods for waiting for multiple encoded frames. Caller must
  // define how many frames are to be waited for via |num_frames| before calling
  // Encode(). Then, they can expect to retrive them via WaitForEncodedFrames().
  void SetWaitForEncodedFramesThreshold(size_t num_frames);
  bool WaitForEncodedFrames(
      std::vector<EncodedImage>* frames,
      std::vector<CodecSpecificInfo>* codec_specific_info);

  // Helper method for waiting a single decoded frame.
  bool WaitForDecodedFrame(std::unique_ptr<VideoFrame>* frame,
                           rtc::Optional<uint8_t>* qp);

  // Populated by InitCodecs().
  VideoCodec codec_settings_;

  std::unique_ptr<VideoFrame> input_frame_;

  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;

 private:
  void InitCodecs();

  FakeEncodeCompleteCallback encode_complete_callback_;
  FakeDecodeCompleteCallback decode_complete_callback_;

  rtc::Event encoded_frame_event_;
  rtc::CriticalSection encoded_frame_section_;
  size_t wait_for_encoded_frames_threshold_;
  std::vector<EncodedImage> encoded_frames_
      RTC_GUARDED_BY(encoded_frame_section_);
  std::vector<CodecSpecificInfo> codec_specific_infos_
      RTC_GUARDED_BY(encoded_frame_section_);

  rtc::Event decoded_frame_event_;
  rtc::CriticalSection decoded_frame_section_;
  rtc::Optional<VideoFrame> decoded_frame_
      RTC_GUARDED_BY(decoded_frame_section_);
  rtc::Optional<uint8_t> decoded_qp_ RTC_GUARDED_BY(decoded_frame_section_);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TEST_H_
