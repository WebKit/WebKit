/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/loopback_media_transport.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "test/gmock.h"

namespace webrtc {

namespace {

class MockMediaTransportAudioSinkInterface
    : public MediaTransportAudioSinkInterface {
 public:
  MOCK_METHOD2(OnData, void(uint64_t, MediaTransportEncodedAudioFrame));
};

class MockMediaTransportVideoSinkInterface
    : public MediaTransportVideoSinkInterface {
 public:
  MOCK_METHOD2(OnData, void(uint64_t, MediaTransportEncodedVideoFrame));
};

class MockMediaTransportKeyFrameRequestCallback
    : public MediaTransportKeyFrameRequestCallback {
 public:
  MOCK_METHOD1(OnKeyFrameRequested, void(uint64_t));
};

class MockDataChannelSink : public DataChannelSink {
 public:
  MOCK_METHOD3(OnDataReceived,
               void(int, DataMessageType, const rtc::CopyOnWriteBuffer&));
  MOCK_METHOD1(OnChannelClosing, void(int));
  MOCK_METHOD1(OnChannelClosed, void(int));
  MOCK_METHOD0(OnReadyToSend, void());
};

class MockStateCallback : public MediaTransportStateCallback {
 public:
  MOCK_METHOD1(OnStateChanged, void(MediaTransportState));
};

}  // namespace

TEST(LoopbackMediaTransport, DataDeliveredToSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink sink;
  transport_pair.first_datagram_transport()->SetDataSink(&sink);

  const int channel_id = 1;
  EXPECT_CALL(
      sink, OnDataReceived(
                channel_id, DataMessageType::kText,
                ::testing::Property<rtc::CopyOnWriteBuffer, const char*>(
                    &rtc::CopyOnWriteBuffer::cdata, ::testing::StrEq("foo"))));

  SendDataParams params;
  params.type = DataMessageType::kText;
  rtc::CopyOnWriteBuffer buffer("foo");
  transport_pair.second_datagram_transport()->SendData(channel_id, params,
                                                       buffer);

  transport_pair.FlushAsyncInvokes();
  transport_pair.first_datagram_transport()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, CloseDeliveredToSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink first_sink;
  transport_pair.first_datagram_transport()->SetDataSink(&first_sink);

  MockDataChannelSink second_sink;
  transport_pair.second_datagram_transport()->SetDataSink(&second_sink);

  const int channel_id = 1;
  {
    ::testing::InSequence s;
    EXPECT_CALL(second_sink, OnChannelClosing(channel_id));
    EXPECT_CALL(second_sink, OnChannelClosed(channel_id));
    EXPECT_CALL(first_sink, OnChannelClosed(channel_id));
  }

  transport_pair.first_datagram_transport()->CloseChannel(channel_id);

  transport_pair.FlushAsyncInvokes();
  transport_pair.first_datagram_transport()->SetDataSink(nullptr);
  transport_pair.second_datagram_transport()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, InitialStateDeliveredWhenCallbackSet) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockStateCallback state_callback;
  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kPending));

  thread->Invoke<void>(RTC_FROM_HERE, [&transport_pair, &state_callback] {
    transport_pair.first_datagram_transport()->SetTransportStateCallback(
        &state_callback);
  });
  transport_pair.FlushAsyncInvokes();
}

TEST(LoopbackMediaTransport, ChangedStateDeliveredWhenCallbackSet) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  transport_pair.SetState(MediaTransportState::kWritable);
  transport_pair.FlushAsyncInvokes();

  MockStateCallback state_callback;

  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kWritable));
  thread->Invoke<void>(RTC_FROM_HERE, [&transport_pair, &state_callback] {
    transport_pair.first_datagram_transport()->SetTransportStateCallback(
        &state_callback);
  });
  transport_pair.FlushAsyncInvokes();
}

TEST(LoopbackMediaTransport, StateChangeDeliveredToCallback) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockStateCallback state_callback;

  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kPending));
  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kWritable));
  thread->Invoke<void>(RTC_FROM_HERE, [&transport_pair, &state_callback] {
    transport_pair.first_datagram_transport()->SetTransportStateCallback(
        &state_callback);
  });
  transport_pair.SetState(MediaTransportState::kWritable);
  transport_pair.FlushAsyncInvokes();
}

TEST(LoopbackMediaTransport, NotReadyToSendWhenDataSinkSet) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink data_channel_sink;
  EXPECT_CALL(data_channel_sink, OnReadyToSend()).Times(0);

  transport_pair.first_datagram_transport()->SetDataSink(&data_channel_sink);
  transport_pair.FlushAsyncInvokes();
  transport_pair.first_datagram_transport()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, ReadyToSendWhenDataSinkSet) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  transport_pair.SetState(MediaTransportState::kWritable);
  transport_pair.FlushAsyncInvokes();

  MockDataChannelSink data_channel_sink;
  EXPECT_CALL(data_channel_sink, OnReadyToSend());

  transport_pair.first_datagram_transport()->SetDataSink(&data_channel_sink);
  transport_pair.FlushAsyncInvokes();
  transport_pair.first_datagram_transport()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, StateChangeDeliveredToDataSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink data_channel_sink;
  EXPECT_CALL(data_channel_sink, OnReadyToSend());

  transport_pair.first_datagram_transport()->SetDataSink(&data_channel_sink);
  transport_pair.SetState(MediaTransportState::kWritable);
  transport_pair.FlushAsyncInvokes();
  transport_pair.first_datagram_transport()->SetDataSink(nullptr);
}

}  // namespace webrtc
