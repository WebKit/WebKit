/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/quicdatachannel.h"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/base/bind.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/quic/quictransportchannel.h"
#include "webrtc/p2p/quic/reliablequicstream.h"

using cricket::FakeTransportChannel;
using cricket::QuicTransportChannel;
using cricket::ReliableQuicStream;

using webrtc::DataBuffer;
using webrtc::DataChannelObserver;
using webrtc::DataChannelInit;
using webrtc::QuicDataChannel;

namespace {

// Timeout for asynchronous operations.
static const int kTimeoutMs = 1000;  // milliseconds

// Small messages that can be sent within a single QUIC packet.
static const std::string kSmallMessage1 = "Hello, world!";
static const std::string kSmallMessage2 = "WebRTC";
static const std::string kSmallMessage3 = "1";
static const std::string kSmallMessage4 = "abcdefghijklmnopqrstuvwxyz";
static const DataBuffer kSmallBuffer1(kSmallMessage1);
static const DataBuffer kSmallBuffer2(kSmallMessage2);
static const DataBuffer kSmallBuffer3(kSmallMessage3);
static const DataBuffer kSmallBuffer4(kSmallMessage4);

// Large messages (> 1350 bytes) that exceed the max size of a QUIC packet.
// These are < 16 KB so they don't exceed the QUIC stream flow control limit.
static const std::string kLargeMessage1 = std::string("a", 2000);
static const std::string kLargeMessage2 = std::string("a", 4000);
static const std::string kLargeMessage3 = std::string("a", 8000);
static const std::string kLargeMessage4 = std::string("a", 12000);
static const DataBuffer kLargeBuffer1(kLargeMessage1);
static const DataBuffer kLargeBuffer2(kLargeMessage2);
static const DataBuffer kLargeBuffer3(kLargeMessage3);
static const DataBuffer kLargeBuffer4(kLargeMessage4);

// Oversized message (> 16 KB) that violates the QUIC stream flow control limit.
static const std::string kOversizedMessage = std::string("a", 20000);
static const DataBuffer kOversizedBuffer(kOversizedMessage);

// Creates a fingerprint from a certificate.
static rtc::SSLFingerprint* CreateFingerprint(rtc::RTCCertificate* cert) {
  std::string digest_algorithm;
  cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_algorithm);
  std::unique_ptr<rtc::SSLFingerprint> fingerprint(
      rtc::SSLFingerprint::Create(digest_algorithm, cert->identity()));
  return fingerprint.release();
}

// FakeObserver receives messages from the QuicDataChannel.
class FakeObserver : public DataChannelObserver {
 public:
  FakeObserver()
      : on_state_change_count_(0), on_buffered_amount_change_count_(0) {}

  // DataChannelObserver overrides.
  void OnStateChange() override { ++on_state_change_count_; }
  void OnBufferedAmountChange(uint64_t previous_amount) override {
    ++on_buffered_amount_change_count_;
  }
  void OnMessage(const webrtc::DataBuffer& buffer) override {
    messages_.push_back(std::string(buffer.data.data<char>(), buffer.size()));
  }

  const std::vector<std::string>& messages() const { return messages_; }

  size_t messages_received() const { return messages_.size(); }

  size_t on_state_change_count() const { return on_state_change_count_; }

  size_t on_buffered_amount_change_count() const {
    return on_buffered_amount_change_count_;
  }

 private:
  std::vector<std::string> messages_;
  size_t on_state_change_count_;
  size_t on_buffered_amount_change_count_;
};

// FakeQuicDataTransport simulates QuicDataTransport by dispatching QUIC
// stream messages to data channels and encoding/decoding messages.
class FakeQuicDataTransport : public sigslot::has_slots<> {
 public:
  FakeQuicDataTransport() {}

  void ConnectToTransportChannel(QuicTransportChannel* quic_transport_channel) {
    quic_transport_channel->SignalIncomingStream.connect(
        this, &FakeQuicDataTransport::OnIncomingStream);
  }

  rtc::scoped_refptr<QuicDataChannel> CreateDataChannel(
      int id,
      const std::string& label,
      const std::string& protocol) {
    DataChannelInit config;
    config.id = id;
    config.protocol = protocol;
    rtc::scoped_refptr<QuicDataChannel> data_channel(
        new QuicDataChannel(rtc::Thread::Current(), rtc::Thread::Current(),
                            rtc::Thread::Current(), label, config));
    data_channel_by_id_[id] = data_channel;
    return data_channel;
  }

 private:
  void OnIncomingStream(cricket::ReliableQuicStream* stream) {
    incoming_stream_ = stream;
    incoming_stream_->SignalDataReceived.connect(
        this, &FakeQuicDataTransport::OnDataReceived);
  }

  void OnDataReceived(net::QuicStreamId id, const char* data, size_t len) {
    ASSERT_EQ(incoming_stream_->id(), id);
    incoming_stream_->SignalDataReceived.disconnect(this);
    // Retrieve the data channel ID and message ID.
    int data_channel_id;
    uint64_t message_id;
    size_t bytes_read;
    ASSERT_TRUE(webrtc::ParseQuicDataMessageHeader(data, len, &data_channel_id,
                                                   &message_id, &bytes_read));
    data += bytes_read;
    len -= bytes_read;
    // Dispatch the message to the matching QuicDataChannel.
    const auto& kv = data_channel_by_id_.find(data_channel_id);
    ASSERT_NE(kv, data_channel_by_id_.end());
    QuicDataChannel* data_channel = kv->second;
    QuicDataChannel::Message message;
    message.id = message_id;
    message.buffer = rtc::CopyOnWriteBuffer(data, len);
    message.stream = incoming_stream_;
    data_channel->OnIncomingMessage(std::move(message));
    incoming_stream_ = nullptr;
  }

  // Map of data channel ID => QuicDataChannel.
  std::map<int, rtc::scoped_refptr<QuicDataChannel>> data_channel_by_id_;
  // Last incoming QUIC stream which has arrived.
  cricket::ReliableQuicStream* incoming_stream_ = nullptr;
};

// A peer who creates a QuicDataChannel to transfer data, and simulates network
// connectivity with a fake ICE channel wrapped by the QUIC transport channel.
class QuicDataChannelPeer {
 public:
  QuicDataChannelPeer()
      : ice_transport_channel_(new FakeTransportChannel("data", 0)),
        quic_transport_channel_(ice_transport_channel_) {
    ice_transport_channel_->SetAsync(true);
    fake_quic_data_transport_.ConnectToTransportChannel(
        &quic_transport_channel_);
  }

  void GenerateCertificateAndFingerprint() {
    rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate("cert_name", rtc::KT_DEFAULT)));
    quic_transport_channel_.SetLocalCertificate(local_cert);
    local_fingerprint_.reset(CreateFingerprint(local_cert.get()));
  }

  rtc::scoped_refptr<QuicDataChannel> CreateDataChannelWithTransportChannel(
      int id,
      const std::string& label,
      const std::string& protocol) {
    rtc::scoped_refptr<QuicDataChannel> data_channel =
        fake_quic_data_transport_.CreateDataChannel(id, label, protocol);
    data_channel->SetTransportChannel(&quic_transport_channel_);
    return data_channel;
  }

  rtc::scoped_refptr<QuicDataChannel> CreateDataChannelWithoutTransportChannel(
      int id,
      const std::string& label,
      const std::string& protocol) {
    return fake_quic_data_transport_.CreateDataChannel(id, label, protocol);
  }

  // Connects |ice_transport_channel_| to that of the other peer.
  void Connect(QuicDataChannelPeer* other_peer) {
    ice_transport_channel_->SetDestination(other_peer->ice_transport_channel_);
  }

  std::unique_ptr<rtc::SSLFingerprint>& local_fingerprint() {
    return local_fingerprint_;
  }

  QuicTransportChannel* quic_transport_channel() {
    return &quic_transport_channel_;
  }

  FakeTransportChannel* ice_transport_channel() {
    return ice_transport_channel_;
  }

 private:
  FakeTransportChannel* ice_transport_channel_;
  QuicTransportChannel quic_transport_channel_;

  std::unique_ptr<rtc::SSLFingerprint> local_fingerprint_;

  FakeQuicDataTransport fake_quic_data_transport_;
};

class QuicDataChannelTest : public testing::Test {
 public:
  QuicDataChannelTest() {}

  // Connect the QuicTransportChannels and complete the crypto handshake.
  void ConnectTransportChannels() {
    SetCryptoParameters();
    peer1_.Connect(&peer2_);
    ASSERT_TRUE_WAIT(peer1_.quic_transport_channel()->writable() &&
                         peer2_.quic_transport_channel()->writable(),
                     kTimeoutMs);
  }

  // Sets crypto parameters required for the QUIC handshake.
  void SetCryptoParameters() {
    peer1_.GenerateCertificateAndFingerprint();
    peer2_.GenerateCertificateAndFingerprint();

    peer1_.quic_transport_channel()->SetSslRole(rtc::SSL_CLIENT);
    peer2_.quic_transport_channel()->SetSslRole(rtc::SSL_SERVER);

    std::unique_ptr<rtc::SSLFingerprint>& peer1_fingerprint =
        peer1_.local_fingerprint();
    std::unique_ptr<rtc::SSLFingerprint>& peer2_fingerprint =
        peer2_.local_fingerprint();

    peer1_.quic_transport_channel()->SetRemoteFingerprint(
        peer2_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(peer2_fingerprint->digest.data()),
        peer2_fingerprint->digest.size());
    peer2_.quic_transport_channel()->SetRemoteFingerprint(
        peer1_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(peer1_fingerprint->digest.data()),
        peer1_fingerprint->digest.size());
  }

 protected:
  QuicDataChannelPeer peer1_;
  QuicDataChannelPeer peer2_;
};

// Tests that a QuicDataChannel transitions from connecting to open when
// the QuicTransportChannel becomes writable for the first time.
TEST_F(QuicDataChannelTest, DataChannelOpensWhenTransportChannelConnects) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(4, "label", "protocol");
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel->state());
  ConnectTransportChannels();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, data_channel->state(),
                 kTimeoutMs);
}

// Tests that a QuicDataChannel transitions from connecting to open when
// SetTransportChannel is called with a QuicTransportChannel that is already
// writable.
TEST_F(QuicDataChannelTest, DataChannelOpensWhenTransportChannelWritable) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithoutTransportChannel(4, "label", "protocol");
  ConnectTransportChannels();
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel->state());
  data_channel->SetTransportChannel(peer1_.quic_transport_channel());
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel->state());
}

// Tests that the QuicDataChannel transfers messages small enough to fit into a
// single QUIC stream frame.
TEST_F(QuicDataChannelTest, TransferSmallMessage) {
  ConnectTransportChannels();
  int data_channel_id = 2;
  std::string label = "label";
  std::string protocol = "protocol";
  rtc::scoped_refptr<QuicDataChannel> peer1_data_channel =
      peer1_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer1_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);
  rtc::scoped_refptr<QuicDataChannel> peer2_data_channel =
      peer2_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer2_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);

  FakeObserver peer1_observer;
  peer1_data_channel->RegisterObserver(&peer1_observer);
  FakeObserver peer2_observer;
  peer2_data_channel->RegisterObserver(&peer2_observer);

  // peer1 -> peer2
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer1));
  ASSERT_EQ_WAIT(1, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kSmallMessage1, peer2_observer.messages()[0]);
  // peer2 -> peer1
  EXPECT_TRUE(peer2_data_channel->Send(kSmallBuffer2));
  ASSERT_EQ_WAIT(1, peer1_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kSmallMessage2, peer1_observer.messages()[0]);
  // peer2 -> peer1
  EXPECT_TRUE(peer2_data_channel->Send(kSmallBuffer3));
  ASSERT_EQ_WAIT(2, peer1_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kSmallMessage3, peer1_observer.messages()[1]);
  // peer1 -> peer2
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer4));
  ASSERT_EQ_WAIT(2, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kSmallMessage4, peer2_observer.messages()[1]);
}

// Tests that QuicDataChannel transfers messages large enough to fit into
// multiple QUIC stream frames, which don't violate the QUIC flow control limit.
// These require buffering by the QuicDataChannel.
TEST_F(QuicDataChannelTest, TransferLargeMessage) {
  ConnectTransportChannels();
  int data_channel_id = 347;
  std::string label = "label";
  std::string protocol = "protocol";
  rtc::scoped_refptr<QuicDataChannel> peer1_data_channel =
      peer1_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer1_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);
  rtc::scoped_refptr<QuicDataChannel> peer2_data_channel =
      peer2_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer2_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);

  FakeObserver peer1_observer;
  peer1_data_channel->RegisterObserver(&peer1_observer);
  FakeObserver peer2_observer;
  peer2_data_channel->RegisterObserver(&peer2_observer);

  // peer1 -> peer2
  EXPECT_TRUE(peer1_data_channel->Send(kLargeBuffer1));
  ASSERT_TRUE_WAIT(peer2_observer.messages_received() == 1, kTimeoutMs);
  EXPECT_EQ(kLargeMessage1, peer2_observer.messages()[0]);
  // peer2 -> peer1
  EXPECT_TRUE(peer2_data_channel->Send(kLargeBuffer2));
  ASSERT_EQ_WAIT(1, peer1_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kLargeMessage2, peer1_observer.messages()[0]);
  // peer2 -> peer1
  EXPECT_TRUE(peer2_data_channel->Send(kLargeBuffer3));
  ASSERT_EQ_WAIT(2, peer1_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kLargeMessage3, peer1_observer.messages()[1]);
  // peer1 -> peer2
  EXPECT_TRUE(peer1_data_channel->Send(kLargeBuffer4));
  ASSERT_EQ_WAIT(2, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kLargeMessage4, peer2_observer.messages()[1]);
}

// Tests that when a message size exceeds the flow control limit (> 16KB), the
// QuicDataChannel can queue the data and send it after receiving window update
// frames from the remote peer.
TEST_F(QuicDataChannelTest, TransferOversizedMessage) {
  ConnectTransportChannels();
  int data_channel_id = 189;
  std::string label = "label";
  std::string protocol = "protocol";
  rtc::scoped_refptr<QuicDataChannel> peer1_data_channel =
      peer1_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  rtc::scoped_refptr<QuicDataChannel> peer2_data_channel =
      peer2_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer2_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);

  FakeObserver peer1_observer;
  peer1_data_channel->RegisterObserver(&peer1_observer);
  FakeObserver peer2_observer;
  peer2_data_channel->RegisterObserver(&peer2_observer);

  EXPECT_TRUE(peer1_data_channel->Send(kOversizedBuffer));
  EXPECT_EQ(1, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_EQ_WAIT(1, peer2_data_channel->GetNumIncomingStreams(), kTimeoutMs);
  ASSERT_EQ_WAIT(1, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(kOversizedMessage, peer2_observer.messages()[0]);
  EXPECT_EQ(0, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_EQ(0, peer2_data_channel->GetNumIncomingStreams());
}

// Tests that empty messages can be sent.
TEST_F(QuicDataChannelTest, TransferEmptyMessage) {
  ConnectTransportChannels();
  int data_channel_id = 69;
  std::string label = "label";
  std::string protocol = "protocol";
  rtc::scoped_refptr<QuicDataChannel> peer1_data_channel =
      peer1_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  rtc::scoped_refptr<QuicDataChannel> peer2_data_channel =
      peer2_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer2_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);

  FakeObserver peer1_observer;
  peer1_data_channel->RegisterObserver(&peer1_observer);
  FakeObserver peer2_observer;
  peer2_data_channel->RegisterObserver(&peer2_observer);

  EXPECT_TRUE(peer1_data_channel->Send(DataBuffer("")));
  ASSERT_EQ_WAIT(1, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ("", peer2_observer.messages()[0]);
}

// Tests that when the QuicDataChannel is open and sends a message while the
// QuicTransportChannel is unwritable, it gets buffered then received once the
// QuicTransportChannel becomes writable again.
TEST_F(QuicDataChannelTest, MessagesReceivedWhenTransportChannelReconnects) {
  ConnectTransportChannels();
  int data_channel_id = 401;
  std::string label = "label";
  std::string protocol = "protocol";
  rtc::scoped_refptr<QuicDataChannel> peer1_data_channel =
      peer1_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer1_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);
  rtc::scoped_refptr<QuicDataChannel> peer2_data_channel =
      peer2_.CreateDataChannelWithTransportChannel(data_channel_id, label,
                                                   protocol);
  ASSERT_TRUE(peer2_data_channel->state() ==
              webrtc::DataChannelInterface::kOpen);

  FakeObserver peer1_observer;
  peer1_data_channel->RegisterObserver(&peer1_observer);
  FakeObserver peer2_observer;
  peer2_data_channel->RegisterObserver(&peer2_observer);
  // writable => unwritable
  peer1_.ice_transport_channel()->SetWritable(false);
  ASSERT_FALSE(peer1_.quic_transport_channel()->writable());
  // Verify that sent data is buffered.
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer1));
  EXPECT_EQ(1, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer2));
  EXPECT_EQ(2, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer3));
  EXPECT_EQ(3, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_TRUE(peer1_data_channel->Send(kSmallBuffer4));
  EXPECT_EQ(4, peer1_data_channel->GetNumWriteBlockedStreams());
  // unwritable => writable
  peer1_.ice_transport_channel()->SetWritable(true);
  ASSERT_TRUE(peer1_.quic_transport_channel()->writable());
  ASSERT_EQ_WAIT(4, peer2_observer.messages_received(), kTimeoutMs);
  EXPECT_EQ(0, peer1_data_channel->GetNumWriteBlockedStreams());
  EXPECT_EQ(0, peer2_data_channel->GetNumIncomingStreams());
}

// Tests that the QuicDataChannel does not send before it is open.
TEST_F(QuicDataChannelTest, TransferMessageBeforeChannelOpens) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(6, "label", "protocol");
  ASSERT_TRUE(data_channel->state() ==
              webrtc::DataChannelInterface::kConnecting);
  EXPECT_FALSE(data_channel->Send(kSmallBuffer1));
}

// Tests that the QuicDataChannel does not send after it is closed.
TEST_F(QuicDataChannelTest, TransferDataAfterChannelClosed) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(42, "label", "protocol");
  data_channel->Close();
  ASSERT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
  EXPECT_FALSE(data_channel->Send(kSmallBuffer1));
}

// Tests that QuicDataChannel state changes fire OnStateChanged() for the
// observer, with the correct data channel states, when the data channel
// transitions from kConnecting => kOpen => kClosing => kClosed.
TEST_F(QuicDataChannelTest, OnStateChangedFired) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(7, "label", "protocol");
  FakeObserver observer;
  data_channel->RegisterObserver(&observer);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel->state());
  EXPECT_EQ(0, observer.on_state_change_count());
  ConnectTransportChannels();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, data_channel->state(),
                 kTimeoutMs);
  EXPECT_EQ(1, observer.on_state_change_count());
  data_channel->Close();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
  // 2 state changes due to kClosing and kClosed.
  EXPECT_EQ(3, observer.on_state_change_count());
}

// Tests that a QuicTransportChannel can be closed without being opened when it
// is connected to a transprot chanenl.
TEST_F(QuicDataChannelTest, NeverOpenedWithTransportChannel) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(7, "label", "protocol");
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel->state());
  data_channel->Close();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
}

// Tests that a QuicTransportChannel can be closed without being opened or
// connected to a transport channel.
TEST_F(QuicDataChannelTest, NeverOpenedWithoutTransportChannel) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithoutTransportChannel(7, "label", "protocol");
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel->state());
  data_channel->Close();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
}

// Tests that the QuicDataChannel is closed when the QUIC connection closes.
TEST_F(QuicDataChannelTest, ClosedOnTransportError) {
  ConnectTransportChannels();
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(1, "label", "protocol");
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel->state());
  ReliableQuicStream* stream =
      peer1_.quic_transport_channel()->CreateQuicStream();
  ASSERT_NE(nullptr, stream);
  stream->CloseConnectionWithDetails(net::QuicErrorCode::QUIC_NO_ERROR,
                                     "Closing QUIC for testing");
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
}

// Tests that an already closed QuicDataChannel does not fire onStateChange and
// remains closed.
TEST_F(QuicDataChannelTest, DoesNotChangeStateWhenClosed) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(4, "label", "protocol");
  FakeObserver observer;
  data_channel->RegisterObserver(&observer);
  data_channel->Close();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kClosed, data_channel->state(),
                 kTimeoutMs);
  // OnStateChange called for kClosing and kClosed.
  EXPECT_EQ(2, observer.on_state_change_count());
  // Call Close() again to verify that the state cannot be kClosing.
  data_channel->Close();
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed, data_channel->state());
  EXPECT_EQ(2, observer.on_state_change_count());
  ConnectTransportChannels();
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed, data_channel->state());
  EXPECT_EQ(2, observer.on_state_change_count());
  // writable => unwritable
  peer1_.ice_transport_channel()->SetWritable(false);
  ASSERT_FALSE(peer1_.quic_transport_channel()->writable());
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed, data_channel->state());
  EXPECT_EQ(2, observer.on_state_change_count());
  // unwritable => writable
  peer1_.ice_transport_channel()->SetWritable(true);
  ASSERT_TRUE(peer1_.quic_transport_channel()->writable());
  EXPECT_EQ(webrtc::DataChannelInterface::kClosed, data_channel->state());
  EXPECT_EQ(2, observer.on_state_change_count());
}

// Tests that when the QuicDataChannel is open and the QuicTransportChannel
// transitions between writable and unwritable, it does not fire onStateChange
// and remains open.
TEST_F(QuicDataChannelTest, DoesNotChangeStateWhenTransportChannelReconnects) {
  ConnectTransportChannels();
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(4, "label", "protocol");
  FakeObserver observer;
  data_channel->RegisterObserver(&observer);
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel->state());
  EXPECT_EQ(0, observer.on_state_change_count());
  // writable => unwritable
  peer1_.ice_transport_channel()->SetWritable(false);
  ASSERT_FALSE(peer1_.quic_transport_channel()->writable());
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel->state());
  EXPECT_EQ(0, observer.on_state_change_count());
  // unwritable => writable
  peer1_.ice_transport_channel()->SetWritable(true);
  ASSERT_TRUE(peer1_.quic_transport_channel()->writable());
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel->state());
  EXPECT_EQ(0, observer.on_state_change_count());
}

// Tests that SetTransportChannel returns false when setting a NULL transport
// channel or a transport channel that is not equivalent to the one already set.
TEST_F(QuicDataChannelTest, SetTransportChannelReturnValue) {
  rtc::scoped_refptr<QuicDataChannel> data_channel =
      peer1_.CreateDataChannelWithTransportChannel(4, "label", "protocol");
  EXPECT_FALSE(data_channel->SetTransportChannel(nullptr));
  QuicTransportChannel* transport_channel = peer1_.quic_transport_channel();
  EXPECT_TRUE(data_channel->SetTransportChannel(transport_channel));
  EXPECT_TRUE(data_channel->SetTransportChannel(transport_channel));
  QuicTransportChannel* other_transport_channel =
      peer2_.quic_transport_channel();
  EXPECT_FALSE(data_channel->SetTransportChannel(other_transport_channel));
}

// Tests that the QUIC message header is encoded with the correct number of
// bytes and is properly decoded.
TEST_F(QuicDataChannelTest, EncodeParseQuicDataMessageHeader) {
  int data_channel_id1 = 127;  // 1 byte
  uint64_t message_id1 = 0;    // 1 byte
  rtc::CopyOnWriteBuffer header1;
  webrtc::WriteQuicDataChannelMessageHeader(data_channel_id1, message_id1,
                                            &header1);
  EXPECT_EQ(2u, header1.size());

  int decoded_data_channel_id1;
  uint64_t decoded_message_id1;
  size_t bytes_read1;
  ASSERT_TRUE(webrtc::ParseQuicDataMessageHeader(
      header1.data<char>(), header1.size(), &decoded_data_channel_id1,
      &decoded_message_id1, &bytes_read1));
  EXPECT_EQ(data_channel_id1, decoded_data_channel_id1);
  EXPECT_EQ(message_id1, decoded_message_id1);
  EXPECT_EQ(2u, bytes_read1);

  int data_channel_id2 = 4178;           // 2 bytes
  uint64_t message_id2 = 1324921792003;  // 6 bytes
  rtc::CopyOnWriteBuffer header2;
  webrtc::WriteQuicDataChannelMessageHeader(data_channel_id2, message_id2,
                                            &header2);
  EXPECT_EQ(8u, header2.size());

  int decoded_data_channel_id2;
  uint64_t decoded_message_id2;
  size_t bytes_read2;
  ASSERT_TRUE(webrtc::ParseQuicDataMessageHeader(
      header2.data<char>(), header2.size(), &decoded_data_channel_id2,
      &decoded_message_id2, &bytes_read2));
  EXPECT_EQ(data_channel_id2, decoded_data_channel_id2);
  EXPECT_EQ(message_id2, decoded_message_id2);
  EXPECT_EQ(8u, bytes_read2);
}

}  // namespace
