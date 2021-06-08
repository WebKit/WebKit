/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/sctp_transport.h"

#include <stdio.h>
#include <string.h>
#include <usrsctp.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/fake_dtls_transport.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace {
static const int kDefaultTimeout = 10000;  // 10 seconds.
// Use ports other than the default 5000 for testing.
static const int kTransport1Port = 5001;
static const int kTransport2Port = 5002;
}  // namespace

namespace cricket {

// This is essentially a buffer to hold recieved data. It stores only the last
// received data. Calling OnDataReceived twice overwrites old data with the
// newer one.
// TODO(ldixon): Implement constraints, and allow new data to be added to old
// instead of replacing it.
class SctpFakeDataReceiver : public sigslot::has_slots<> {
 public:
  SctpFakeDataReceiver() : received_(false) {}

  void Clear() {
    received_ = false;
    last_data_ = "";
    last_params_ = ReceiveDataParams();
    num_messages_received_ = 0;
  }

  void OnDataReceived(const ReceiveDataParams& params,
                      const rtc::CopyOnWriteBuffer& data) {
    num_messages_received_++;
    received_ = true;
    last_data_ = std::string(data.data<char>(), data.size());
    last_params_ = params;
  }

  bool received() const { return received_; }
  std::string last_data() const { return last_data_; }
  ReceiveDataParams last_params() const { return last_params_; }
  size_t num_messages_received() const { return num_messages_received_; }

 private:
  bool received_;
  std::string last_data_;
  size_t num_messages_received_ = 0;
  ReceiveDataParams last_params_;
};

class SctpTransportObserver : public sigslot::has_slots<> {
 public:
  explicit SctpTransportObserver(SctpTransport* transport) {
    transport->SignalClosingProcedureComplete.connect(
        this, &SctpTransportObserver::OnClosingProcedureComplete);
    transport->SignalReadyToSendData.connect(
        this, &SctpTransportObserver::OnReadyToSend);
  }

  int StreamCloseCount(int stream) {
    return absl::c_count(closed_streams_, stream);
  }

  bool WasStreamClosed(int stream) {
    return absl::c_linear_search(closed_streams_, stream);
  }

  bool ReadyToSend() { return ready_to_send_; }

 private:
  void OnClosingProcedureComplete(int stream) {
    closed_streams_.push_back(stream);
  }
  void OnReadyToSend() { ready_to_send_ = true; }

  std::vector<int> closed_streams_;
  bool ready_to_send_ = false;
};

// Helper class used to immediately attempt to reopen a stream as soon as it's
// been closed.
class SignalTransportClosedReopener : public sigslot::has_slots<> {
 public:
  SignalTransportClosedReopener(SctpTransport* transport, SctpTransport* peer)
      : transport_(transport), peer_(peer) {}

  int StreamCloseCount(int stream) { return absl::c_count(streams_, stream); }

 private:
  void OnStreamClosed(int stream) {
    transport_->OpenStream(stream);
    peer_->OpenStream(stream);
    streams_.push_back(stream);
  }

  SctpTransport* transport_;
  SctpTransport* peer_;
  std::vector<int> streams_;
};

// SCTP Data Engine testing framework.
class SctpTransportTest : public ::testing::Test, public sigslot::has_slots<> {
 protected:
  // usrsctp uses the NSS random number generator on non-Android platforms,
  // so we need to initialize SSL.
  static void SetUpTestSuite() {}

  void SetupConnectedTransportsWithTwoStreams() {
    SetupConnectedTransportsWithTwoStreams(kTransport1Port, kTransport2Port);
  }

  void SetupConnectedTransportsWithTwoStreams(int port1, int port2) {
    fake_dtls1_.reset(new FakeDtlsTransport("fake dtls 1", 0));
    fake_dtls2_.reset(new FakeDtlsTransport("fake dtls 2", 0));
    recv1_.reset(new SctpFakeDataReceiver());
    recv2_.reset(new SctpFakeDataReceiver());
    transport1_.reset(CreateTransport(fake_dtls1_.get(), recv1_.get()));
    transport1_->set_debug_name_for_testing("transport1");
    transport1_->SignalReadyToSendData.connect(
        this, &SctpTransportTest::OnChan1ReadyToSend);
    transport2_.reset(CreateTransport(fake_dtls2_.get(), recv2_.get()));
    transport2_->set_debug_name_for_testing("transport2");
    transport2_->SignalReadyToSendData.connect(
        this, &SctpTransportTest::OnChan2ReadyToSend);
    // Setup two connected transports ready to send and receive.
    bool asymmetric = false;
    fake_dtls1_->SetDestination(fake_dtls2_.get(), asymmetric);

    RTC_LOG(LS_VERBOSE) << "Transport setup ----------------------------- ";
    AddStream(1);
    AddStream(2);

    RTC_LOG(LS_VERBOSE)
        << "Connect the transports -----------------------------";
    // Both transports need to have started (with matching ports) for an
    // association to be formed.
    transport1_->Start(port1, port2, kSctpSendBufferSize);
    transport2_->Start(port2, port1, kSctpSendBufferSize);
  }

  bool AddStream(int sid) {
    bool ret = true;
    ret = ret && transport1_->OpenStream(sid);
    ret = ret && transport2_->OpenStream(sid);
    return ret;
  }

  SctpTransport* CreateTransport(FakeDtlsTransport* fake_dtls,
                                 SctpFakeDataReceiver* recv) {
    SctpTransport* transport =
        new SctpTransport(rtc::Thread::Current(), fake_dtls);
    // When data is received, pass it to the SctpFakeDataReceiver.
    transport->SignalDataReceived.connect(
        recv, &SctpFakeDataReceiver::OnDataReceived);
    return transport;
  }

  bool SendData(SctpTransport* chan,
                int sid,
                const std::string& msg,
                SendDataResult* result,
                bool ordered = false) {
    SendDataParams params;
    params.sid = sid;
    params.ordered = ordered;

    return chan->SendData(params, rtc::CopyOnWriteBuffer(&msg[0], msg.length()),
                          result);
  }

  bool ReceivedData(const SctpFakeDataReceiver* recv,
                    int sid,
                    const std::string& msg) {
    return (recv->received() && recv->last_params().sid == sid &&
            recv->last_data() == msg);
  }

  bool ProcessMessagesUntilIdle() {
    rtc::Thread* thread = rtc::Thread::Current();
    while (!thread->empty()) {
      rtc::Message msg;
      if (thread->Get(&msg, rtc::Thread::kForever)) {
        thread->Dispatch(&msg);
      }
    }
    return !thread->IsQuitting();
  }

  SctpTransport* transport1() { return transport1_.get(); }
  SctpTransport* transport2() { return transport2_.get(); }
  SctpFakeDataReceiver* receiver1() { return recv1_.get(); }
  SctpFakeDataReceiver* receiver2() { return recv2_.get(); }
  FakeDtlsTransport* fake_dtls1() { return fake_dtls1_.get(); }
  FakeDtlsTransport* fake_dtls2() { return fake_dtls2_.get(); }

  int transport1_ready_to_send_count() {
    return transport1_ready_to_send_count_;
  }
  int transport2_ready_to_send_count() {
    return transport2_ready_to_send_count_;
  }

 private:
  std::unique_ptr<FakeDtlsTransport> fake_dtls1_;
  std::unique_ptr<FakeDtlsTransport> fake_dtls2_;
  std::unique_ptr<SctpFakeDataReceiver> recv1_;
  std::unique_ptr<SctpFakeDataReceiver> recv2_;
  std::unique_ptr<SctpTransport> transport1_;
  std::unique_ptr<SctpTransport> transport2_;

  int transport1_ready_to_send_count_ = 0;
  int transport2_ready_to_send_count_ = 0;

  void OnChan1ReadyToSend() { ++transport1_ready_to_send_count_; }
  void OnChan2ReadyToSend() { ++transport2_ready_to_send_count_; }
};

TEST_F(SctpTransportTest, MessageInterleavedWithNotification) {
  FakeDtlsTransport fake_dtls1("fake dtls 1", 0);
  FakeDtlsTransport fake_dtls2("fake dtls 2", 0);
  SctpFakeDataReceiver recv1;
  SctpFakeDataReceiver recv2;
  std::unique_ptr<SctpTransport> transport1(
      CreateTransport(&fake_dtls1, &recv1));
  std::unique_ptr<SctpTransport> transport2(
      CreateTransport(&fake_dtls2, &recv2));

  // Add a stream.
  transport1->OpenStream(1);
  transport2->OpenStream(1);

  // Start SCTP transports.
  transport1->Start(kSctpDefaultPort, kSctpDefaultPort, kSctpSendBufferSize);
  transport2->Start(kSctpDefaultPort, kSctpDefaultPort, kSctpSendBufferSize);

  // Connect the two fake DTLS transports.
  fake_dtls1.SetDestination(&fake_dtls2, false);

  // Ensure the SCTP association has been established
  // Note: I'd rather watch for an assoc established state here but couldn't
  //       find any exposed...
  SendDataResult result;
  ASSERT_TRUE(SendData(transport2.get(), 1, "meow", &result));
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "meow"), kDefaultTimeout);

  // Detach the DTLS transport to ensure only we will inject packets from here
  // on.
  transport1->SetDtlsTransport(nullptr);

  // Prepare chunk buffer and metadata
  auto chunk = rtc::CopyOnWriteBuffer(32);
  struct sctp_rcvinfo meta = {0};
  meta.rcv_sid = 1;
  meta.rcv_ssn = 1337;
  meta.rcv_ppid = rtc::HostToNetwork32(51);  // text (complete)

  // Inject chunk 1/2.
  meta.rcv_tsn = 42;
  meta.rcv_cumtsn = 42;
  chunk.SetData("meow?", 5);
  EXPECT_EQ(1, transport1->InjectDataOrNotificationFromSctpForTesting(
                   chunk.data(), chunk.size(), meta, 0));

  // Inject a notification in between chunks.
  union sctp_notification notification;
  memset(&notification, 0, sizeof(notification));
  // Type chosen since it's not handled apart from being logged
  notification.sn_header.sn_type = SCTP_PEER_ADDR_CHANGE;
  notification.sn_header.sn_flags = 0;
  notification.sn_header.sn_length = sizeof(notification);
  EXPECT_EQ(1, transport1->InjectDataOrNotificationFromSctpForTesting(
                   &notification, sizeof(notification), {0}, MSG_NOTIFICATION));

  // Inject chunk 2/2
  meta.rcv_tsn = 42;
  meta.rcv_cumtsn = 43;
  chunk.SetData(" rawr!", 6);
  EXPECT_EQ(1, transport1->InjectDataOrNotificationFromSctpForTesting(
                   chunk.data(), chunk.size(), meta, MSG_EOR));

  // Expect the message to contain both chunks.
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "meow? rawr!"), kDefaultTimeout);
}

// Test that data can be sent end-to-end when an SCTP transport starts with one
// transport (which is unwritable), and then switches to another transport. A
// common scenario due to how BUNDLE works.
TEST_F(SctpTransportTest, SwitchDtlsTransport) {
  FakeDtlsTransport black_hole("black hole", 0);
  FakeDtlsTransport fake_dtls1("fake dtls 1", 0);
  FakeDtlsTransport fake_dtls2("fake dtls 2", 0);
  SctpFakeDataReceiver recv1;
  SctpFakeDataReceiver recv2;

  // Construct transport1 with the "black hole" transport.
  std::unique_ptr<SctpTransport> transport1(
      CreateTransport(&black_hole, &recv1));
  std::unique_ptr<SctpTransport> transport2(
      CreateTransport(&fake_dtls2, &recv2));

  // Add a stream.
  transport1->OpenStream(1);
  transport2->OpenStream(1);

  // Tell them both to start (though transport1_ is connected to black_hole).
  transport1->Start(kTransport1Port, kTransport2Port, kSctpSendBufferSize);
  transport2->Start(kTransport2Port, kTransport1Port, kSctpSendBufferSize);

  // Switch transport1_ to the normal fake_dtls1_ transport.
  transport1->SetDtlsTransport(&fake_dtls1);

  // Connect the two fake DTLS transports.
  bool asymmetric = false;
  fake_dtls1.SetDestination(&fake_dtls2, asymmetric);

  // Make sure we end up able to send data.
  SendDataResult result;
  ASSERT_TRUE(SendData(transport1.get(), 1, "foo", &result));
  ASSERT_TRUE(SendData(transport2.get(), 1, "bar", &result));
  EXPECT_TRUE_WAIT(ReceivedData(&recv2, 1, "foo"), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "bar"), kDefaultTimeout);

  // Setting a null DtlsTransport should work. This could happen when an SCTP
  // data section is rejected.
  transport1->SetDtlsTransport(nullptr);
}

// Calling Start twice shouldn't do anything bad, if with the same parameters.
TEST_F(SctpTransportTest, DuplicateStartCallsIgnored) {
  SetupConnectedTransportsWithTwoStreams();
  EXPECT_TRUE(transport1()->Start(kTransport1Port, kTransport2Port,
                                  kSctpSendBufferSize));

  // Make sure we can still send/recv data.
  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "foo", &result));
  ASSERT_TRUE(SendData(transport2(), 1, "bar", &result));
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "foo"), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 1, "bar"), kDefaultTimeout);
}

// Calling Start a second time with a different port should fail.
TEST_F(SctpTransportTest, CallingStartWithDifferentPortFails) {
  SetupConnectedTransportsWithTwoStreams();
  EXPECT_FALSE(transport1()->Start(kTransport1Port, 1234, kSctpSendBufferSize));
  EXPECT_FALSE(transport1()->Start(1234, kTransport2Port, kSctpSendBufferSize));
}

// A value of -1 for the local/remote port should be treated as the default
// (5000).
TEST_F(SctpTransportTest, NegativeOnePortTreatedAsDefault) {
  FakeDtlsTransport fake_dtls1("fake dtls 1", 0);
  FakeDtlsTransport fake_dtls2("fake dtls 2", 0);
  SctpFakeDataReceiver recv1;
  SctpFakeDataReceiver recv2;
  std::unique_ptr<SctpTransport> transport1(
      CreateTransport(&fake_dtls1, &recv1));
  std::unique_ptr<SctpTransport> transport2(
      CreateTransport(&fake_dtls2, &recv2));

  // Add a stream.
  transport1->OpenStream(1);
  transport2->OpenStream(1);

  // Tell them both to start, giving one transport the default port and the
  // other transport -1.
  transport1->Start(kSctpDefaultPort, kSctpDefaultPort, kSctpSendBufferSize);
  transport2->Start(-1, -1, kSctpSendBufferSize);

  // Connect the two fake DTLS transports.
  bool asymmetric = false;
  fake_dtls1.SetDestination(&fake_dtls2, asymmetric);

  // Make sure we end up able to send data.
  SendDataResult result;
  ASSERT_TRUE(SendData(transport1.get(), 1, "foo", &result));
  ASSERT_TRUE(SendData(transport2.get(), 1, "bar", &result));
  EXPECT_TRUE_WAIT(ReceivedData(&recv2, 1, "foo"), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "bar"), kDefaultTimeout);
}

TEST_F(SctpTransportTest, OpenStreamWithAlreadyOpenedStreamFails) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));
  EXPECT_TRUE(transport->OpenStream(1));
  EXPECT_FALSE(transport->OpenStream(1));
}

TEST_F(SctpTransportTest, ResetStreamWithAlreadyResetStreamFails) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));
  EXPECT_TRUE(transport->OpenStream(1));
  EXPECT_TRUE(transport->ResetStream(1));
  EXPECT_FALSE(transport->ResetStream(1));
}

// Test that SignalReadyToSendData is fired after Start has been called and the
// DTLS transport is writable.
TEST_F(SctpTransportTest, SignalReadyToSendDataAfterDtlsWritable) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));
  SctpTransportObserver observer(transport.get());

  transport->Start(kSctpDefaultPort, kSctpDefaultPort, kSctpSendBufferSize);
  fake_dtls.SetWritable(true);
  EXPECT_TRUE_WAIT(observer.ReadyToSend(), kDefaultTimeout);
}

// Run the below tests using both ordered and unordered mode.
class SctpTransportTestWithOrdered
    : public SctpTransportTest,
      public ::testing::WithParamInterface<bool> {};

// Tests that a small message gets buffered and later sent by the SctpTransport
// when the sctp library only accepts the message partially.
TEST_P(SctpTransportTestWithOrdered, SendSmallBufferedOutgoingMessage) {
  bool ordered = GetParam();
  SetupConnectedTransportsWithTwoStreams();
  // Wait for initial SCTP association to be formed.
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  // Make the fake transport unwritable so that messages pile up for the SCTP
  // socket.
  fake_dtls1()->SetWritable(false);
  SendDataResult result;

  // Fill almost all of sctp library's send buffer.
  ASSERT_TRUE(SendData(transport1(), /*sid=*/1,
                       std::string(kSctpSendBufferSize - 1, 'a'), &result,
                       ordered));

  std::string buffered_message("hello hello");
  // SctpTransport accepts this message by buffering part of it.
  ASSERT_TRUE(
      SendData(transport1(), /*sid=*/1, buffered_message, &result, ordered));
  ASSERT_TRUE(transport1()->ReadyToSendData());

  // Sending anything else should block now.
  ASSERT_FALSE(
      SendData(transport1(), /*sid=*/1, "hello again", &result, ordered));
  ASSERT_EQ(SDR_BLOCK, result);
  ASSERT_FALSE(transport1()->ReadyToSendData());

  // Make sure the ready-to-send count hasn't changed.
  EXPECT_EQ(1, transport1_ready_to_send_count());
  // Make the transport writable again and expect a "SignalReadyToSendData" at
  // some point after sending the buffered message.
  fake_dtls1()->SetWritable(true);
  EXPECT_EQ_WAIT(2, transport1_ready_to_send_count(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, buffered_message),
                   kDefaultTimeout);
  EXPECT_EQ(2u, receiver2()->num_messages_received());
}

// Tests that a large message gets buffered and later sent by the SctpTransport
// when the sctp library only accepts the message partially.
TEST_P(SctpTransportTestWithOrdered, SendLargeBufferedOutgoingMessage) {
  bool ordered = GetParam();
  SetupConnectedTransportsWithTwoStreams();
  // Wait for initial SCTP association to be formed.
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  // Make the fake transport unwritable so that messages pile up for the SCTP
  // socket.
  fake_dtls1()->SetWritable(false);
  SendDataResult result;

  // Fill almost all of sctp library's send buffer.
  ASSERT_TRUE(SendData(transport1(), /*sid=*/1,
                       std::string(kSctpSendBufferSize / 2, 'a'), &result,
                       ordered));

  std::string buffered_message(kSctpSendBufferSize, 'b');
  // SctpTransport accepts this message by buffering the second half.
  ASSERT_TRUE(
      SendData(transport1(), /*sid=*/1, buffered_message, &result, ordered));
  ASSERT_TRUE(transport1()->ReadyToSendData());

  // Sending anything else should block now.
  ASSERT_FALSE(
      SendData(transport1(), /*sid=*/1, "hello again", &result, ordered));
  ASSERT_EQ(SDR_BLOCK, result);
  ASSERT_FALSE(transport1()->ReadyToSendData());

  // Make sure the ready-to-send count hasn't changed.
  EXPECT_EQ(1, transport1_ready_to_send_count());
  // Make the transport writable again and expect a "SignalReadyToSendData" at
  // some point.
  fake_dtls1()->SetWritable(true);
  EXPECT_EQ_WAIT(2, transport1_ready_to_send_count(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, buffered_message),
                   kDefaultTimeout);
  EXPECT_EQ(2u, receiver2()->num_messages_received());
}

TEST_P(SctpTransportTestWithOrdered, SendData) {
  bool ordered = GetParam();
  SetupConnectedTransportsWithTwoStreams();

  SendDataResult result;
  RTC_LOG(LS_VERBOSE)
      << "transport1 sending: 'hello?' -----------------------------";
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result, ordered));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  RTC_LOG(LS_VERBOSE) << "recv2.received=" << receiver2()->received()
                      << ", recv2.last_params.sid="
                      << receiver2()->last_params().sid
                      << ", recv2.last_params.timestamp="
                      << receiver2()->last_params().timestamp
                      << ", recv2.last_params.seq_num="
                      << receiver2()->last_params().seq_num
                      << ", recv2.last_data=" << receiver2()->last_data();

  RTC_LOG(LS_VERBOSE)
      << "transport2 sending: 'hi transport1' -----------------------------";
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result, ordered));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);
  RTC_LOG(LS_VERBOSE) << "recv1.received=" << receiver1()->received()
                      << ", recv1.last_params.sid="
                      << receiver1()->last_params().sid
                      << ", recv1.last_params.timestamp="
                      << receiver1()->last_params().timestamp
                      << ", recv1.last_params.seq_num="
                      << receiver1()->last_params().seq_num
                      << ", recv1.last_data=" << receiver1()->last_data();
}

// Sends a lot of large messages at once and verifies SDR_BLOCK is returned.
TEST_P(SctpTransportTestWithOrdered, SendDataBlocked) {
  SetupConnectedTransportsWithTwoStreams();

  SendDataResult result;
  SendDataParams params;
  params.sid = 1;
  params.ordered = GetParam();

  std::vector<char> buffer(1024 * 64, 0);

  for (size_t i = 0; i < 100; ++i) {
    transport1()->SendData(
        params, rtc::CopyOnWriteBuffer(&buffer[0], buffer.size()), &result);
    if (result == SDR_BLOCK)
      break;
  }

  EXPECT_EQ(SDR_BLOCK, result);
}

// Test that after an SCTP socket's buffer is filled, SignalReadyToSendData
// is fired after it begins to be drained.
TEST_P(SctpTransportTestWithOrdered, SignalReadyToSendDataAfterBlocked) {
  SetupConnectedTransportsWithTwoStreams();
  // Wait for initial SCTP association to be formed.
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  // Make the fake transport unwritable so that messages pile up for the SCTP
  // socket.
  fake_dtls1()->SetWritable(false);
  // Send messages until we get EWOULDBLOCK.
  static const size_t kMaxMessages = 1024;
  SendDataParams params;
  params.sid = 1;
  params.ordered = GetParam();
  rtc::CopyOnWriteBuffer buf(1024);
  memset(buf.data<uint8_t>(), 0, 1024);
  SendDataResult result;
  size_t message_count = 0;
  for (; message_count < kMaxMessages; ++message_count) {
    if (!transport1()->SendData(params, buf, &result) && result == SDR_BLOCK) {
      break;
    }
  }
  ASSERT_NE(kMaxMessages, message_count)
      << "Sent max number of messages without getting SDR_BLOCK?";
  // Make sure the ready-to-send count hasn't changed.
  EXPECT_EQ(1, transport1_ready_to_send_count());
  // Make the transport writable again and expect a "SignalReadyToSendData" at
  // some point.
  fake_dtls1()->SetWritable(true);
  EXPECT_EQ_WAIT(2, transport1_ready_to_send_count(), kDefaultTimeout);
  EXPECT_EQ_WAIT(message_count, receiver2()->num_messages_received(),
                 kDefaultTimeout);
}

INSTANTIATE_TEST_SUITE_P(SctpTransportTest,
                         SctpTransportTestWithOrdered,
                         ::testing::Bool());

// This is a regression test that fails with earlier versions of SCTP in
// unordered mode. See bugs.webrtc.org/10939.
TEST_F(SctpTransportTest, SendsLargeDataBufferedBySctpLib) {
  SetupConnectedTransportsWithTwoStreams();
  // Wait for initial SCTP association to be formed.
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  // Make the fake transport unwritable so that messages pile up for the SCTP
  // socket.
  fake_dtls1()->SetWritable(false);

  SendDataResult result;
  std::string buffered_message(kSctpSendBufferSize - 1, 'a');
  ASSERT_TRUE(SendData(transport1(), 1, buffered_message, &result, false));

  fake_dtls1()->SetWritable(true);
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, buffered_message),
                   kDefaultTimeout);
}

// Trying to send data for a nonexistent stream should fail.
TEST_F(SctpTransportTest, SendDataWithNonexistentStreamFails) {
  SetupConnectedTransportsWithTwoStreams();
  SendDataResult result;
  EXPECT_FALSE(SendData(transport2(), 123, "some data", &result));
  EXPECT_EQ(SDR_ERROR, result);
}

TEST_F(SctpTransportTest, SendDataHighPorts) {
  SetupConnectedTransportsWithTwoStreams(32768, 32769);

  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);

  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);
}

TEST_F(SctpTransportTest, ClosesRemoteStream) {
  SetupConnectedTransportsWithTwoStreams();
  SctpTransportObserver transport1_observer(transport1());
  SctpTransportObserver transport2_observer(transport2());

  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);

  // Close stream 1 on transport 1. Transport 2 should notify us.
  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(1), kDefaultTimeout);
}
TEST_F(SctpTransportTest, ClosesRemoteStreamWithNoData) {
  SetupConnectedTransportsWithTwoStreams();
  SctpTransportObserver transport1_observer(transport1());
  SctpTransportObserver transport2_observer(transport2());

  // Close stream 1 on transport 1. Transport 2 should notify us.
  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(1), kDefaultTimeout);
}

TEST_F(SctpTransportTest, ClosesTwoRemoteStreams) {
  SetupConnectedTransportsWithTwoStreams();
  AddStream(3);
  SctpTransportObserver transport1_observer(transport1());
  SctpTransportObserver transport2_observer(transport2());

  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);

  // Close two streams on one side.
  transport2()->ResetStream(2);
  transport2()->ResetStream(3);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(2), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(3), kDefaultTimeout);
}

TEST_F(SctpTransportTest, ClosesStreamsOnBothSides) {
  SetupConnectedTransportsWithTwoStreams();
  AddStream(3);
  AddStream(4);
  SctpTransportObserver transport1_observer(transport1());
  SctpTransportObserver transport2_observer(transport2());

  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);

  // Close one stream on transport1(), while closing three streams on
  // transport2().  They will conflict (only one side can close anything at a
  // time, apparently).  Test the resolution of the conflict.
  transport1()->ResetStream(1);

  transport2()->ResetStream(2);
  transport2()->ResetStream(3);
  transport2()->ResetStream(4);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(1), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_observer.WasStreamClosed(2), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_observer.WasStreamClosed(3), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_observer.WasStreamClosed(4), kDefaultTimeout);
}

TEST_F(SctpTransportTest, RefusesHighNumberedTransports) {
  SetupConnectedTransportsWithTwoStreams();
  EXPECT_TRUE(AddStream(kMaxSctpSid));
  EXPECT_FALSE(AddStream(kMaxSctpSid + 1));
}

TEST_F(SctpTransportTest, ReusesAStream) {
  // Shut down transport 1, then open it up again for reuse.
  SetupConnectedTransportsWithTwoStreams();
  SendDataResult result;
  SctpTransportObserver transport2_observer(transport2());

  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);

  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_observer.WasStreamClosed(1), kDefaultTimeout);
  // Transport 1 is gone now.

  // Create a new transport 1.
  AddStream(1);
  ASSERT_TRUE(SendData(transport1(), 1, "hi?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hi?"), kDefaultTimeout);
  transport1()->ResetStream(1);
  EXPECT_EQ_WAIT(2, transport2_observer.StreamCloseCount(1), kDefaultTimeout);
}

TEST_F(SctpTransportTest, RejectsTooLargeMessageSize) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));

  EXPECT_FALSE(transport->Start(kSctpDefaultPort, kSctpDefaultPort,
                                kSctpSendBufferSize + 1));
}

TEST_F(SctpTransportTest, RejectsTooSmallMessageSize) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));

  EXPECT_FALSE(transport->Start(kSctpDefaultPort, kSctpDefaultPort, 0));
}

TEST_F(SctpTransportTest, RejectsSendTooLargeMessages) {
  SetupConnectedTransportsWithTwoStreams();
  // Use "Start" to reduce the max message size
  transport1()->Start(kTransport1Port, kTransport2Port, 10);
  EXPECT_EQ(10, transport1()->max_message_size());
  const char eleven_characters[] = "12345678901";
  SendDataResult result;
  EXPECT_FALSE(SendData(transport1(), 1, eleven_characters, &result));
}

// Regression test for: crbug.com/1137936
TEST_F(SctpTransportTest, SctpRestartWithPendingDataDoesNotDeadlock) {
  // In order to trigger a restart, we'll connect two transports, then
  // disconnect them and connect the first to a third, which will initiate the
  // new handshake.
  FakeDtlsTransport fake_dtls1("fake dtls 1", 0);
  FakeDtlsTransport fake_dtls2("fake dtls 2", 0);
  FakeDtlsTransport fake_dtls3("fake dtls 3", 0);
  SctpFakeDataReceiver recv1;
  SctpFakeDataReceiver recv2;
  SctpFakeDataReceiver recv3;

  std::unique_ptr<SctpTransport> transport1(
      CreateTransport(&fake_dtls1, &recv1));
  std::unique_ptr<SctpTransport> transport2(
      CreateTransport(&fake_dtls2, &recv2));
  std::unique_ptr<SctpTransport> transport3(
      CreateTransport(&fake_dtls3, &recv3));
  SctpTransportObserver observer(transport1.get());

  // Connect the first two transports.
  fake_dtls1.SetDestination(&fake_dtls2, /*asymmetric=*/false);
  transport1->OpenStream(1);
  transport2->OpenStream(1);
  transport1->Start(5000, 5000, kSctpSendBufferSize);
  transport2->Start(5000, 5000, kSctpSendBufferSize);

  // Sanity check that we can send data.
  SendDataResult result;
  ASSERT_TRUE(SendData(transport1.get(), 1, "foo", &result));
  ASSERT_TRUE_WAIT(ReceivedData(&recv2, 1, "foo"), kDefaultTimeout);

  // Disconnect the transports and attempt to send a message, which will be
  // stored in an output queue; this is necessary to reproduce the bug.
  fake_dtls1.SetDestination(nullptr, /*asymmetric=*/false);
  EXPECT_TRUE(SendData(transport1.get(), 1, "bar", &result));

  // Now connect to the third transport.
  fake_dtls1.SetDestination(&fake_dtls3, /*asymmetric=*/false);
  transport3->OpenStream(1);
  transport3->Start(5000, 5000, kSctpSendBufferSize);

  // Send data from the new endpoint to the original endpoint. If data is
  // received that means the restart must have been successful.
  EXPECT_TRUE(SendData(transport3.get(), 1, "baz", &result));
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "baz"), kDefaultTimeout);
}

}  // namespace cricket
