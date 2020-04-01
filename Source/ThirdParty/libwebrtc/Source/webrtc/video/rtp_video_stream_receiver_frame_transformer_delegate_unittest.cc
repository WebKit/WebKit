/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver_frame_transformer_delegate.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/rtp_video_stream_receiver.h"

namespace webrtc {
namespace {

using ::testing::_;

std::unique_ptr<video_coding::RtpFrameObject> CreateRtpFrameObject() {
  return std::make_unique<video_coding::RtpFrameObject>(
      0, 0, true, 0, 0, 0, 0, 0, VideoSendTiming(), 0, kVideoCodecGeneric,
      kVideoRotation_0, VideoContentType::UNSPECIFIED, RTPVideoHeader(),
      absl::nullopt, RtpPacketInfos(), EncodedImageBuffer::Create(0));
}

class FakeTransport : public Transport {
 public:
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) {
    return true;
  }
  bool SendRtcp(const uint8_t* packet, size_t length) { return true; }
};

class FakeNackSender : public NackSender {
 public:
  void SendNack(const std::vector<uint16_t>& sequence_numbers) {}
  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) {}
};

class FakeOnCompleteFrameCallback
    : public video_coding::OnCompleteFrameCallback {
 public:
  void OnCompleteFrame(
      std::unique_ptr<video_coding::EncodedFrame> frame) override {}
};

class TestRtpVideoStreamReceiverInitializer {
 public:
  TestRtpVideoStreamReceiverInitializer()
      : test_config_(nullptr),
        test_process_thread_(ProcessThread::Create("TestThread")) {
    test_config_.rtp.remote_ssrc = 1111;
    test_config_.rtp.local_ssrc = 2222;
    test_rtp_receive_statistics_ =
        ReceiveStatistics::Create(Clock::GetRealTimeClock());
  }

 protected:
  VideoReceiveStream::Config test_config_;
  FakeTransport fake_transport_;
  FakeNackSender fake_nack_sender_;
  FakeOnCompleteFrameCallback fake_on_complete_frame_callback_;
  std::unique_ptr<ProcessThread> test_process_thread_;
  std::unique_ptr<ReceiveStatistics> test_rtp_receive_statistics_;
};

class TestRtpVideoStreamReceiver : public TestRtpVideoStreamReceiverInitializer,
                                   public RtpVideoStreamReceiver {
 public:
  TestRtpVideoStreamReceiver()
      : TestRtpVideoStreamReceiverInitializer(),
        RtpVideoStreamReceiver(Clock::GetRealTimeClock(),
                               &fake_transport_,
                               nullptr,
                               nullptr,
                               &test_config_,
                               test_rtp_receive_statistics_.get(),
                               nullptr,
                               test_process_thread_.get(),
                               &fake_nack_sender_,
                               nullptr,
                               &fake_on_complete_frame_callback_,
                               nullptr,
                               nullptr) {}
  ~TestRtpVideoStreamReceiver() override = default;

  MOCK_METHOD(void,
              ManageFrame,
              (std::unique_ptr<video_coding::RtpFrameObject> frame),
              (override));
};

class MockFrameTransformer : public FrameTransformerInterface {
 public:
  ~MockFrameTransformer() override = default;
  MOCK_METHOD(void,
              TransformFrame,
              (std::unique_ptr<video_coding::EncodedFrame>,
               std::vector<uint8_t>,
               uint32_t),
              (override));
  MOCK_METHOD(void,
              RegisterTransformedFrameCallback,
              (rtc::scoped_refptr<TransformedFrameCallback>),
              (override));
  MOCK_METHOD(void, UnregisterTransformedFrameCallback, (), (override));
};

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     RegisterTransformedFrameCallbackOnInit) {
  TestRtpVideoStreamReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current()));
  EXPECT_CALL(*frame_transformer, RegisterTransformedFrameCallback);
  delegate->Init();
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     UnregisterTransformedFrameCallbackOnReset) {
  TestRtpVideoStreamReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current()));
  EXPECT_CALL(*frame_transformer, UnregisterTransformedFrameCallback);
  delegate->Reset();
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest, TransformFrame) {
  TestRtpVideoStreamReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current()));
  auto frame = CreateRtpFrameObject();
  EXPECT_CALL(*frame_transformer,
              TransformFrame(_, RtpDescriptorAuthentication(RTPVideoHeader()),
                             /*remote_ssrc*/ 1111));
  delegate->TransformFrame(std::move(frame), /*remote_ssrc*/ 1111);
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     ManageFrameOnTransformedFrame) {
  auto main_thread = rtc::Thread::Create();
  main_thread->Start();
  auto network_thread = rtc::Thread::Create();
  network_thread->Start();

  TestRtpVideoStreamReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  auto delegate = network_thread->Invoke<
      rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate>>(
      RTC_FROM_HERE, [&]() mutable {
        return new rtc::RefCountedObject<
            RtpVideoStreamReceiverFrameTransformerDelegate>(
            &receiver, frame_transformer, network_thread.get());
      });

  auto frame = CreateRtpFrameObject();

  EXPECT_CALL(receiver, ManageFrame)
      .WillOnce([&network_thread](
                    std::unique_ptr<video_coding::RtpFrameObject> frame) {
        EXPECT_TRUE(network_thread->IsCurrent());
      });
  main_thread->Invoke<void>(RTC_FROM_HERE, [&]() mutable {
    delegate->OnTransformedFrame(std::move(frame));
  });
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();

  main_thread->Stop();
  network_thread->Stop();
}

}  // namespace
}  // namespace webrtc
