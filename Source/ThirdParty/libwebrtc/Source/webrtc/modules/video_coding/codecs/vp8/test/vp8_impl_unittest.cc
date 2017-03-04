/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <memory>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace {
void Calc16ByteAlignedStride(int width, int* stride_y, int* stride_uv) {
  *stride_y = 16 * ((width + 15) / 16);
  *stride_uv = 16 * ((width + 31) / 32);
}

}  // Anonymous namespace

enum { kMaxWaitEncTimeMs = 100 };
enum { kMaxWaitDecTimeMs = 25 };

static const uint32_t kTestTimestamp = 123;
static const int64_t kTestNtpTimeMs = 456;

// TODO(mikhal): Replace these with mocks.
class Vp8UnitTestEncodeCompleteCallback : public webrtc::EncodedImageCallback {
 public:
  Vp8UnitTestEncodeCompleteCallback(EncodedImage* frame,
                                    unsigned int decoderSpecificSize,
                                    void* decoderSpecificInfo)
      : encoded_frame_(frame), encode_complete_(false) {}

  Result OnEncodedImage(const EncodedImage& encoded_frame_,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override;
  bool EncodeComplete();

 private:
  EncodedImage* const encoded_frame_;
  std::unique_ptr<uint8_t[]> frame_buffer_;
  bool encode_complete_;
};

webrtc::EncodedImageCallback::Result
Vp8UnitTestEncodeCompleteCallback::OnEncodedImage(
    const EncodedImage& encoded_frame,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  if (encoded_frame_->_size < encoded_frame._length) {
    delete[] encoded_frame_->_buffer;
    frame_buffer_.reset(new uint8_t[encoded_frame._length]);
    encoded_frame_->_buffer = frame_buffer_.get();
    encoded_frame_->_size = encoded_frame._length;
  }
  memcpy(encoded_frame_->_buffer, encoded_frame._buffer, encoded_frame._length);
  encoded_frame_->_length = encoded_frame._length;
  encoded_frame_->_encodedWidth = encoded_frame._encodedWidth;
  encoded_frame_->_encodedHeight = encoded_frame._encodedHeight;
  encoded_frame_->_timeStamp = encoded_frame._timeStamp;
  encoded_frame_->_frameType = encoded_frame._frameType;
  encoded_frame_->_completeFrame = encoded_frame._completeFrame;
  encoded_frame_->qp_ = encoded_frame.qp_;
  encode_complete_ = true;
  return Result(Result::OK, 0);
}

bool Vp8UnitTestEncodeCompleteCallback::EncodeComplete() {
  if (encode_complete_) {
    encode_complete_ = false;
    return true;
  }
  return false;
}

class Vp8UnitTestDecodeCompleteCallback : public webrtc::DecodedImageCallback {
 public:
  explicit Vp8UnitTestDecodeCompleteCallback(rtc::Optional<VideoFrame>* frame,
                                             rtc::Optional<uint8_t>* qp)
      : decoded_frame_(frame), decoded_qp_(qp), decode_complete(false) {}
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
  bool DecodeComplete();

 private:
  rtc::Optional<VideoFrame>* decoded_frame_;
  rtc::Optional<uint8_t>* decoded_qp_;
  bool decode_complete;
};

bool Vp8UnitTestDecodeCompleteCallback::DecodeComplete() {
  if (decode_complete) {
    decode_complete = false;
    return true;
  }
  return false;
}

void Vp8UnitTestDecodeCompleteCallback::Decoded(
    VideoFrame& frame,
    rtc::Optional<int32_t> decode_time_ms,
    rtc::Optional<uint8_t> qp) {
  *decoded_frame_ = rtc::Optional<VideoFrame>(frame);
  *decoded_qp_ = qp;
  decode_complete = true;
}

class TestVp8Impl : public ::testing::Test {
 protected:
  virtual void SetUp() {
    encoder_.reset(VP8Encoder::Create());
    decoder_.reset(VP8Decoder::Create());
    memset(&codec_inst_, 0, sizeof(codec_inst_));
    encode_complete_callback_.reset(
        new Vp8UnitTestEncodeCompleteCallback(&encoded_frame_, 0, NULL));
    decode_complete_callback_.reset(
        new Vp8UnitTestDecodeCompleteCallback(&decoded_frame_, &decoded_qp_));
    encoder_->RegisterEncodeCompleteCallback(encode_complete_callback_.get());
    decoder_->RegisterDecodeCompleteCallback(decode_complete_callback_.get());
    // Using a QCIF image (aligned stride (u,v planes) > width).
    // Processing only one frame.
    source_file_ = fopen(test::ResourcePath("paris_qcif", "yuv").c_str(), "rb");
    ASSERT_TRUE(source_file_ != NULL);
    rtc::scoped_refptr<VideoFrameBuffer> compact_buffer(
        test::ReadI420Buffer(kWidth, kHeight, source_file_));
    ASSERT_TRUE(compact_buffer);
    codec_inst_.width = kWidth;
    codec_inst_.height = kHeight;
    const int kFramerate = 30;
    codec_inst_.maxFramerate = kFramerate;
    // Setting aligned stride values.
    int stride_uv;
    int stride_y;
    Calc16ByteAlignedStride(codec_inst_.width, &stride_y, &stride_uv);
    EXPECT_EQ(stride_y, 176);
    EXPECT_EQ(stride_uv, 96);

    rtc::scoped_refptr<I420Buffer> stride_buffer(
        I420Buffer::Create(kWidth, kHeight, stride_y, stride_uv, stride_uv));

    // No scaling in our case, just a copy, to add stride to the image.
    stride_buffer->ScaleFrom(*compact_buffer);

    input_frame_.reset(
        new VideoFrame(stride_buffer, kVideoRotation_0, 0));
    input_frame_->set_timestamp(kTestTimestamp);
  }

  void SetUpEncodeDecode() {
    codec_inst_.startBitrate = 300;
    codec_inst_.maxBitrate = 4000;
    codec_inst_.qpMax = 56;
    codec_inst_.VP8()->denoisingOn = true;
    codec_inst_.VP8()->tl_factory = &tl_factory_;
    codec_inst_.VP8()->numberOfTemporalLayers = 1;

    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->InitEncode(&codec_inst_, 1, 1440));
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->InitDecode(&codec_inst_, 1));
  }

  size_t WaitForEncodedFrame() const {
    int64_t startTime = rtc::TimeMillis();
    while (rtc::TimeMillis() - startTime < kMaxWaitEncTimeMs) {
      if (encode_complete_callback_->EncodeComplete()) {
        return encoded_frame_._length;
      }
    }
    return 0;
  }

  size_t WaitForDecodedFrame() const {
    int64_t startTime = rtc::TimeMillis();
    while (rtc::TimeMillis() - startTime < kMaxWaitDecTimeMs) {
      if (decode_complete_callback_->DecodeComplete()) {
        return CalcBufferSize(kI420, decoded_frame_->width(),
                              decoded_frame_->height());
      }
    }
    return 0;
  }

  const int kWidth = 172;
  const int kHeight = 144;

  std::unique_ptr<Vp8UnitTestEncodeCompleteCallback> encode_complete_callback_;
  std::unique_ptr<Vp8UnitTestDecodeCompleteCallback> decode_complete_callback_;
  std::unique_ptr<uint8_t[]> source_buffer_;
  FILE* source_file_;
  std::unique_ptr<VideoFrame> input_frame_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;
  EncodedImage encoded_frame_;
  rtc::Optional<VideoFrame> decoded_frame_;
  rtc::Optional<uint8_t> decoded_qp_;
  VideoCodec codec_inst_;
  TemporalLayersFactory tl_factory_;
};

TEST_F(TestVp8Impl, EncoderParameterTest) {
  strncpy(codec_inst_.plName, "VP8", 31);
  codec_inst_.plType = 126;
  codec_inst_.maxBitrate = 0;
  codec_inst_.minBitrate = 0;
  codec_inst_.width = 1440;
  codec_inst_.height = 1080;
  codec_inst_.maxFramerate = 30;
  codec_inst_.startBitrate = 300;
  codec_inst_.qpMax = 56;
  codec_inst_.VP8()->complexity = kComplexityNormal;
  codec_inst_.VP8()->numberOfTemporalLayers = 1;
  codec_inst_.VP8()->tl_factory = &tl_factory_;
  // Calls before InitEncode().
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
  int bit_rate = 300;
  BitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, bit_rate * 1000);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_UNINITIALIZED,
            encoder_->SetRateAllocation(bitrate_allocation,
                                        codec_inst_.maxFramerate));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->InitEncode(&codec_inst_, 1, 1440));

  // Decoder parameter tests.
  // Calls before InitDecode().
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->InitDecode(&codec_inst_, 1));
}

TEST_F(TestVp8Impl, DecodedQpEqualsEncodedQp) {
  SetUpEncodeDecode();
  encoder_->Encode(*input_frame_, nullptr, nullptr);
  EXPECT_GT(WaitForEncodedFrame(), 0u);
  // First frame should be a key frame.
  encoded_frame_._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame_, false, nullptr));
  EXPECT_GT(WaitForDecodedFrame(), 0u);
  ASSERT_TRUE(decoded_frame_);
  EXPECT_GT(I420PSNR(input_frame_.get(), &*decoded_frame_), 36);
  ASSERT_TRUE(decoded_qp_);
  EXPECT_EQ(encoded_frame_.qp_, *decoded_qp_);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AlignedStrideEncodeDecode DISABLED_AlignedStrideEncodeDecode
#else
#define MAYBE_AlignedStrideEncodeDecode AlignedStrideEncodeDecode
#endif
TEST_F(TestVp8Impl, MAYBE_AlignedStrideEncodeDecode) {
  SetUpEncodeDecode();
  encoder_->Encode(*input_frame_, NULL, NULL);
  EXPECT_GT(WaitForEncodedFrame(), 0u);
  // First frame should be a key frame.
  encoded_frame_._frameType = kVideoFrameKey;
  encoded_frame_.ntp_time_ms_ = kTestNtpTimeMs;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame_, false, NULL));
  EXPECT_GT(WaitForDecodedFrame(), 0u);
  ASSERT_TRUE(decoded_frame_);
  // Compute PSNR on all planes (faster than SSIM).
  EXPECT_GT(I420PSNR(input_frame_.get(), &*decoded_frame_), 36);
  EXPECT_EQ(kTestTimestamp, decoded_frame_->timestamp());
  EXPECT_EQ(kTestNtpTimeMs, decoded_frame_->ntp_time_ms());
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_DecodeWithACompleteKeyFrame DISABLED_DecodeWithACompleteKeyFrame
#else
#define MAYBE_DecodeWithACompleteKeyFrame DecodeWithACompleteKeyFrame
#endif
TEST_F(TestVp8Impl, MAYBE_DecodeWithACompleteKeyFrame) {
  SetUpEncodeDecode();
  encoder_->Encode(*input_frame_, NULL, NULL);
  EXPECT_GT(WaitForEncodedFrame(), 0u);
  // Setting complete to false -> should return an error.
  encoded_frame_._completeFrame = false;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            decoder_->Decode(encoded_frame_, false, NULL));
  // Setting complete back to true. Forcing a delta frame.
  encoded_frame_._frameType = kVideoFrameDelta;
  encoded_frame_._completeFrame = true;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_ERROR,
            decoder_->Decode(encoded_frame_, false, NULL));
  // Now setting a key frame.
  encoded_frame_._frameType = kVideoFrameKey;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->Decode(encoded_frame_, false, NULL));
  ASSERT_TRUE(decoded_frame_);
  EXPECT_GT(I420PSNR(input_frame_.get(), &*decoded_frame_), 36);
}

}  // namespace webrtc
