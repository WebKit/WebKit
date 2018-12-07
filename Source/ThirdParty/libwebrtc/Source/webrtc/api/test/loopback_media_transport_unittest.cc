/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/loopback_media_transport.h"
#include "test/gmock.h"

namespace webrtc {

namespace {

class MockMediaTransportAudioSinkInterface
    : public MediaTransportAudioSinkInterface {
 public:
  MOCK_METHOD2(OnData, void(uint64_t, MediaTransportEncodedAudioFrame));
};

class MockDataChannelSink : public DataChannelSink {
 public:
  MOCK_METHOD3(OnDataReceived,
               void(int, DataMessageType, const rtc::CopyOnWriteBuffer&));
  MOCK_METHOD1(OnChannelClosing, void(int));
  MOCK_METHOD1(OnChannelClosed, void(int));
};

class MockStateCallback : public MediaTransportStateCallback {
 public:
  MOCK_METHOD1(OnStateChanged, void(MediaTransportState));
};

// Test only uses the sequence number.
MediaTransportEncodedAudioFrame CreateAudioFrame(int sequence_number) {
  static constexpr int kSamplingRateHz = 48000;
  static constexpr int kStartingSampleIndex = 0;
  static constexpr int kSamplesPerChannel = 480;
  static constexpr uint8_t kPayloadType = 17;

  return MediaTransportEncodedAudioFrame(
      kSamplingRateHz, kStartingSampleIndex, kSamplesPerChannel,
      sequence_number, MediaTransportEncodedAudioFrame::FrameType::kSpeech,
      kPayloadType, std::vector<uint8_t>(kSamplesPerChannel));
}

}  // namespace

TEST(LoopbackMediaTransport, AudioWithNoSinkSilentlyIgnored) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());
  transport_pair.first()->SendAudioFrame(1, CreateAudioFrame(0));
  transport_pair.second()->SendAudioFrame(2, CreateAudioFrame(0));
  transport_pair.FlushAsyncInvokes();
}

TEST(LoopbackMediaTransport, AudioDeliveredToSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());
  testing::StrictMock<MockMediaTransportAudioSinkInterface> sink;
  EXPECT_CALL(sink,
              OnData(1, testing::Property(
                            &MediaTransportEncodedAudioFrame::sequence_number,
                            testing::Eq(10))));
  transport_pair.second()->SetReceiveAudioSink(&sink);
  transport_pair.first()->SendAudioFrame(1, CreateAudioFrame(10));

  transport_pair.FlushAsyncInvokes();
  transport_pair.second()->SetReceiveAudioSink(nullptr);
}

TEST(LoopbackMediaTransport, DataDeliveredToSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink sink;
  transport_pair.first()->SetDataSink(&sink);

  const int channel_id = 1;
  EXPECT_CALL(sink,
              OnDataReceived(
                  channel_id, DataMessageType::kText,
                  testing::Property<rtc::CopyOnWriteBuffer, const char*>(
                      &rtc::CopyOnWriteBuffer::cdata, testing::StrEq("foo"))));

  SendDataParams params;
  params.type = DataMessageType::kText;
  rtc::CopyOnWriteBuffer buffer("foo");
  transport_pair.second()->SendData(channel_id, params, buffer);

  transport_pair.FlushAsyncInvokes();
  transport_pair.first()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, CloseDeliveredToSink) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockDataChannelSink first_sink;
  transport_pair.first()->SetDataSink(&first_sink);

  MockDataChannelSink second_sink;
  transport_pair.second()->SetDataSink(&second_sink);

  const int channel_id = 1;
  {
    testing::InSequence s;
    EXPECT_CALL(second_sink, OnChannelClosing(channel_id));
    EXPECT_CALL(second_sink, OnChannelClosed(channel_id));
    EXPECT_CALL(first_sink, OnChannelClosed(channel_id));
  }

  transport_pair.first()->CloseChannel(channel_id);

  transport_pair.FlushAsyncInvokes();
  transport_pair.first()->SetDataSink(nullptr);
  transport_pair.second()->SetDataSink(nullptr);
}

TEST(LoopbackMediaTransport, InitialStateDeliveredWhenCallbackSet) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockStateCallback state_callback;

  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kPending));
  transport_pair.first()->SetMediaTransportStateCallback(&state_callback);
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
  transport_pair.first()->SetMediaTransportStateCallback(&state_callback);
  transport_pair.FlushAsyncInvokes();
}

TEST(LoopbackMediaTransport, StateChangeDeliveredToCallback) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->Start();
  MediaTransportPair transport_pair(thread.get());

  MockStateCallback state_callback;

  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kPending));
  EXPECT_CALL(state_callback, OnStateChanged(MediaTransportState::kWritable));
  transport_pair.first()->SetMediaTransportStateCallback(&state_callback);
  transport_pair.SetState(MediaTransportState::kWritable);
  transport_pair.FlushAsyncInvokes();
}

}  // namespace webrtc
