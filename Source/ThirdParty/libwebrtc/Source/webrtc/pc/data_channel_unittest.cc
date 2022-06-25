/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>
#include <vector>

#include "media/sctp/sctp_transport_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sctp_utils.h"
#include "pc/test/fake_data_channel_provider.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

using webrtc::DataChannelInterface;
using webrtc::SctpDataChannel;
using webrtc::SctpSidAllocator;

static constexpr int kDefaultTimeout = 10000;

class FakeDataChannelObserver : public webrtc::DataChannelObserver {
 public:
  FakeDataChannelObserver()
      : messages_received_(0),
        on_state_change_count_(0),
        on_buffered_amount_change_count_(0) {}

  void OnStateChange() { ++on_state_change_count_; }

  void OnBufferedAmountChange(uint64_t previous_amount) {
    ++on_buffered_amount_change_count_;
  }

  void OnMessage(const webrtc::DataBuffer& buffer) { ++messages_received_; }

  size_t messages_received() const { return messages_received_; }

  void ResetOnStateChangeCount() { on_state_change_count_ = 0; }

  void ResetOnBufferedAmountChangeCount() {
    on_buffered_amount_change_count_ = 0;
  }

  size_t on_state_change_count() const { return on_state_change_count_; }

  size_t on_buffered_amount_change_count() const {
    return on_buffered_amount_change_count_;
  }

 private:
  size_t messages_received_;
  size_t on_state_change_count_;
  size_t on_buffered_amount_change_count_;
};

// TODO(deadbeef): The fact that these tests use a fake provider makes them not
// too valuable. Should rewrite using the
// peerconnection_datachannel_unittest.cc infrastructure.
// TODO(bugs.webrtc.org/11547): Incorporate a dedicated network thread.
class SctpDataChannelTest : public ::testing::Test {
 protected:
  SctpDataChannelTest()
      : provider_(new FakeDataChannelProvider()),
        webrtc_data_channel_(SctpDataChannel::Create(provider_.get(),
                                                     "test",
                                                     init_,
                                                     rtc::Thread::Current(),
                                                     rtc::Thread::Current())) {}

  void SetChannelReady() {
    provider_->set_transport_available(true);
    webrtc_data_channel_->OnTransportChannelCreated();
    if (webrtc_data_channel_->id() < 0) {
      webrtc_data_channel_->SetSctpSid(0);
    }
    provider_->set_ready_to_send(true);
  }

  void AddObserver() {
    observer_.reset(new FakeDataChannelObserver());
    webrtc_data_channel_->RegisterObserver(observer_.get());
  }

  webrtc::InternalDataChannelInit init_;
  std::unique_ptr<FakeDataChannelProvider> provider_;
  std::unique_ptr<FakeDataChannelObserver> observer_;
  rtc::scoped_refptr<SctpDataChannel> webrtc_data_channel_;
};

class StateSignalsListener : public sigslot::has_slots<> {
 public:
  int opened_count() const { return opened_count_; }
  int closed_count() const { return closed_count_; }

  void OnSignalOpened(DataChannelInterface* data_channel) { ++opened_count_; }

  void OnSignalClosed(DataChannelInterface* data_channel) { ++closed_count_; }

 private:
  int opened_count_ = 0;
  int closed_count_ = 0;
};

// Verifies that the data channel is connected to the transport after creation.
TEST_F(SctpDataChannelTest, ConnectedToTransportOnCreated) {
  provider_->set_transport_available(true);
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", init_,
                              rtc::Thread::Current(), rtc::Thread::Current());

  EXPECT_TRUE(provider_->IsConnected(dc.get()));
  // The sid is not set yet, so it should not have added the streams.
  EXPECT_FALSE(provider_->IsSendStreamAdded(dc->id()));
  EXPECT_FALSE(provider_->IsRecvStreamAdded(dc->id()));

  dc->SetSctpSid(0);
  EXPECT_TRUE(provider_->IsSendStreamAdded(dc->id()));
  EXPECT_TRUE(provider_->IsRecvStreamAdded(dc->id()));
}

// Verifies that the data channel is connected to the transport if the transport
// is not available initially and becomes available later.
TEST_F(SctpDataChannelTest, ConnectedAfterTransportBecomesAvailable) {
  EXPECT_FALSE(provider_->IsConnected(webrtc_data_channel_.get()));

  provider_->set_transport_available(true);
  webrtc_data_channel_->OnTransportChannelCreated();
  EXPECT_TRUE(provider_->IsConnected(webrtc_data_channel_.get()));
}

// Tests the state of the data channel.
TEST_F(SctpDataChannelTest, StateTransition) {
  StateSignalsListener state_signals_listener;
  webrtc_data_channel_->SignalOpened.connect(
      &state_signals_listener, &StateSignalsListener::OnSignalOpened);
  webrtc_data_channel_->SignalClosed.connect(
      &state_signals_listener, &StateSignalsListener::OnSignalClosed);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            webrtc_data_channel_->state());
  EXPECT_EQ(state_signals_listener.opened_count(), 0);
  EXPECT_EQ(state_signals_listener.closed_count(), 0);
  SetChannelReady();

  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, webrtc_data_channel_->state());
  EXPECT_EQ(state_signals_listener.opened_count(), 1);
  EXPECT_EQ(state_signals_listener.closed_count(), 0);
  webrtc_data_channel_->Close();
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed,
            webrtc_data_channel_->state());
  EXPECT_TRUE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(state_signals_listener.opened_count(), 1);
  EXPECT_EQ(state_signals_listener.closed_count(), 1);
  // Verifies that it's disconnected from the transport.
  EXPECT_FALSE(provider_->IsConnected(webrtc_data_channel_.get()));
}

// Tests that DataChannel::buffered_amount() is correct after the channel is
// blocked.
TEST_F(SctpDataChannelTest, BufferedAmountWhenBlocked) {
  AddObserver();
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  size_t successful_send_count = 1;

  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());

  provider_->set_send_blocked(true);

  const int number_of_packets = 3;
  for (int i = 0; i < number_of_packets; ++i) {
    EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  }
  EXPECT_EQ(buffer.data.size() * number_of_packets,
            webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());

  provider_->set_send_blocked(false);
  successful_send_count += number_of_packets;
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());
}

// Tests that the queued data are sent when the channel transitions from blocked
// to unblocked.
TEST_F(SctpDataChannelTest, QueuedDataSentWhenUnblocked) {
  AddObserver();
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  provider_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  provider_->set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(1U, observer_->on_buffered_amount_change_count());
}

// Tests that no crash when the channel is blocked right away while trying to
// send queued data.
TEST_F(SctpDataChannelTest, BlockedWhenSendQueuedDataNoCrash) {
  AddObserver();
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  provider_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  // Set channel ready while it is still blocked.
  SetChannelReady();
  EXPECT_EQ(buffer.size(), webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  // Unblock the channel to send queued data again, there should be no crash.
  provider_->set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(1U, observer_->on_buffered_amount_change_count());
}

// Tests that DataChannel::messages_sent() and DataChannel::bytes_sent() are
// correct, sending data both while unblocked and while blocked.
TEST_F(SctpDataChannelTest, VerifyMessagesAndBytesSent) {
  AddObserver();
  SetChannelReady();
  std::vector<webrtc::DataBuffer> buffers({
      webrtc::DataBuffer("message 1"),
      webrtc::DataBuffer("msg 2"),
      webrtc::DataBuffer("message three"),
      webrtc::DataBuffer("quadra message"),
      webrtc::DataBuffer("fifthmsg"),
      webrtc::DataBuffer("message of the beast"),
  });

  // Default values.
  EXPECT_EQ(0U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_sent());

  // Send three buffers while not blocked.
  provider_->set_send_blocked(false);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[0]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[1]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[2]));
  size_t bytes_sent = buffers[0].size() + buffers[1].size() + buffers[2].size();
  EXPECT_EQ_WAIT(0U, webrtc_data_channel_->buffered_amount(), kDefaultTimeout);
  EXPECT_EQ(3U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());

  // Send three buffers while blocked, queuing the buffers.
  provider_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[3]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[4]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[5]));
  size_t bytes_queued =
      buffers[3].size() + buffers[4].size() + buffers[5].size();
  EXPECT_EQ(bytes_queued, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(3U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());

  // Unblock and make sure everything was sent.
  provider_->set_send_blocked(false);
  EXPECT_EQ_WAIT(0U, webrtc_data_channel_->buffered_amount(), kDefaultTimeout);
  bytes_sent += bytes_queued;
  EXPECT_EQ(6U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());
}

// Tests that the queued control message is sent when channel is ready.
TEST_F(SctpDataChannelTest, OpenMessageSent) {
  // Initially the id is unassigned.
  EXPECT_EQ(-1, webrtc_data_channel_->id());

  SetChannelReady();
  EXPECT_GE(webrtc_data_channel_->id(), 0);
  EXPECT_EQ(webrtc::DataMessageType::kControl,
            provider_->last_send_data_params().type);
  EXPECT_EQ(provider_->last_sid(), webrtc_data_channel_->id());
}

TEST_F(SctpDataChannelTest, QueuedOpenMessageSent) {
  provider_->set_send_blocked(true);
  SetChannelReady();
  provider_->set_send_blocked(false);

  EXPECT_EQ(webrtc::DataMessageType::kControl,
            provider_->last_send_data_params().type);
  EXPECT_EQ(provider_->last_sid(), webrtc_data_channel_->id());
}

// Tests that the DataChannel created after transport gets ready can enter OPEN
// state.
TEST_F(SctpDataChannelTest, LateCreatedChannelTransitionToOpen) {
  SetChannelReady();
  webrtc::InternalDataChannelInit init;
  init.id = 1;
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", init,
                              rtc::Thread::Current(), rtc::Thread::Current());
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, dc->state());
  EXPECT_TRUE_WAIT(webrtc::DataChannelInterface::kOpen == dc->state(), 1000);
}

// Tests that an unordered DataChannel sends data as ordered until the OPEN_ACK
// message is received.
TEST_F(SctpDataChannelTest, SendUnorderedAfterReceivesOpenAck) {
  SetChannelReady();
  webrtc::InternalDataChannelInit init;
  init.id = 1;
  init.ordered = false;
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", init,
                              rtc::Thread::Current(), rtc::Thread::Current());

  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, dc->state(), 1000);

  // Sends a message and verifies it's ordered.
  webrtc::DataBuffer buffer("some data");
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_TRUE(provider_->last_send_data_params().ordered);

  // Emulates receiving an OPEN_ACK message.
  cricket::ReceiveDataParams params;
  params.sid = init.id;
  params.type = webrtc::DataMessageType::kControl;
  rtc::CopyOnWriteBuffer payload;
  webrtc::WriteDataChannelOpenAckMessage(&payload);
  dc->OnDataReceived(params, payload);

  // Sends another message and verifies it's unordered.
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_FALSE(provider_->last_send_data_params().ordered);
}

// Tests that an unordered DataChannel sends unordered data after any DATA
// message is received.
TEST_F(SctpDataChannelTest, SendUnorderedAfterReceiveData) {
  SetChannelReady();
  webrtc::InternalDataChannelInit init;
  init.id = 1;
  init.ordered = false;
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", init,
                              rtc::Thread::Current(), rtc::Thread::Current());

  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, dc->state(), 1000);

  // Emulates receiving a DATA message.
  cricket::ReceiveDataParams params;
  params.sid = init.id;
  params.type = webrtc::DataMessageType::kText;
  webrtc::DataBuffer buffer("data");
  dc->OnDataReceived(params, buffer.data);

  // Sends a message and verifies it's unordered.
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_FALSE(provider_->last_send_data_params().ordered);
}

// Tests that the channel can't open until it's successfully sent the OPEN
// message.
TEST_F(SctpDataChannelTest, OpenWaitsForOpenMesssage) {
  webrtc::DataBuffer buffer("foo");

  provider_->set_send_blocked(true);
  SetChannelReady();
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            webrtc_data_channel_->state());
  provider_->set_send_blocked(false);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen,
                 webrtc_data_channel_->state(), 1000);
  EXPECT_EQ(webrtc::DataMessageType::kControl,
            provider_->last_send_data_params().type);
}

// Tests that close first makes sure all queued data gets sent.
TEST_F(SctpDataChannelTest, QueuedCloseFlushes) {
  webrtc::DataBuffer buffer("foo");

  provider_->set_send_blocked(true);
  SetChannelReady();
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting,
            webrtc_data_channel_->state());
  provider_->set_send_blocked(false);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen,
                 webrtc_data_channel_->state(), 1000);
  provider_->set_send_blocked(true);
  webrtc_data_channel_->Send(buffer);
  webrtc_data_channel_->Close();
  provider_->set_send_blocked(false);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed,
                 webrtc_data_channel_->state(), 1000);
  EXPECT_TRUE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(webrtc::DataMessageType::kText,
            provider_->last_send_data_params().type);
}

// Tests that messages are sent with the right id.
TEST_F(SctpDataChannelTest, SendDataId) {
  webrtc_data_channel_->SetSctpSid(1);
  SetChannelReady();
  webrtc::DataBuffer buffer("data");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(1, provider_->last_sid());
}

// Tests that the incoming messages with wrong ids are rejected.
TEST_F(SctpDataChannelTest, ReceiveDataWithInvalidId) {
  webrtc_data_channel_->SetSctpSid(1);
  SetChannelReady();

  AddObserver();

  cricket::ReceiveDataParams params;
  params.sid = 0;
  webrtc::DataBuffer buffer("abcd");
  webrtc_data_channel_->OnDataReceived(params, buffer.data);

  EXPECT_EQ(0U, observer_->messages_received());
}

// Tests that the incoming messages with right ids are accepted.
TEST_F(SctpDataChannelTest, ReceiveDataWithValidId) {
  webrtc_data_channel_->SetSctpSid(1);
  SetChannelReady();

  AddObserver();

  cricket::ReceiveDataParams params;
  params.sid = 1;
  webrtc::DataBuffer buffer("abcd");

  webrtc_data_channel_->OnDataReceived(params, buffer.data);
  EXPECT_EQ(1U, observer_->messages_received());
}

// Tests that no CONTROL message is sent if the datachannel is negotiated and
// not created from an OPEN message.
TEST_F(SctpDataChannelTest, NoMsgSentIfNegotiatedAndNotFromOpenMsg) {
  webrtc::InternalDataChannelInit config;
  config.id = 1;
  config.negotiated = true;
  config.open_handshake_role = webrtc::InternalDataChannelInit::kNone;

  SetChannelReady();
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", config,
                              rtc::Thread::Current(), rtc::Thread::Current());

  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, dc->state(), 1000);
  EXPECT_EQ(0, provider_->last_sid());
}

// Tests that DataChannel::messages_received() and DataChannel::bytes_received()
// are correct, receiving data both while not open and while open.
TEST_F(SctpDataChannelTest, VerifyMessagesAndBytesReceived) {
  AddObserver();
  std::vector<webrtc::DataBuffer> buffers({
      webrtc::DataBuffer("message 1"),
      webrtc::DataBuffer("msg 2"),
      webrtc::DataBuffer("message three"),
      webrtc::DataBuffer("quadra message"),
      webrtc::DataBuffer("fifthmsg"),
      webrtc::DataBuffer("message of the beast"),
  });

  webrtc_data_channel_->SetSctpSid(1);
  cricket::ReceiveDataParams params;
  params.sid = 1;

  // Default values.
  EXPECT_EQ(0U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_received());

  // Receive three buffers while data channel isn't open.
  webrtc_data_channel_->OnDataReceived(params, buffers[0].data);
  webrtc_data_channel_->OnDataReceived(params, buffers[1].data);
  webrtc_data_channel_->OnDataReceived(params, buffers[2].data);
  EXPECT_EQ(0U, observer_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_received());

  // Open channel and make sure everything was received.
  SetChannelReady();
  size_t bytes_received =
      buffers[0].size() + buffers[1].size() + buffers[2].size();
  EXPECT_EQ(3U, observer_->messages_received());
  EXPECT_EQ(3U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(bytes_received, webrtc_data_channel_->bytes_received());

  // Receive three buffers while open.
  webrtc_data_channel_->OnDataReceived(params, buffers[3].data);
  webrtc_data_channel_->OnDataReceived(params, buffers[4].data);
  webrtc_data_channel_->OnDataReceived(params, buffers[5].data);
  bytes_received += buffers[3].size() + buffers[4].size() + buffers[5].size();
  EXPECT_EQ(6U, observer_->messages_received());
  EXPECT_EQ(6U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(bytes_received, webrtc_data_channel_->bytes_received());
}

// Tests that OPEN_ACK message is sent if the datachannel is created from an
// OPEN message.
TEST_F(SctpDataChannelTest, OpenAckSentIfCreatedFromOpenMessage) {
  webrtc::InternalDataChannelInit config;
  config.id = 1;
  config.negotiated = true;
  config.open_handshake_role = webrtc::InternalDataChannelInit::kAcker;

  SetChannelReady();
  rtc::scoped_refptr<SctpDataChannel> dc =
      SctpDataChannel::Create(provider_.get(), "test1", config,
                              rtc::Thread::Current(), rtc::Thread::Current());

  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, dc->state(), 1000);

  EXPECT_EQ(config.id, provider_->last_sid());
  EXPECT_EQ(webrtc::DataMessageType::kControl,
            provider_->last_send_data_params().type);
}

// Tests the OPEN_ACK role assigned by InternalDataChannelInit.
TEST_F(SctpDataChannelTest, OpenAckRoleInitialization) {
  webrtc::InternalDataChannelInit init;
  EXPECT_EQ(webrtc::InternalDataChannelInit::kOpener, init.open_handshake_role);
  EXPECT_FALSE(init.negotiated);

  webrtc::DataChannelInit base;
  base.negotiated = true;
  webrtc::InternalDataChannelInit init2(base);
  EXPECT_EQ(webrtc::InternalDataChannelInit::kNone, init2.open_handshake_role);
}

// Tests that the DataChannel is closed if the sending buffer is full.
TEST_F(SctpDataChannelTest, ClosedWhenSendBufferFull) {
  SetChannelReady();

  rtc::CopyOnWriteBuffer buffer(1024);
  memset(buffer.MutableData(), 0, buffer.size());

  webrtc::DataBuffer packet(buffer, true);
  provider_->set_send_blocked(true);

  for (size_t i = 0; i < 16 * 1024 + 1; ++i) {
    EXPECT_TRUE(webrtc_data_channel_->Send(packet));
  }

  EXPECT_TRUE(
      webrtc::DataChannelInterface::kClosed == webrtc_data_channel_->state() ||
      webrtc::DataChannelInterface::kClosing == webrtc_data_channel_->state());
}

// Tests that the DataChannel is closed on transport errors.
TEST_F(SctpDataChannelTest, ClosedOnTransportError) {
  SetChannelReady();
  webrtc::DataBuffer buffer("abcd");
  provider_->set_transport_error();

  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(webrtc::DataChannelInterface::kClosed,
            webrtc_data_channel_->state());
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(webrtc::RTCErrorType::NETWORK_ERROR,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(webrtc::RTCErrorDetailType::NONE,
            webrtc_data_channel_->error().error_detail());
}

// Tests that the DataChannel is closed if the received buffer is full.
TEST_F(SctpDataChannelTest, ClosedWhenReceivedBufferFull) {
  SetChannelReady();
  rtc::CopyOnWriteBuffer buffer(1024);
  memset(buffer.MutableData(), 0, buffer.size());

  cricket::ReceiveDataParams params;
  params.sid = 0;

  // Receiving data without having an observer will overflow the buffer.
  for (size_t i = 0; i < 16 * 1024 + 1; ++i) {
    webrtc_data_channel_->OnDataReceived(params, buffer);
  }
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed,
            webrtc_data_channel_->state());
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(webrtc::RTCErrorType::RESOURCE_EXHAUSTED,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(webrtc::RTCErrorDetailType::NONE,
            webrtc_data_channel_->error().error_detail());
}

// Tests that sending empty data returns no error and keeps the channel open.
TEST_F(SctpDataChannelTest, SendEmptyData) {
  webrtc_data_channel_->SetSctpSid(1);
  SetChannelReady();
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, webrtc_data_channel_->state());

  webrtc::DataBuffer buffer("");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, webrtc_data_channel_->state());
}

// Tests that a channel can be closed without being opened or assigned an sid.
TEST_F(SctpDataChannelTest, NeverOpened) {
  provider_->set_transport_available(true);
  webrtc_data_channel_->OnTransportChannelCreated();
  webrtc_data_channel_->Close();
}

// Test that the data channel goes to the "closed" state (and doesn't crash)
// when its transport goes away, even while data is buffered.
TEST_F(SctpDataChannelTest, TransportDestroyedWhileDataBuffered) {
  SetChannelReady();

  rtc::CopyOnWriteBuffer buffer(1024);
  memset(buffer.MutableData(), 0, buffer.size());
  webrtc::DataBuffer packet(buffer, true);

  // Send a packet while sending is blocked so it ends up buffered.
  provider_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(packet));

  // Tell the data channel that its transport is being destroyed.
  // It should then stop using the transport (allowing us to delete it) and
  // transition to the "closed" state.
  webrtc::RTCError error(webrtc::RTCErrorType::OPERATION_ERROR_WITH_DATA, "");
  error.set_error_detail(webrtc::RTCErrorDetailType::SCTP_FAILURE);
  webrtc_data_channel_->OnTransportChannelClosed(error);
  provider_.reset(nullptr);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed,
                 webrtc_data_channel_->state(), kDefaultTimeout);
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(webrtc::RTCErrorType::OPERATION_ERROR_WITH_DATA,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(webrtc::RTCErrorDetailType::SCTP_FAILURE,
            webrtc_data_channel_->error().error_detail());
}

TEST_F(SctpDataChannelTest, TransportGotErrorCode) {
  SetChannelReady();

  // Tell the data channel that its transport is being destroyed with an
  // error code.
  // It should then report that error code.
  webrtc::RTCError error(webrtc::RTCErrorType::OPERATION_ERROR_WITH_DATA,
                         "Transport channel closed");
  error.set_error_detail(webrtc::RTCErrorDetailType::SCTP_FAILURE);
  error.set_sctp_cause_code(
      static_cast<uint16_t>(cricket::SctpErrorCauseCode::kProtocolViolation));
  webrtc_data_channel_->OnTransportChannelClosed(error);
  provider_.reset(nullptr);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed,
                 webrtc_data_channel_->state(), kDefaultTimeout);
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(webrtc::RTCErrorType::OPERATION_ERROR_WITH_DATA,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(webrtc::RTCErrorDetailType::SCTP_FAILURE,
            webrtc_data_channel_->error().error_detail());
  EXPECT_EQ(
      static_cast<uint16_t>(cricket::SctpErrorCauseCode::kProtocolViolation),
      webrtc_data_channel_->error().sctp_cause_code());
}

class SctpSidAllocatorTest : public ::testing::Test {
 protected:
  SctpSidAllocator allocator_;
};

// Verifies that an even SCTP id is allocated for SSL_CLIENT and an odd id for
// SSL_SERVER.
TEST_F(SctpSidAllocatorTest, SctpIdAllocationBasedOnRole) {
  int id;
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &id));
  EXPECT_EQ(1, id);
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &id));
  EXPECT_EQ(0, id);
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &id));
  EXPECT_EQ(3, id);
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &id));
  EXPECT_EQ(2, id);
}

// Verifies that SCTP ids of existing DataChannels are not reused.
TEST_F(SctpSidAllocatorTest, SctpIdAllocationNoReuse) {
  int old_id = 1;
  EXPECT_TRUE(allocator_.ReserveSid(old_id));

  int new_id;
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &new_id));
  EXPECT_NE(old_id, new_id);

  old_id = 0;
  EXPECT_TRUE(allocator_.ReserveSid(old_id));
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &new_id));
  EXPECT_NE(old_id, new_id);
}

// Verifies that SCTP ids of removed DataChannels can be reused.
TEST_F(SctpSidAllocatorTest, SctpIdReusedForRemovedDataChannel) {
  int odd_id = 1;
  int even_id = 0;
  EXPECT_TRUE(allocator_.ReserveSid(odd_id));
  EXPECT_TRUE(allocator_.ReserveSid(even_id));

  int allocated_id = -1;
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &allocated_id));
  EXPECT_EQ(odd_id + 2, allocated_id);

  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &allocated_id));
  EXPECT_EQ(even_id + 2, allocated_id);

  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &allocated_id));
  EXPECT_EQ(odd_id + 4, allocated_id);

  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &allocated_id));
  EXPECT_EQ(even_id + 4, allocated_id);

  allocator_.ReleaseSid(odd_id);
  allocator_.ReleaseSid(even_id);

  // Verifies that removed ids are reused.
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &allocated_id));
  EXPECT_EQ(odd_id, allocated_id);

  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &allocated_id));
  EXPECT_EQ(even_id, allocated_id);

  // Verifies that used higher ids are not reused.
  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_SERVER, &allocated_id));
  EXPECT_EQ(odd_id + 6, allocated_id);

  EXPECT_TRUE(allocator_.AllocateSid(rtc::SSL_CLIENT, &allocated_id));
  EXPECT_EQ(even_id + 6, allocated_id);
}
