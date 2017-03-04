/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/bind.h"
#include "webrtc/base/copyonwritebuffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/sctp/sctptransport.h"
#include "webrtc/p2p/base/fakedtlstransport.h"

namespace {
static const int kDefaultTimeout = 10000;  // 10 seconds.
// Use ports other than the default 5000 for testing.
static const int kTransport1Port = 5001;
static const int kTransport2Port = 5002;
}

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
  }

  void OnDataReceived(const ReceiveDataParams& params,
                      const rtc::CopyOnWriteBuffer& data) {
    received_ = true;
    last_data_ = std::string(data.data<char>(), data.size());
    last_params_ = params;
  }

  bool received() const { return received_; }
  std::string last_data() const { return last_data_; }
  ReceiveDataParams last_params() const { return last_params_; }

 private:
  bool received_;
  std::string last_data_;
  ReceiveDataParams last_params_;
};

class SignalReadyToSendObserver : public sigslot::has_slots<> {
 public:
  SignalReadyToSendObserver() : signaled_(false) {}

  void OnSignaled() { signaled_ = true; }

  bool IsSignaled() { return signaled_; }

 private:
  bool signaled_;
};

class SignalTransportClosedObserver : public sigslot::has_slots<> {
 public:
  SignalTransportClosedObserver() {}
  void BindSelf(SctpTransport* transport) {
    transport->SignalStreamClosedRemotely.connect(
        this, &SignalTransportClosedObserver::OnStreamClosed);
  }
  void OnStreamClosed(int stream) { streams_.push_back(stream); }

  int StreamCloseCount(int stream) {
    return std::count(streams_.begin(), streams_.end(), stream);
  }

  bool WasStreamClosed(int stream) {
    return std::find(streams_.begin(), streams_.end(), stream) !=
           streams_.end();
  }

 private:
  std::vector<int> streams_;
};

class SignalTransportClosedReopener : public sigslot::has_slots<> {
 public:
  SignalTransportClosedReopener(SctpTransport* transport, SctpTransport* peer)
      : transport_(transport), peer_(peer) {}

  void OnStreamClosed(int stream) {
    transport_->OpenStream(stream);
    peer_->OpenStream(stream);
    streams_.push_back(stream);
  }

  int StreamCloseCount(int stream) {
    return std::count(streams_.begin(), streams_.end(), stream);
  }

 private:
  SctpTransport* transport_;
  SctpTransport* peer_;
  std::vector<int> streams_;
};

// SCTP Data Engine testing framework.
class SctpTransportTest : public testing::Test, public sigslot::has_slots<> {
 protected:
  // usrsctp uses the NSS random number generator on non-Android platforms,
  // so we need to initialize SSL.
  static void SetUpTestCase() {}

  void SetupConnectedTransportsWithTwoStreams() {
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

    LOG(LS_VERBOSE) << "Transport setup ----------------------------- ";
    AddStream(1);
    AddStream(2);

    LOG(LS_VERBOSE) << "Connect the transports -----------------------------";
    // Both transports need to have started (with matching ports) for an
    // association to be formed.
    transport1_->Start(kTransport1Port, kTransport2Port);
    transport2_->Start(kTransport2Port, kTransport1Port);
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
                SendDataResult* result) {
    SendDataParams params;
    params.sid = sid;

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

// Test that data can be sent end-to-end when an SCTP transport starts with one
// transport channel (which is unwritable), and then switches to another
// channel. A common scenario due to how BUNDLE works.
TEST_F(SctpTransportTest, SwitchTransportChannel) {
  FakeDtlsTransport black_hole("black hole", 0);
  FakeDtlsTransport fake_dtls1("fake dtls 1", 0);
  FakeDtlsTransport fake_dtls2("fake dtls 2", 0);
  SctpFakeDataReceiver recv1;
  SctpFakeDataReceiver recv2;

  // Construct transport1 with the "black hole" channel.
  std::unique_ptr<SctpTransport> transport1(
      CreateTransport(&black_hole, &recv1));
  std::unique_ptr<SctpTransport> transport2(
      CreateTransport(&fake_dtls2, &recv2));

  // Add a stream.
  transport1->OpenStream(1);
  transport2->OpenStream(1);

  // Tell them both to start (though transport1_ is connected to black_hole).
  transport1->Start(kTransport1Port, kTransport2Port);
  transport2->Start(kTransport2Port, kTransport1Port);

  // Switch transport1_ to the normal fake_dtls1_ channel.
  transport1->SetTransportChannel(&fake_dtls1);

  // Connect the two fake DTLS channels.
  bool asymmetric = false;
  fake_dtls1.SetDestination(&fake_dtls2, asymmetric);

  // Make sure we end up able to send data.
  SendDataResult result;
  ASSERT_TRUE(SendData(transport1.get(), 1, "foo", &result));
  ASSERT_TRUE(SendData(transport2.get(), 1, "bar", &result));
  EXPECT_TRUE_WAIT(ReceivedData(&recv2, 1, "foo"), kDefaultTimeout);
  EXPECT_TRUE_WAIT(ReceivedData(&recv1, 1, "bar"), kDefaultTimeout);
}

// Calling Start twice shouldn't do anything bad, if with the same parameters.
TEST_F(SctpTransportTest, DuplicateStartCallsIgnored) {
  SetupConnectedTransportsWithTwoStreams();
  EXPECT_TRUE(transport1()->Start(kTransport1Port, kTransport2Port));

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
  EXPECT_FALSE(transport1()->Start(kTransport1Port, 1234));
  EXPECT_FALSE(transport1()->Start(1234, kTransport2Port));
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
  transport1->Start(kSctpDefaultPort, kSctpDefaultPort);
  transport2->Start(-1, -1);

  // Connect the two fake DTLS channels.
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
// DTLS channel is writable.
TEST_F(SctpTransportTest, SignalReadyToSendDataAfterDtlsWritable) {
  FakeDtlsTransport fake_dtls("fake dtls", 0);
  SctpFakeDataReceiver recv;
  std::unique_ptr<SctpTransport> transport(CreateTransport(&fake_dtls, &recv));

  SignalReadyToSendObserver signal_observer;
  transport->SignalReadyToSendData.connect(
      &signal_observer, &SignalReadyToSendObserver::OnSignaled);

  transport->Start(kSctpDefaultPort, kSctpDefaultPort);
  fake_dtls.SetWritable(true);
  EXPECT_TRUE_WAIT(signal_observer.IsSignaled(), kDefaultTimeout);
}

// Test that after an SCTP socket's buffer is filled, SignalReadyToSendData
// is fired after it begins to be drained.
TEST_F(SctpTransportTest, SignalReadyToSendDataAfterBlocked) {
  SetupConnectedTransportsWithTwoStreams();
  // Wait for initial SCTP association to be formed.
  EXPECT_EQ_WAIT(1, transport1_ready_to_send_count(), kDefaultTimeout);
  // Make the fake transport unwritable so that messages pile up for the SCTP
  // socket.
  fake_dtls1()->SetWritable(false);
  // Send messages until we get EWOULDBLOCK.
  static const int kMaxMessages = 1024;
  SendDataParams params;
  params.sid = 1;
  rtc::CopyOnWriteBuffer buf(1024);
  memset(buf.data<uint8_t>(), 0, 1024);
  SendDataResult result;
  int message_count;
  for (message_count = 0; message_count < kMaxMessages; ++message_count) {
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
}

TEST_F(SctpTransportTest, SendData) {
  SetupConnectedTransportsWithTwoStreams();

  SendDataResult result;
  LOG(LS_VERBOSE)
      << "transport1 sending: 'hello?' -----------------------------";
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  LOG(LS_VERBOSE) << "recv2.received=" << receiver2()->received()
                  << ", recv2.last_params.sid="
                  << receiver2()->last_params().sid
                  << ", recv2.last_params.timestamp="
                  << receiver2()->last_params().timestamp
                  << ", recv2.last_params.seq_num="
                  << receiver2()->last_params().seq_num
                  << ", recv2.last_data=" << receiver2()->last_data();

  LOG(LS_VERBOSE)
      << "transport2 sending: 'hi transport1' -----------------------------";
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);
  LOG(LS_VERBOSE) << "recv1.received=" << receiver1()->received()
                  << ", recv1.last_params.sid="
                  << receiver1()->last_params().sid
                  << ", recv1.last_params.timestamp="
                  << receiver1()->last_params().timestamp
                  << ", recv1.last_params.seq_num="
                  << receiver1()->last_params().seq_num
                  << ", recv1.last_data=" << receiver1()->last_data();
}

// Sends a lot of large messages at once and verifies SDR_BLOCK is returned.
TEST_F(SctpTransportTest, SendDataBlocked) {
  SetupConnectedTransportsWithTwoStreams();

  SendDataResult result;
  SendDataParams params;
  params.sid = 1;

  std::vector<char> buffer(1024 * 64, 0);

  for (size_t i = 0; i < 100; ++i) {
    transport1()->SendData(
        params, rtc::CopyOnWriteBuffer(&buffer[0], buffer.size()), &result);
    if (result == SDR_BLOCK)
      break;
  }

  EXPECT_EQ(SDR_BLOCK, result);
}

// Trying to send data for a nonexistent stream should fail.
TEST_F(SctpTransportTest, SendDataWithNonexistentStreamFails) {
  SetupConnectedTransportsWithTwoStreams();
  SendDataResult result;
  EXPECT_FALSE(SendData(transport2(), 123, "some data", &result));
  EXPECT_EQ(SDR_ERROR, result);
}

TEST_F(SctpTransportTest, ClosesRemoteStream) {
  SetupConnectedTransportsWithTwoStreams();
  SignalTransportClosedObserver transport1_sig_receiver,
      transport2_sig_receiver;
  transport1_sig_receiver.BindSelf(transport1());
  transport2_sig_receiver.BindSelf(transport2());

  SendDataResult result;
  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);
  ASSERT_TRUE(SendData(transport2(), 2, "hi transport1", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver1(), 2, "hi transport1"),
                   kDefaultTimeout);

  // Close transport 1.  Transport 2 should notify us.
  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_sig_receiver.WasStreamClosed(1), kDefaultTimeout);
}

TEST_F(SctpTransportTest, ClosesTwoRemoteStreams) {
  SetupConnectedTransportsWithTwoStreams();
  AddStream(3);
  SignalTransportClosedObserver transport1_sig_receiver,
      transport2_sig_receiver;
  transport1_sig_receiver.BindSelf(transport1());
  transport2_sig_receiver.BindSelf(transport2());

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
  EXPECT_TRUE_WAIT(transport1_sig_receiver.WasStreamClosed(2), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_sig_receiver.WasStreamClosed(3), kDefaultTimeout);
}

TEST_F(SctpTransportTest, ClosesStreamsOnBothSides) {
  SetupConnectedTransportsWithTwoStreams();
  AddStream(3);
  AddStream(4);
  SignalTransportClosedObserver transport1_sig_receiver,
      transport2_sig_receiver;
  transport1_sig_receiver.BindSelf(transport1());
  transport2_sig_receiver.BindSelf(transport2());

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
  EXPECT_TRUE_WAIT(transport2_sig_receiver.WasStreamClosed(1), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_sig_receiver.WasStreamClosed(2), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_sig_receiver.WasStreamClosed(3), kDefaultTimeout);
  EXPECT_TRUE_WAIT(transport1_sig_receiver.WasStreamClosed(4), kDefaultTimeout);
}

TEST_F(SctpTransportTest, RefusesHighNumberedTransports) {
  SetupConnectedTransportsWithTwoStreams();
  EXPECT_TRUE(AddStream(kMaxSctpSid));
  EXPECT_FALSE(AddStream(kMaxSctpSid + 1));
}

// Flaky, see webrtc:4453.
TEST_F(SctpTransportTest, DISABLED_ReusesAStream) {
  // Shut down transport 1, then open it up again for reuse.
  SetupConnectedTransportsWithTwoStreams();
  SendDataResult result;
  SignalTransportClosedObserver transport2_sig_receiver;
  transport2_sig_receiver.BindSelf(transport2());

  ASSERT_TRUE(SendData(transport1(), 1, "hello?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hello?"), kDefaultTimeout);

  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_sig_receiver.WasStreamClosed(1), kDefaultTimeout);
  // Transport 1 is gone now.

  // Create a new transport 1.
  AddStream(1);
  ASSERT_TRUE(SendData(transport1(), 1, "hi?", &result));
  EXPECT_EQ(SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(receiver2(), 1, "hi?"), kDefaultTimeout);
  transport1()->ResetStream(1);
  EXPECT_TRUE_WAIT(transport2_sig_receiver.StreamCloseCount(1) == 2,
                   kDefaultTimeout);
}

}  // namespace cricket
