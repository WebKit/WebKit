/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/buffered_frame_decryptor.h"

#include <map>
#include <memory>
#include <vector>

#include "api/test/mock_frame_decryptor.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/ref_counted_object.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Return;

namespace webrtc {
namespace {

FrameDecryptorInterface::Result DecryptSuccess() {
  return FrameDecryptorInterface::Result(FrameDecryptorInterface::Status::kOk,
                                         0);
}

FrameDecryptorInterface::Result DecryptFail() {
  return FrameDecryptorInterface::Result(
      FrameDecryptorInterface::Status::kFailedToDecrypt, 0);
}

}  // namespace

class BufferedFrameDecryptorTest : public ::testing::Test,
                                   public OnDecryptedFrameCallback,
                                   public OnDecryptionStatusChangeCallback {
 public:
  // Implements the OnDecryptedFrameCallbackInterface
  void OnDecryptedFrame(std::unique_ptr<RtpFrameObject> frame) override {
    decrypted_frame_call_count_++;
  }

  void OnDecryptionStatusChange(FrameDecryptorInterface::Status status) {
    ++decryption_status_change_count_;
  }

  // Returns a new fake RtpFrameObject it abstracts the difficult construction
  // of the RtpFrameObject to simplify testing.
  std::unique_ptr<RtpFrameObject> CreateRtpFrameObject(bool key_frame) {
    seq_num_++;
    RTPVideoHeader rtp_video_header;
    rtp_video_header.generic.emplace();

    // clang-format off
    return std::make_unique<RtpFrameObject>(
        seq_num_,
        seq_num_,
        /*markerBit=*/true,
        /*times_nacked=*/0,
        /*first_packet_received_time=*/0,
        /*last_packet_received_time=*/0,
        /*rtp_timestamp=*/0,
        /*ntp_time_ms=*/0,
        VideoSendTiming(),
        /*payload_type=*/0,
        kVideoCodecGeneric,
        kVideoRotation_0,
        VideoContentType::UNSPECIFIED,
        rtp_video_header,
        /*color_space=*/absl::nullopt,
        RtpPacketInfos(),
        EncodedImageBuffer::Create(/*size=*/0));
    // clang-format on
  }

 protected:
  BufferedFrameDecryptorTest() {
    fake_packet_data_ = std::vector<uint8_t>(100);
    decrypted_frame_call_count_ = 0;
    decryption_status_change_count_ = 0;
    seq_num_ = 0;
    mock_frame_decryptor_ = rtc::make_ref_counted<MockFrameDecryptor>();
    buffered_frame_decryptor_ =
        std::make_unique<BufferedFrameDecryptor>(this, this);
    buffered_frame_decryptor_->SetFrameDecryptor(mock_frame_decryptor_.get());
  }

  static const size_t kMaxStashedFrames;

  std::vector<uint8_t> fake_packet_data_;
  rtc::scoped_refptr<MockFrameDecryptor> mock_frame_decryptor_;
  std::unique_ptr<BufferedFrameDecryptor> buffered_frame_decryptor_;
  size_t decrypted_frame_call_count_;
  size_t decryption_status_change_count_ = 0;
  uint16_t seq_num_;
};

const size_t BufferedFrameDecryptorTest::kMaxStashedFrames = 24;

// Callback should always be triggered on a successful decryption.
TEST_F(BufferedFrameDecryptorTest, CallbackCalledOnSuccessfulDecryption) {
  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(1)
      .WillOnce(Return(DecryptSuccess()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .Times(1)
      .WillOnce(Return(0));
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(1));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(1));
}

// An initial fail to decrypt should not trigger the callback.
TEST_F(BufferedFrameDecryptorTest, CallbackNotCalledOnFailedDecryption) {
  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(1)
      .WillOnce(Return(DecryptFail()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .Times(1)
      .WillOnce(Return(0));
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(0));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(1));
}

// Initial failures should be stored and retried after the first successful
// decryption.
TEST_F(BufferedFrameDecryptorTest, DelayedCallbackOnBufferedFrames) {
  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(3)
      .WillOnce(Return(DecryptFail()))
      .WillOnce(Return(DecryptSuccess()))
      .WillOnce(Return(DecryptSuccess()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .Times(3)
      .WillRepeatedly(Return(0));

  // The first decrypt will fail stashing the first frame.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(0));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(1));
  // The second call will succeed playing back both frames.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(false));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(2));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(2));
}

// Subsequent failure to decrypts after the first successful decryption should
// fail to decryptk
TEST_F(BufferedFrameDecryptorTest, FTDDiscardedAfterFirstSuccess) {
  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(4)
      .WillOnce(Return(DecryptFail()))
      .WillOnce(Return(DecryptSuccess()))
      .WillOnce(Return(DecryptSuccess()))
      .WillOnce(Return(DecryptFail()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .Times(4)
      .WillRepeatedly(Return(0));

  // The first decrypt will fail stashing the first frame.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(0));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(1));
  // The second call will succeed playing back both frames.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(false));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(2));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(2));
  // A new failure call will not result in an additional decrypted frame
  // callback.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(2));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(3));
}

// Validate that the maximum number of stashed frames cannot be exceeded even if
// more than its maximum arrives before the first successful decryption.
TEST_F(BufferedFrameDecryptorTest, MaximumNumberOfFramesStored) {
  const size_t failed_to_decrypt_count = kMaxStashedFrames * 2;
  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(failed_to_decrypt_count)
      .WillRepeatedly(Return(DecryptFail()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .WillRepeatedly(Return(0));

  for (size_t i = 0; i < failed_to_decrypt_count; ++i) {
    buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  }
  EXPECT_EQ(decrypted_frame_call_count_, static_cast<size_t>(0));
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(1));

  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(kMaxStashedFrames + 1)
      .WillRepeatedly(Return(DecryptSuccess()));
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, kMaxStashedFrames + 1);
  EXPECT_EQ(decryption_status_change_count_, static_cast<size_t>(2));
}

// Verifies if a BufferedFrameDecryptor is attached but has no FrameDecryptor
// attached it will still store frames up to the frame max.
TEST_F(BufferedFrameDecryptorTest, FramesStoredIfDecryptorNull) {
  buffered_frame_decryptor_->SetFrameDecryptor(nullptr);
  for (size_t i = 0; i < (2 * kMaxStashedFrames); ++i) {
    buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  }

  EXPECT_CALL(*mock_frame_decryptor_, Decrypt)
      .Times(kMaxStashedFrames + 1)
      .WillRepeatedly(Return(DecryptSuccess()));
  EXPECT_CALL(*mock_frame_decryptor_, GetMaxPlaintextByteSize)
      .WillRepeatedly(Return(0));

  // Attach the frame decryptor at a later point after frames have arrived.
  buffered_frame_decryptor_->SetFrameDecryptor(mock_frame_decryptor_.get());

  // Next frame should trigger kMaxStashedFrame decryptions.
  buffered_frame_decryptor_->ManageEncryptedFrame(CreateRtpFrameObject(true));
  EXPECT_EQ(decrypted_frame_call_count_, kMaxStashedFrames + 1);
}

}  // namespace webrtc
