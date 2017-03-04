/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

static const int kEncodeTimeoutMs = 100;
static const int kDecodeTimeoutMs = 25;
// Set a start bitrate to get higher quality.
static const int kStartBitrate = 300;
static const int kWidth = 172;        // Width of the input image.
static const int kHeight = 144;       // Height of the input image.
static const int kMaxFramerate = 30;  // Arbitrary value.

class TestVp9Impl : public ::testing::Test {
 public:
  TestVp9Impl()
      : encode_complete_callback_(this),
        decode_complete_callback_(this),
        encoded_frame_event_(false /* manual reset */,
                             false /* initially signaled */),
        decoded_frame_event_(false /* manual reset */,
                             false /* initially signaled */) {}

 protected:
  class FakeEncodeCompleteCallback : public webrtc::EncodedImageCallback {
   public:
    explicit FakeEncodeCompleteCallback(TestVp9Impl* test) : test_(test) {}

    Result OnEncodedImage(const EncodedImage& frame,
                          const CodecSpecificInfo* codec_specific_info,
                          const RTPFragmentationHeader* fragmentation) {
      rtc::CritScope lock(&test_->encoded_frame_section_);
      test_->encoded_frame_ = rtc::Optional<EncodedImage>(frame);
      test_->encoded_frame_event_.Set();
      return Result(Result::OK);
    }

   private:
    TestVp9Impl* const test_;
  };

  class FakeDecodeCompleteCallback : public webrtc::DecodedImageCallback {
   public:
    explicit FakeDecodeCompleteCallback(TestVp9Impl* test) : test_(test) {}

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
                 rtc::Optional<uint8_t> qp) override {
      rtc::CritScope lock(&test_->decoded_frame_section_);
      test_->decoded_frame_ = rtc::Optional<VideoFrame>(frame);
      test_->decoded_qp_ = qp;
      test_->decoded_frame_event_.Set();
    }

   private:
    TestVp9Impl* const test_;
  };

  void SetUp() override {
    // Using a QCIF image. Processing only one frame.
    FILE* source_file_ =
        fopen(test::ResourcePath("paris_qcif", "yuv").c_str(), "rb");
    ASSERT_TRUE(source_file_ != NULL);
    rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer(
        test::ReadI420Buffer(kWidth, kHeight, source_file_));
    input_frame_.reset(new VideoFrame(video_frame_buffer, kVideoRotation_0, 0));
    fclose(source_file_);

    encoder_.reset(VP9Encoder::Create());
    decoder_.reset(VP9Decoder::Create());
    encoder_->RegisterEncodeCompleteCallback(&encode_complete_callback_);
    decoder_->RegisterDecodeCompleteCallback(&decode_complete_callback_);

    InitCodecs();
  }

  void InitCodecs() {
    VideoCodec codec_inst_;
    codec_inst_.startBitrate = kStartBitrate;
    codec_inst_.codecType = webrtc::kVideoCodecVP9;
    codec_inst_.maxFramerate = kMaxFramerate;
    codec_inst_.width = kWidth;
    codec_inst_.height = kHeight;
    codec_inst_.VP9()->numberOfTemporalLayers = 1;
    codec_inst_.VP9()->numberOfSpatialLayers = 1;
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_inst_, 1 /* number of cores */,
                                   0 /* max payload size (unused) */));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              decoder_->InitDecode(&codec_inst_, 1 /* number of cores */));
  }

  bool WaitForEncodedFrame(EncodedImage* frame) {
    bool ret = encoded_frame_event_.Wait(kEncodeTimeoutMs);
    EXPECT_TRUE(ret) << "Timed out while waiting for an encoded frame.";
    // This becomes unsafe if there are multiple threads waiting for frames.
    rtc::CritScope lock(&encoded_frame_section_);
    EXPECT_TRUE(encoded_frame_);
    if (encoded_frame_) {
      *frame = std::move(*encoded_frame_);
      encoded_frame_.reset();
      return true;
    } else {
      return false;
    }
  }

  bool WaitForDecodedFrame(std::unique_ptr<VideoFrame>* frame,
                           rtc::Optional<uint8_t>* qp) {
    bool ret = decoded_frame_event_.Wait(kDecodeTimeoutMs);
    EXPECT_TRUE(ret) << "Timed out while waiting for a decoded frame.";
    // This becomes unsafe if there are multiple threads waiting for frames.
    rtc::CritScope lock(&decoded_frame_section_);
    EXPECT_TRUE(decoded_frame_);
    if (decoded_frame_) {
      frame->reset(new VideoFrame(std::move(*decoded_frame_)));
      *qp = decoded_qp_;
      decoded_frame_.reset();
      return true;
    } else {
      return false;
    }
  }

  std::unique_ptr<VideoFrame> input_frame_;

  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;

 private:
  FakeEncodeCompleteCallback encode_complete_callback_;
  FakeDecodeCompleteCallback decode_complete_callback_;

  rtc::Event encoded_frame_event_;
  rtc::CriticalSection encoded_frame_section_;
  rtc::Optional<EncodedImage> encoded_frame_ GUARDED_BY(encoded_frame_section_);

  rtc::Event decoded_frame_event_;
  rtc::CriticalSection decoded_frame_section_;
  rtc::Optional<VideoFrame> decoded_frame_ GUARDED_BY(decoded_frame_section_);
  rtc::Optional<uint8_t> decoded_qp_ GUARDED_BY(decoded_frame_section_);
};

TEST_F(TestVp9Impl, EncodeDecode) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  EXPECT_GT(I420PSNR(input_frame_.get(), decoded_frame.get()), 36);
}

TEST_F(TestVp9Impl, DecodedQpEqualsEncodedQp) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->Encode(*input_frame_, nullptr, nullptr));
  EncodedImage encoded_frame;
  ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame));
  // First frame should be a key frame.
  encoded_frame._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame, false, nullptr));
  std::unique_ptr<VideoFrame> decoded_frame;
  rtc::Optional<uint8_t> decoded_qp;
  ASSERT_TRUE(WaitForDecodedFrame(&decoded_frame, &decoded_qp));
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(decoded_qp);
  EXPECT_EQ(encoded_frame.qp_, *decoded_qp);
}

}  // namespace webrtc
