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

#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace {

using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::_;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiver;
using ::webrtc::RtcpTransceiverConfig;

void WaitPostedTasks(rtc::TaskQueue* queue) {
  rtc::Event done(false, false);
  queue->PostTask([&done] { done.Set(); });
  ASSERT_TRUE(done.Wait(1000));
}

TEST(RtcpTransceiverTest, SendsRtcpOnTaskQueueWhenCreatedOffTaskQueue) {
  rtc::TaskQueue queue("rtcp");
  MockTransport outgoing_transport;
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
  rtc::TaskQueue queue("rtcp");
  MockTransport outgoing_transport;
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
    rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(config);
    rtcp_transceiver->SendCompoundPacket();
  });
  WaitPostedTasks(&queue);
}

TEST(RtcpTransceiverTest, CanBeDestoryedOnTaskQueue) {
  rtc::TaskQueue queue("rtcp");
  NiceMock<MockTransport> outgoing_transport;
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  auto rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(config);

  queue.PostTask([&] { rtcp_transceiver.reset(); });
  WaitPostedTasks(&queue);
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

TEST(RtcpTransceiverTest, DoesntSendPacketsAfterDestruction) {
  MockTransport outgoing_transport;
  rtc::TaskQueue queue("rtcp");
  RtcpTransceiverConfig config;
  config.outgoing_transport = &outgoing_transport;
  config.task_queue = &queue;
  config.schedule_periodic_compound_packets = false;

  EXPECT_CALL(outgoing_transport, SendRtcp(_, _)).Times(0);

  auto rtcp_transceiver = rtc::MakeUnique<RtcpTransceiver>(config);
  rtc::Event pause(false, false);
  queue.PostTask([&] {
    pause.Wait(rtc::Event::kForever);
    rtcp_transceiver.reset();
  });
  rtcp_transceiver->SendCompoundPacket();
  pause.Set();
  WaitPostedTasks(&queue);
  EXPECT_FALSE(rtcp_transceiver);
}

}  // namespace
