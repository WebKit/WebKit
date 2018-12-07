/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_transceiver.h"

#include <memory>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace {

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::_;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiver;
using ::webrtc::RtcpTransceiverConfig;
using ::webrtc::rtcp::TransportFeedback;

class MockMediaReceiverRtcpObserver : public webrtc::MediaReceiverRtcpObserver {
 public:
  MOCK_METHOD3(OnSenderReport, void(uint32_t, webrtc::NtpTime, uint32_t));
};

constexpr int kTimeoutMs = 1000;

void WaitPostedTasks(rtc::TaskQueue* queue) {
  rtc::Event done;
  queue->PostTask([&done] { done.Set(); });
  ASSERT_TRUE(done.Wait(kTimeoutMs));
}

TEST(RtcpTransceiverTest, SendsRtcpOnTaskQueueWhenCreatedOffTaskQueue) {
  MockTransport outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(InvokeWithoutArgs([&] {
        EXPECT_TRUE(queue.IsCurrent());
        return true;
      }));

  RtcpTransceiver rtcp_transceiver(config);
  rtcp_transceiver.SendCompoundPacket();
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, SendsRtcpOnTaskQueueWhenCreatedOnTaskQueue) {
  MockTransport outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(InvokeWithoutArgs([&] {
        EXPECT_TRUE(queue.IsCurrent());
        return true;
      }));

  std::unique_ptr<RtcpTransceiver> rtcp_transceiver;
  queue.PostTask([&] {
    rtcp_transceiver = absl::make_unique<RtcpTransceiver>(config);
    rtcp_transceiver->SendCompoundPacket();
  });
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, CanBeDestroyedOnTaskQueue) {
  NiceMock<MockTransport> outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  auto rtcp_transceiver = absl::make_unique<RtcpTransceiver>(config);

  queue.PostTask([&] {
    // Insert a packet just before destruction to test for races.
    rtcp_transceiver->SendCompoundPacket();
    rtcp_transceiver.reset();
  });
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, CanBeDestroyedWithoutBlocking) {
  rtc::TaskQueue queue("rtcp");
  NiceMock<MockTransport> outgoing_transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  auto* rtcp_transceiver = new RtcpTransceiver(config);
  rtcp_transceiver->SendCompoundPacket();

  rtc::Event done;
  rtc::Event heavy_task;
  queue.PostTask([&] {
    EXPECT_TRUE(heavy_task.Wait(kTimeoutMs));
    done.Set();
  });
  delete rtcp_transceiver;

  heavy_task.Set();
  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(RtcpTransceiverTest, MaySendPacketsAfterDestructor) {  // i.e. Be careful!
  NiceMock<MockTransport> outgoing_transport;  // Must outlive queue below.
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  auto* rtcp_transceiver = new RtcpTransceiver(config);

  rtc::Event heavy_task;
  queue.PostTask([&] { EXPECT_TRUE(heavy_task.Wait(kTimeoutMs)); });
  rtcp_transceiver->SendCompoundPacket();
  delete rtcp_transceiver;

  EXPECT_CALL(outgoing_transport, SendRtcp);
  heavy_task.Set();

  WaitPostedTasks(&queue);
}

// Use rtp timestamp to distinguish different incoming sender reports.
rtc::CopyOnWriteBuffer CreateSenderReport(uint32_t ssrc, uint32_t rtp_time) {
  webrtc::rtcp::SenderReport sr;
  sr.SetSenderSsrc(ssrc);
  sr.SetRtpTimestamp(rtp_time);
  rtc::Buffer buffer = sr.Build();
  // Switch to an efficient way creating CopyOnWriteBuffer from RtcpPacket when
  // there is one. Until then do not worry about extra memcpy in test.
  return rtc::CopyOnWriteBuffer(buffer.data(), buffer.size());
}

TEST(RtcpTransceiverTest, DoesntPostToRtcpObserverAfterCallToRemove) {
  const uint32_t kRemoteSsrc = 1234;
  MockTransport null_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &null_transport;
  config.task_queue = &queue;
  RtcpTransceiver rtcp_transceiver(config);
  rtc::Event observer_deleted;

  auto observer = absl::make_unique<MockMediaReceiverRtcpObserver>();
  EXPECT_CALL(*observer, OnSenderReport(kRemoteSsrc, _, 1));
  EXPECT_CALL(*observer, OnSenderReport(kRemoteSsrc, _, 2)).Times(0);

  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, observer.get());
  rtcp_transceiver.ReceivePacket(CreateSenderReport(kRemoteSsrc, 1));
  rtcp_transceiver.RemoveMediaReceiverRtcpObserver(kRemoteSsrc, observer.get(),
                                                   /*on_removed=*/[&] {
                                                     observer.reset();
                                                     observer_deleted.Set();
                                                   });
  rtcp_transceiver.ReceivePacket(CreateSenderReport(kRemoteSsrc, 2));

  EXPECT_TRUE(observer_deleted.Wait(kTimeoutMs));
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, RemoveMediaReceiverRtcpObserverIsNonBlocking) {
  const uint32_t kRemoteSsrc = 1234;
  MockTransport null_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &null_transport;
  config.task_queue = &queue;
  RtcpTransceiver rtcp_transceiver(config);
  auto observer = absl::make_unique<MockMediaReceiverRtcpObserver>();
  rtcp_transceiver.AddMediaReceiverRtcpObserver(kRemoteSsrc, observer.get());

  rtc::Event queue_blocker;
  rtc::Event observer_deleted;
  queue.PostTask([&] { EXPECT_TRUE(queue_blocker.Wait(kTimeoutMs)); });
  rtcp_transceiver.RemoveMediaReceiverRtcpObserver(kRemoteSsrc, observer.get(),
                                                   /*on_removed=*/[&] {
                                                     observer.reset();
                                                     observer_deleted.Set();
                                                   });

  EXPECT_THAT(observer, Not(IsNull()));
  queue_blocker.Set();
  EXPECT_TRUE(observer_deleted.Wait(kTimeoutMs));
}

TEST(RtcpTransceiverTest, CanCallSendCompoundPacketFromAnyThread) {
  MockTransport outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      // If test is slow, a periodic task may send an extra packet.
      .Times(AtLeast(3))
      .WillRepeatedly(InvokeWithoutArgs([&] {
        EXPECT_TRUE(queue.IsCurrent());
        return true;
      }));

  RtcpTransceiver rtcp_transceiver(config);

  // Call from the construction thread.
  rtcp_transceiver.SendCompoundPacket();
  // Call from the same queue transceiver use for processing.
  queue.PostTask([&] { rtcp_transceiver.SendCompoundPacket(); });
  // Call from unrelated task queue.
  rtc::TaskQueue queue_send("send_packet");
  queue_send.PostTask([&] { rtcp_transceiver.SendCompoundPacket(); });

  WaitPostedTasks(&queue_send);
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, DoesntSendPacketsAfterStopCallback) {
  NiceMock<MockTransport> outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  config.schedule_periodic_compound_packets = true;

  auto rtcp_transceiver = absl::make_unique<RtcpTransceiver>(config);
  rtc::Event done;
  rtcp_transceiver->SendCompoundPacket();
  rtcp_transceiver->Stop([&] {
    EXPECT_CALL(outgoing_transport, SendRtcp).Times(0);
    done.Set();
  });
  rtcp_transceiver = nullptr;
  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(RtcpTransceiverTest, SendsTransportFeedbackOnTaskQueue) {
  static constexpr uint32_t kSenderSsrc = 12345;
  MockTransport outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.feedback_ssrc = kSenderSsrc;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  config.schedule_periodic_compound_packets = false;
  RtcpTransceiver rtcp_transceiver(config);

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillOnce(Invoke([&](const uint8_t* buffer, size_t size) {
        EXPECT_TRUE(queue.IsCurrent());

        std::unique_ptr<TransportFeedback> transport_feedback =
            TransportFeedback::ParseFrom(buffer, size);
        EXPECT_TRUE(transport_feedback);
        EXPECT_EQ(transport_feedback->sender_ssrc(), kSenderSsrc);
        return true;
      }));

  // Create minimalistic transport feedback packet.
  TransportFeedback transport_feedback;
  transport_feedback.SetSenderSsrc(rtcp_transceiver.SSRC());
  transport_feedback.AddReceivedPacket(321, 10000);

  EXPECT_TRUE(rtcp_transceiver.SendFeedbackPacket(transport_feedback));

  WaitPostedTasks(&queue);
}

}  // namespace
