/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/quicdatatransport.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/quic/quictransportchannel.h"
#include "webrtc/p2p/quic/reliablequicstream.h"
#include "webrtc/pc/quicdatachannel.h"

using webrtc::DataBuffer;
using webrtc::DataChannelInit;
using webrtc::DataChannelInterface;
using webrtc::DataChannelObserver;
using webrtc::QuicDataChannel;
using webrtc::QuicDataTransport;
using cricket::FakeTransportChannel;
using cricket::FakeTransportController;
using cricket::QuicTransportChannel;
using cricket::ReliableQuicStream;

namespace {

// Timeout for asynchronous operations.
static const int kTimeoutMs = 1000;  // milliseconds
static const char kTransportName[] = "data";

// FakeObserver receives messages from the data channel.
class FakeObserver : public DataChannelObserver {
 public:
  FakeObserver() {}

  void OnStateChange() override {}

  void OnBufferedAmountChange(uint64_t previous_amount) override {}

  void OnMessage(const webrtc::DataBuffer& buffer) override {
    messages_.push_back(std::string(buffer.data.data<char>(), buffer.size()));
  }

  const std::vector<std::string>& messages() const { return messages_; }

  size_t messages_received() const { return messages_.size(); }

 private:
  std::vector<std::string> messages_;
};

// A peer who uses a QUIC transport channel and fake ICE transport channel to
// send or receive data.
class QuicDataTransportPeer {
 public:
  QuicDataTransportPeer()
      : fake_transport_controller_(new FakeTransportController()),
        quic_data_transport_(rtc::Thread::Current(),
                             rtc::Thread::Current(),
                             rtc::Thread::Current(),
                             fake_transport_controller_.get()) {
    fake_transport_controller_->use_quic();
    quic_data_transport_.set_content_name("data");
    quic_data_transport_.SetTransport(kTransportName);
    ice_transport_channel_ = static_cast<FakeTransportChannel*>(
        quic_data_transport_.quic_transport_channel()->ice_transport_channel());
    ice_transport_channel_->SetAsync(true);
  }

  void GenerateCertificateAndFingerprint() {
    rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate("cert_name", rtc::KT_DEFAULT)));
    quic_data_transport_.quic_transport_channel()->SetLocalCertificate(
        local_cert);
    local_fingerprint_.reset(CreateFingerprint(local_cert.get()));
  }

  // Connects |ice_transport_channel_| to that of the other peer.
  void Connect(QuicDataTransportPeer* other_peer) {
    ice_transport_channel_->SetDestination(other_peer->ice_transport_channel_);
  }

  std::unique_ptr<rtc::SSLFingerprint>& local_fingerprint() {
    return local_fingerprint_;
  }

  QuicTransportChannel* quic_transport_channel() {
    return quic_data_transport_.quic_transport_channel();
  }

  // Write a messge directly to the ReliableQuicStream.
  void WriteMessage(int data_channel_id,
                    uint64_t message_id,
                    const std::string& message) {
    ReliableQuicStream* stream =
        quic_data_transport_.quic_transport_channel()->CreateQuicStream();
    rtc::CopyOnWriteBuffer payload;
    webrtc::WriteQuicDataChannelMessageHeader(data_channel_id, message_id,
                                              &payload);
    stream->Write(payload.data<char>(), payload.size(), false);
    stream->Write(message.data(), message.size(), true);
  }

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const DataChannelInit* config) {
    return quic_data_transport_.CreateDataChannel("testing", config);
  }

  QuicDataTransport* quic_data_transport() { return &quic_data_transport_; }

 private:
  // Creates a fingerprint from a certificate.
  rtc::SSLFingerprint* CreateFingerprint(rtc::RTCCertificate* cert) {
    std::string digest_algorithm;
    cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_algorithm);
    std::unique_ptr<rtc::SSLFingerprint> fingerprint(
        rtc::SSLFingerprint::Create(digest_algorithm, cert->identity()));
    return fingerprint.release();
  }

  std::unique_ptr<FakeTransportController> fake_transport_controller_;
  QuicDataTransport quic_data_transport_;
  FakeTransportChannel* ice_transport_channel_;
  std::unique_ptr<rtc::SSLFingerprint> local_fingerprint_;
};

class QuicDataTransportTest : public testing::Test {
 public:
  QuicDataTransportTest() {}

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
  QuicDataTransportPeer peer1_;
  QuicDataTransportPeer peer2_;
};

// Tests creation and destruction of data channels.
TEST_F(QuicDataTransportTest, CreateAndDestroyDataChannels) {
  QuicDataTransport* quic_data_transport = peer2_.quic_data_transport();
  EXPECT_FALSE(quic_data_transport->HasDataChannels());
  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    EXPECT_FALSE(quic_data_transport->HasDataChannel(data_channel_id));
    webrtc::DataChannelInit config;
    config.id = data_channel_id;
    rtc::scoped_refptr<DataChannelInterface> data_channel =
        peer2_.CreateDataChannel(&config);
    EXPECT_NE(nullptr, data_channel);
    EXPECT_EQ(data_channel_id, data_channel->id());
    EXPECT_TRUE(quic_data_transport->HasDataChannel(data_channel_id));
  }
  EXPECT_TRUE(quic_data_transport->HasDataChannels());
  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    quic_data_transport->DestroyDataChannel(data_channel_id);
    EXPECT_FALSE(quic_data_transport->HasDataChannel(data_channel_id));
  }
  EXPECT_FALSE(quic_data_transport->HasDataChannels());
}

// Tests that the QuicDataTransport does not allow creating multiple
// QuicDataChannels with the same id.
TEST_F(QuicDataTransportTest, CannotCreateDataChannelsWithSameId) {
  webrtc::DataChannelInit config;
  config.id = 2;
  EXPECT_NE(nullptr, peer2_.CreateDataChannel(&config));
  EXPECT_EQ(nullptr, peer2_.CreateDataChannel(&config));
}

// Tests that any data channels created by the QuicDataTransport are in state
// kConnecting before the QuicTransportChannel is set, then transition to state
// kOpen when the transport channel becomes writable.
TEST_F(QuicDataTransportTest, DataChannelsOpenWhenTransportChannelWritable) {
  webrtc::DataChannelInit config1;
  config1.id = 7;
  rtc::scoped_refptr<DataChannelInterface> data_channel1 =
      peer2_.CreateDataChannel(&config1);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel1->state());
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel1->state());
  webrtc::DataChannelInit config2;
  config2.id = 14;
  rtc::scoped_refptr<DataChannelInterface> data_channel2 =
      peer2_.CreateDataChannel(&config2);
  EXPECT_EQ(webrtc::DataChannelInterface::kConnecting, data_channel2->state());
  // Existing data channels should open once the transport channel is writable.
  ConnectTransportChannels();
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, data_channel1->state(),
                 kTimeoutMs);
  EXPECT_EQ_WAIT(webrtc::DataChannelInterface::kOpen, data_channel2->state(),
                 kTimeoutMs);
  // Any data channels created afterwards should start in state kOpen.
  webrtc::DataChannelInit config3;
  config3.id = 21;
  rtc::scoped_refptr<DataChannelInterface> data_channel3 =
      peer2_.CreateDataChannel(&config3);
  EXPECT_EQ(webrtc::DataChannelInterface::kOpen, data_channel3->state());
}

// Tests that the QuicTransport dispatches messages for one QuicDataChannel.
TEST_F(QuicDataTransportTest, ReceiveMessagesForSingleDataChannel) {
  ConnectTransportChannels();

  int data_channel_id = 1337;
  webrtc::DataChannelInit config;
  config.id = data_channel_id;
  rtc::scoped_refptr<DataChannelInterface> peer2_data_channel =
      peer2_.CreateDataChannel(&config);
  FakeObserver observer;
  peer2_data_channel->RegisterObserver(&observer);

  uint64_t message1_id = 26u;
  peer1_.WriteMessage(data_channel_id, message1_id, "Testing");
  ASSERT_EQ_WAIT(1, observer.messages_received(), kTimeoutMs);
  EXPECT_EQ("Testing", observer.messages()[0]);

  uint64_t message2_id = 402u;
  peer1_.WriteMessage(data_channel_id, message2_id, "Hello, World!");
  ASSERT_EQ_WAIT(2, observer.messages_received(), kTimeoutMs);
  EXPECT_EQ("Hello, World!", observer.messages()[1]);

  uint64_t message3_id = 100260415u;
  peer1_.WriteMessage(data_channel_id, message3_id, "Third message");
  ASSERT_EQ_WAIT(3, observer.messages_received(), kTimeoutMs);
  EXPECT_EQ("Third message", observer.messages()[2]);
}

// Tests that the QuicTransport dispatches messages to the correct data channel
// when multiple are in use.
TEST_F(QuicDataTransportTest, ReceiveMessagesForMultipleDataChannels) {
  ConnectTransportChannels();

  std::vector<rtc::scoped_refptr<DataChannelInterface>> data_channels;
  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    webrtc::DataChannelInit config;
    config.id = data_channel_id;
    data_channels.push_back(peer2_.CreateDataChannel(&config));
  }

  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    uint64_t message1_id = 48023u;
    FakeObserver observer;
    DataChannelInterface* peer2_data_channel =
        data_channels[data_channel_id].get();
    peer2_data_channel->RegisterObserver(&observer);
    peer1_.WriteMessage(data_channel_id, message1_id, "Testing");
    ASSERT_EQ_WAIT(1, observer.messages_received(), kTimeoutMs);
    EXPECT_EQ("Testing", observer.messages()[0]);

    uint64_t message2_id = 1372643095u;
    peer1_.WriteMessage(data_channel_id, message2_id, "Hello, World!");
    ASSERT_EQ_WAIT(2, observer.messages_received(), kTimeoutMs);
    EXPECT_EQ("Hello, World!", observer.messages()[1]);
  }
}

// Tests end-to-end that both peers can use multiple QuicDataChannels to
// send/receive messages using a QuicDataTransport.
TEST_F(QuicDataTransportTest, EndToEndSendReceiveMessages) {
  ConnectTransportChannels();

  std::vector<rtc::scoped_refptr<DataChannelInterface>> peer1_data_channels;
  std::vector<rtc::scoped_refptr<DataChannelInterface>> peer2_data_channels;

  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    webrtc::DataChannelInit config;
    config.id = data_channel_id;
    peer1_data_channels.push_back(peer1_.CreateDataChannel(&config));
    peer2_data_channels.push_back(peer2_.CreateDataChannel(&config));
  }

  for (int data_channel_id = 0; data_channel_id < 5; ++data_channel_id) {
    DataChannelInterface* peer1_data_channel =
        peer1_data_channels[data_channel_id].get();
    FakeObserver observer1;
    peer1_data_channel->RegisterObserver(&observer1);
    DataChannelInterface* peer2_data_channel =
        peer2_data_channels[data_channel_id].get();
    FakeObserver observer2;
    peer2_data_channel->RegisterObserver(&observer2);

    peer1_data_channel->Send(webrtc::DataBuffer("Peer 1 message 1"));
    ASSERT_EQ_WAIT(1, observer2.messages_received(), kTimeoutMs);
    EXPECT_EQ("Peer 1 message 1", observer2.messages()[0]);

    peer1_data_channel->Send(webrtc::DataBuffer("Peer 1 message 2"));
    ASSERT_EQ_WAIT(2, observer2.messages_received(), kTimeoutMs);
    EXPECT_EQ("Peer 1 message 2", observer2.messages()[1]);

    peer2_data_channel->Send(webrtc::DataBuffer("Peer 2 message 1"));
    ASSERT_EQ_WAIT(1, observer1.messages_received(), kTimeoutMs);
    EXPECT_EQ("Peer 2 message 1", observer1.messages()[0]);

    peer2_data_channel->Send(webrtc::DataBuffer("Peer 2 message 2"));
    ASSERT_EQ_WAIT(2, observer1.messages_received(), kTimeoutMs);
    EXPECT_EQ("Peer 2 message 2", observer1.messages()[1]);
  }
}

// Tests that SetTransport returns false when setting a transport that is not
// equivalent to the one already set.
TEST_F(QuicDataTransportTest, SetTransportReturnValue) {
  QuicDataTransport* quic_data_transport = peer1_.quic_data_transport();
  // Ignore the same transport name.
  EXPECT_TRUE(quic_data_transport->SetTransport(kTransportName));
  // Return false when setting a different transport name.
  EXPECT_FALSE(quic_data_transport->SetTransport("another transport name"));
}

}  // namespace
