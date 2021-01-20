/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/pseudo_tcp.h"

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/memory_stream.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

using cricket::PseudoTcp;

static const int kConnectTimeoutMs = 10000;  // ~3 * default RTO of 3000ms
static const int kTransferTimeoutMs = 15000;
static const int kBlockSize = 4096;

class PseudoTcpForTest : public cricket::PseudoTcp {
 public:
  PseudoTcpForTest(cricket::IPseudoTcpNotify* notify, uint32_t conv)
      : PseudoTcp(notify, conv) {}

  bool isReceiveBufferFull() const { return PseudoTcp::isReceiveBufferFull(); }

  void disableWindowScale() { PseudoTcp::disableWindowScale(); }
};

class PseudoTcpTestBase : public ::testing::Test,
                          public rtc::MessageHandlerAutoCleanup,
                          public cricket::IPseudoTcpNotify {
 public:
  PseudoTcpTestBase()
      : local_(this, 1),
        remote_(this, 1),
        have_connected_(false),
        have_disconnected_(false),
        local_mtu_(65535),
        remote_mtu_(65535),
        delay_(0),
        loss_(0) {
    // Set use of the test RNG to get predictable loss patterns. Otherwise,
    // this test would occasionally get really unlucky loss and time out.
    rtc::SetRandomTestMode(true);
  }
  ~PseudoTcpTestBase() {
    // Put it back for the next test.
    rtc::SetRandomTestMode(false);
  }
  // If true, both endpoints will send the "connect" segment simultaneously,
  // rather than |local_| sending it followed by a response from |remote_|.
  // Note that this is what chromoting ends up doing.
  void SetSimultaneousOpen(bool enabled) { simultaneous_open_ = enabled; }
  void SetLocalMtu(int mtu) {
    local_.NotifyMTU(mtu);
    local_mtu_ = mtu;
  }
  void SetRemoteMtu(int mtu) {
    remote_.NotifyMTU(mtu);
    remote_mtu_ = mtu;
  }
  void SetDelay(int delay) { delay_ = delay; }
  void SetLoss(int percent) { loss_ = percent; }
  // Used to cause the initial "connect" segment to be lost, needed for a
  // regression test.
  void DropNextPacket() { drop_next_packet_ = true; }
  void SetOptNagling(bool enable_nagles) {
    local_.SetOption(PseudoTcp::OPT_NODELAY, !enable_nagles);
    remote_.SetOption(PseudoTcp::OPT_NODELAY, !enable_nagles);
  }
  void SetOptAckDelay(int ack_delay) {
    local_.SetOption(PseudoTcp::OPT_ACKDELAY, ack_delay);
    remote_.SetOption(PseudoTcp::OPT_ACKDELAY, ack_delay);
  }
  void SetOptSndBuf(int size) {
    local_.SetOption(PseudoTcp::OPT_SNDBUF, size);
    remote_.SetOption(PseudoTcp::OPT_SNDBUF, size);
  }
  void SetRemoteOptRcvBuf(int size) {
    remote_.SetOption(PseudoTcp::OPT_RCVBUF, size);
  }
  void SetLocalOptRcvBuf(int size) {
    local_.SetOption(PseudoTcp::OPT_RCVBUF, size);
  }
  void DisableRemoteWindowScale() { remote_.disableWindowScale(); }
  void DisableLocalWindowScale() { local_.disableWindowScale(); }

 protected:
  int Connect() {
    int ret = local_.Connect();
    if (ret == 0) {
      UpdateLocalClock();
    }
    if (simultaneous_open_) {
      ret = remote_.Connect();
      if (ret == 0) {
        UpdateRemoteClock();
      }
    }
    return ret;
  }
  void Close() {
    local_.Close(false);
    UpdateLocalClock();
  }

  enum {
    MSG_LPACKET,
    MSG_RPACKET,
    MSG_LCLOCK,
    MSG_RCLOCK,
    MSG_IOCOMPLETE,
    MSG_WRITE
  };
  virtual void OnTcpOpen(PseudoTcp* tcp) {
    // Consider ourselves connected when the local side gets OnTcpOpen.
    // OnTcpWriteable isn't fired at open, so we trigger it now.
    RTC_LOG(LS_VERBOSE) << "Opened";
    if (tcp == &local_) {
      have_connected_ = true;
      OnTcpWriteable(tcp);
    }
  }
  // Test derived from the base should override
  //   virtual void OnTcpReadable(PseudoTcp* tcp)
  // and
  //   virtual void OnTcpWritable(PseudoTcp* tcp)
  virtual void OnTcpClosed(PseudoTcp* tcp, uint32_t error) {
    // Consider ourselves closed when the remote side gets OnTcpClosed.
    // TODO(?): OnTcpClosed is only ever notified in case of error in
    // the current implementation.  Solicited close is not (yet) supported.
    RTC_LOG(LS_VERBOSE) << "Closed";
    EXPECT_EQ(0U, error);
    if (tcp == &remote_) {
      have_disconnected_ = true;
    }
  }
  virtual WriteResult TcpWritePacket(PseudoTcp* tcp,
                                     const char* buffer,
                                     size_t len) {
    // Drop a packet if the test called DropNextPacket.
    if (drop_next_packet_) {
      drop_next_packet_ = false;
      RTC_LOG(LS_VERBOSE) << "Dropping packet due to DropNextPacket, size="
                          << len;
      return WR_SUCCESS;
    }
    // Randomly drop the desired percentage of packets.
    if (rtc::CreateRandomId() % 100 < static_cast<uint32_t>(loss_)) {
      RTC_LOG(LS_VERBOSE) << "Randomly dropping packet, size=" << len;
      return WR_SUCCESS;
    }
    // Also drop packets that are larger than the configured MTU.
    if (len > static_cast<size_t>(std::min(local_mtu_, remote_mtu_))) {
      RTC_LOG(LS_VERBOSE) << "Dropping packet that exceeds path MTU, size="
                          << len;
      return WR_SUCCESS;
    }
    int id = (tcp == &local_) ? MSG_RPACKET : MSG_LPACKET;
    std::string packet(buffer, len);
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, delay_, this, id,
                                        rtc::WrapMessageData(packet));
    return WR_SUCCESS;
  }

  void UpdateLocalClock() { UpdateClock(&local_, MSG_LCLOCK); }
  void UpdateRemoteClock() { UpdateClock(&remote_, MSG_RCLOCK); }
  void UpdateClock(PseudoTcp* tcp, uint32_t message) {
    long interval = 0;  // NOLINT
    tcp->GetNextClock(PseudoTcp::Now(), interval);
    interval = std::max<int>(interval, 0L);  // sometimes interval is < 0
    rtc::Thread::Current()->Clear(this, message);
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, interval, this, message);
  }

  virtual void OnMessage(rtc::Message* message) {
    switch (message->message_id) {
      case MSG_LPACKET: {
        const std::string& s(rtc::UseMessageData<std::string>(message->pdata));
        local_.NotifyPacket(s.c_str(), s.size());
        UpdateLocalClock();
        break;
      }
      case MSG_RPACKET: {
        const std::string& s(rtc::UseMessageData<std::string>(message->pdata));
        remote_.NotifyPacket(s.c_str(), s.size());
        UpdateRemoteClock();
        break;
      }
      case MSG_LCLOCK:
        local_.NotifyClock(PseudoTcp::Now());
        UpdateLocalClock();
        break;
      case MSG_RCLOCK:
        remote_.NotifyClock(PseudoTcp::Now());
        UpdateRemoteClock();
        break;
      default:
        break;
    }
    delete message->pdata;
  }

  PseudoTcpForTest local_;
  PseudoTcpForTest remote_;
  rtc::MemoryStream send_stream_;
  rtc::MemoryStream recv_stream_;
  bool have_connected_;
  bool have_disconnected_;
  int local_mtu_;
  int remote_mtu_;
  int delay_;
  int loss_;
  bool drop_next_packet_ = false;
  bool simultaneous_open_ = false;
};

class PseudoTcpTest : public PseudoTcpTestBase {
 public:
  void TestTransfer(int size) {
    uint32_t start;
    int32_t elapsed;
    size_t received;
    // Create some dummy data to send.
    send_stream_.ReserveSize(size);
    for (int i = 0; i < size; ++i) {
      char ch = static_cast<char>(i);
      send_stream_.Write(&ch, 1, NULL, NULL);
    }
    send_stream_.Rewind();
    // Prepare the receive stream.
    recv_stream_.ReserveSize(size);
    // Connect and wait until connected.
    start = rtc::Time32();
    EXPECT_EQ(0, Connect());
    EXPECT_TRUE_WAIT(have_connected_, kConnectTimeoutMs);
    // Sending will start from OnTcpWriteable and complete when all data has
    // been received.
    EXPECT_TRUE_WAIT(have_disconnected_, kTransferTimeoutMs);
    elapsed = rtc::Time32() - start;
    recv_stream_.GetSize(&received);
    // Ensure we closed down OK and we got the right data.
    // TODO(?): Ensure the errors are cleared properly.
    // EXPECT_EQ(0, local_.GetError());
    // EXPECT_EQ(0, remote_.GetError());
    EXPECT_EQ(static_cast<size_t>(size), received);
    EXPECT_EQ(0,
              memcmp(send_stream_.GetBuffer(), recv_stream_.GetBuffer(), size));
    RTC_LOG(LS_INFO) << "Transferred " << received << " bytes in " << elapsed
                     << " ms (" << size * 8 / elapsed << " Kbps)";
  }

 private:
  // IPseudoTcpNotify interface

  virtual void OnTcpReadable(PseudoTcp* tcp) {
    // Stream bytes to the recv stream as they arrive.
    if (tcp == &remote_) {
      ReadData();

      // TODO(?): OnTcpClosed() is currently only notified on error -
      // there is no on-the-wire equivalent of TCP FIN.
      // So we fake the notification when all the data has been read.
      size_t received, required;
      recv_stream_.GetPosition(&received);
      send_stream_.GetSize(&required);
      if (received == required)
        OnTcpClosed(&remote_, 0);
    }
  }
  virtual void OnTcpWriteable(PseudoTcp* tcp) {
    // Write bytes from the send stream when we can.
    // Shut down when we've sent everything.
    if (tcp == &local_) {
      RTC_LOG(LS_VERBOSE) << "Flow Control Lifted";
      bool done;
      WriteData(&done);
      if (done) {
        Close();
      }
    }
  }

  void ReadData() {
    char block[kBlockSize];
    size_t position;
    int rcvd;
    do {
      rcvd = remote_.Recv(block, sizeof(block));
      if (rcvd != -1) {
        recv_stream_.Write(block, rcvd, NULL, NULL);
        recv_stream_.GetPosition(&position);
        RTC_LOG(LS_VERBOSE) << "Received: " << position;
      }
    } while (rcvd > 0);
  }
  void WriteData(bool* done) {
    size_t position, tosend;
    int sent;
    char block[kBlockSize];
    do {
      send_stream_.GetPosition(&position);
      if (send_stream_.Read(block, sizeof(block), &tosend, NULL) !=
          rtc::SR_EOS) {
        sent = local_.Send(block, tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_.SetPosition(position + sent);
          RTC_LOG(LS_VERBOSE) << "Sent: " << position + sent;
        } else {
          send_stream_.SetPosition(position);
          RTC_LOG(LS_VERBOSE) << "Flow Controlled";
        }
      } else {
        sent = static_cast<int>(tosend = 0);
      }
    } while (sent > 0);
    *done = (tosend == 0);
  }

 private:
  rtc::MemoryStream send_stream_;
  rtc::MemoryStream recv_stream_;
};

class PseudoTcpTestPingPong : public PseudoTcpTestBase {
 public:
  PseudoTcpTestPingPong()
      : iterations_remaining_(0),
        sender_(NULL),
        receiver_(NULL),
        bytes_per_send_(0) {}
  void SetBytesPerSend(int bytes) { bytes_per_send_ = bytes; }
  void TestPingPong(int size, int iterations) {
    uint32_t start, elapsed;
    iterations_remaining_ = iterations;
    receiver_ = &remote_;
    sender_ = &local_;
    // Create some dummy data to send.
    send_stream_.ReserveSize(size);
    for (int i = 0; i < size; ++i) {
      char ch = static_cast<char>(i);
      send_stream_.Write(&ch, 1, NULL, NULL);
    }
    send_stream_.Rewind();
    // Prepare the receive stream.
    recv_stream_.ReserveSize(size);
    // Connect and wait until connected.
    start = rtc::Time32();
    EXPECT_EQ(0, Connect());
    EXPECT_TRUE_WAIT(have_connected_, kConnectTimeoutMs);
    // Sending will start from OnTcpWriteable and stop when the required
    // number of iterations have completed.
    EXPECT_TRUE_WAIT(have_disconnected_, kTransferTimeoutMs);
    elapsed = rtc::TimeSince(start);
    RTC_LOG(LS_INFO) << "Performed " << iterations << " pings in " << elapsed
                     << " ms";
  }

 private:
  // IPseudoTcpNotify interface

  virtual void OnTcpReadable(PseudoTcp* tcp) {
    if (tcp != receiver_) {
      RTC_LOG_F(LS_ERROR) << "unexpected OnTcpReadable";
      return;
    }
    // Stream bytes to the recv stream as they arrive.
    ReadData();
    // If we've received the desired amount of data, rewind things
    // and send it back the other way!
    size_t position, desired;
    recv_stream_.GetPosition(&position);
    send_stream_.GetSize(&desired);
    if (position == desired) {
      if (receiver_ == &local_ && --iterations_remaining_ == 0) {
        Close();
        // TODO(?): Fake OnTcpClosed() on the receiver for now.
        OnTcpClosed(&remote_, 0);
        return;
      }
      PseudoTcp* tmp = receiver_;
      receiver_ = sender_;
      sender_ = tmp;
      recv_stream_.Rewind();
      send_stream_.Rewind();
      OnTcpWriteable(sender_);
    }
  }
  virtual void OnTcpWriteable(PseudoTcp* tcp) {
    if (tcp != sender_)
      return;
    // Write bytes from the send stream when we can.
    // Shut down when we've sent everything.
    RTC_LOG(LS_VERBOSE) << "Flow Control Lifted";
    WriteData();
  }

  void ReadData() {
    char block[kBlockSize];
    size_t position;
    int rcvd;
    do {
      rcvd = receiver_->Recv(block, sizeof(block));
      if (rcvd != -1) {
        recv_stream_.Write(block, rcvd, NULL, NULL);
        recv_stream_.GetPosition(&position);
        RTC_LOG(LS_VERBOSE) << "Received: " << position;
      }
    } while (rcvd > 0);
  }
  void WriteData() {
    size_t position, tosend;
    int sent;
    char block[kBlockSize];
    do {
      send_stream_.GetPosition(&position);
      tosend = bytes_per_send_ ? bytes_per_send_ : sizeof(block);
      if (send_stream_.Read(block, tosend, &tosend, NULL) != rtc::SR_EOS) {
        sent = sender_->Send(block, tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_.SetPosition(position + sent);
          RTC_LOG(LS_VERBOSE) << "Sent: " << position + sent;
        } else {
          send_stream_.SetPosition(position);
          RTC_LOG(LS_VERBOSE) << "Flow Controlled";
        }
      } else {
        sent = static_cast<int>(tosend = 0);
      }
    } while (sent > 0);
  }

 private:
  int iterations_remaining_;
  PseudoTcp* sender_;
  PseudoTcp* receiver_;
  int bytes_per_send_;
};

// Fill the receiver window until it is full, drain it and then
// fill it with the same amount. This is to test that receiver window
// contracts and enlarges correctly.
class PseudoTcpTestReceiveWindow : public PseudoTcpTestBase {
 public:
  // Not all the data are transfered, |size| just need to be big enough
  // to fill up the receiver window twice.
  void TestTransfer(int size) {
    // Create some dummy data to send.
    send_stream_.ReserveSize(size);
    for (int i = 0; i < size; ++i) {
      char ch = static_cast<char>(i);
      send_stream_.Write(&ch, 1, NULL, NULL);
    }
    send_stream_.Rewind();

    // Prepare the receive stream.
    recv_stream_.ReserveSize(size);

    // Connect and wait until connected.
    EXPECT_EQ(0, Connect());
    EXPECT_TRUE_WAIT(have_connected_, kConnectTimeoutMs);

    rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_WRITE);
    EXPECT_TRUE_WAIT(have_disconnected_, kTransferTimeoutMs);

    ASSERT_EQ(2u, send_position_.size());
    ASSERT_EQ(2u, recv_position_.size());

    const size_t estimated_recv_window = EstimateReceiveWindowSize();

    // The difference in consecutive send positions should equal the
    // receive window size or match very closely. This verifies that receive
    // window is open after receiver drained all the data.
    const size_t send_position_diff = send_position_[1] - send_position_[0];
    EXPECT_GE(1024u, estimated_recv_window - send_position_diff);

    // Receiver drained the receive window twice.
    EXPECT_EQ(2 * estimated_recv_window, recv_position_[1]);
  }

  virtual void OnMessage(rtc::Message* message) {
    int message_id = message->message_id;
    PseudoTcpTestBase::OnMessage(message);

    switch (message_id) {
      case MSG_WRITE: {
        WriteData();
        break;
      }
      default:
        break;
    }
  }

  uint32_t EstimateReceiveWindowSize() const {
    return static_cast<uint32_t>(recv_position_[0]);
  }

  uint32_t EstimateSendWindowSize() const {
    return static_cast<uint32_t>(send_position_[0] - recv_position_[0]);
  }

 private:
  // IPseudoTcpNotify interface
  virtual void OnTcpReadable(PseudoTcp* tcp) {}

  virtual void OnTcpWriteable(PseudoTcp* tcp) {}

  void ReadUntilIOPending() {
    char block[kBlockSize];
    size_t position;
    int rcvd;

    do {
      rcvd = remote_.Recv(block, sizeof(block));
      if (rcvd != -1) {
        recv_stream_.Write(block, rcvd, NULL, NULL);
        recv_stream_.GetPosition(&position);
        RTC_LOG(LS_VERBOSE) << "Received: " << position;
      }
    } while (rcvd > 0);

    recv_stream_.GetPosition(&position);
    recv_position_.push_back(position);

    // Disconnect if we have done two transfers.
    if (recv_position_.size() == 2u) {
      Close();
      OnTcpClosed(&remote_, 0);
    } else {
      WriteData();
    }
  }

  void WriteData() {
    size_t position, tosend;
    int sent;
    char block[kBlockSize];
    do {
      send_stream_.GetPosition(&position);
      if (send_stream_.Read(block, sizeof(block), &tosend, NULL) !=
          rtc::SR_EOS) {
        sent = local_.Send(block, tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_.SetPosition(position + sent);
          RTC_LOG(LS_VERBOSE) << "Sent: " << position + sent;
        } else {
          send_stream_.SetPosition(position);
          RTC_LOG(LS_VERBOSE) << "Flow Controlled";
        }
      } else {
        sent = static_cast<int>(tosend = 0);
      }
    } while (sent > 0);
    // At this point, we've filled up the available space in the send queue.

    int message_queue_size = static_cast<int>(rtc::Thread::Current()->size());
    // The message queue will always have at least 2 messages, an RCLOCK and
    // an LCLOCK, since they are added back on the delay queue at the same time
    // they are pulled off and therefore are never really removed.
    if (message_queue_size > 2) {
      // If there are non-clock messages remaining, attempt to continue sending
      // after giving those messages time to process, which should free up the
      // send buffer.
      rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 10, this, MSG_WRITE);
    } else {
      if (!remote_.isReceiveBufferFull()) {
        RTC_LOG(LS_ERROR) << "This shouldn't happen - the send buffer is full, "
                             "the receive buffer is not, and there are no "
                             "remaining messages to process.";
      }
      send_stream_.GetPosition(&position);
      send_position_.push_back(position);

      // Drain the receiver buffer.
      ReadUntilIOPending();
    }
  }

 private:
  rtc::MemoryStream send_stream_;
  rtc::MemoryStream recv_stream_;

  std::vector<size_t> send_position_;
  std::vector<size_t> recv_position_;
};

// Basic end-to-end data transfer tests

// Test the normal case of sending data from one side to the other.
TEST_F(PseudoTcpTest, TestSend) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestTransfer(1000000);
}

// Test sending data with a 50 ms RTT. Transmission should take longer due
// to a slower ramp-up in send rate.
TEST_F(PseudoTcpTest, TestSendWithDelay) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  TestTransfer(1000000);
}

// Test sending data with packet loss. Transmission should take much longer due
// to send back-off when loss occurs.
TEST_F(PseudoTcpTest, TestSendWithLoss) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with a 50 ms RTT and 10% packet loss. Transmission should
// take much longer due to send back-off and slower detection of loss.
TEST_F(PseudoTcpTest, TestSendWithDelayAndLoss) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetLoss(10);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with 10% packet loss and Nagling disabled.  Transmission
// should take about the same time as with Nagling enabled.
TEST_F(PseudoTcpTest, TestSendWithLossAndOptNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  SetOptNagling(false);
  TestTransfer(100000);  // less data so test runs faster
}

// Regression test for bugs.webrtc.org/9208.
//
// This bug resulted in corrupted data if a "connect" segment was received after
// a data segment. This is only possible if:
//
// * The initial "connect" segment is lost, and retransmitted later.
// * Both sides send "connect"s simultaneously, such that the local side thinks
//   a connection is established even before its "connect" has been
//   acknowledged.
// * Nagle algorithm disabled, allowing a data segment to be sent before the
//   "connect" has been acknowledged.
TEST_F(PseudoTcpTest,
       TestSendWhenFirstPacketLostWithOptNaglingOffAndSimultaneousOpen) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  DropNextPacket();
  SetOptNagling(false);
  SetSimultaneousOpen(true);
  TestTransfer(10000);
}

// Test sending data with 10% packet loss and Delayed ACK disabled.
// Transmission should be slightly faster than with it enabled.
TEST_F(PseudoTcpTest, TestSendWithLossAndOptAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  SetOptAckDelay(0);
  TestTransfer(100000);
}

// Test sending data with 50ms delay and Nagling disabled.
TEST_F(PseudoTcpTest, TestSendWithDelayAndOptNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetOptNagling(false);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with 50ms delay and Delayed ACK disabled.
TEST_F(PseudoTcpTest, TestSendWithDelayAndOptAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetOptAckDelay(0);
  TestTransfer(100000);  // less data so test runs faster
}

// Test a large receive buffer with a sender that doesn't support scaling.
TEST_F(PseudoTcpTest, TestSendRemoteNoWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLocalOptRcvBuf(100000);
  DisableRemoteWindowScale();
  TestTransfer(1000000);
}

// Test a large sender-side receive buffer with a receiver that doesn't support
// scaling.
TEST_F(PseudoTcpTest, TestSendLocalNoWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  DisableLocalWindowScale();
  TestTransfer(1000000);
}

// Test when both sides use window scaling.
TEST_F(PseudoTcpTest, TestSendBothUseWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  TestTransfer(1000000);
}

// Test using a large window scale value.
TEST_F(PseudoTcpTest, TestSendLargeInFlight) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  SetOptSndBuf(150000);
  TestTransfer(1000000);
}

TEST_F(PseudoTcpTest, TestSendBothUseLargeWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(1000000);
  SetLocalOptRcvBuf(1000000);
  TestTransfer(10000000);
}

// Test using a small receive buffer.
TEST_F(PseudoTcpTest, TestSendSmallReceiveBuffer) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(10000);
  SetLocalOptRcvBuf(10000);
  TestTransfer(1000000);
}

// Test using a very small receive buffer.
TEST_F(PseudoTcpTest, TestSendVerySmallReceiveBuffer) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100);
  SetLocalOptRcvBuf(100);
  TestTransfer(100000);
}

// Ping-pong (request/response) tests

// Test sending <= 1x MTU of data in each ping/pong.  Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong1xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(100, 100);
}

// Test sending 2x-3x MTU of data in each ping/pong.  Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong3xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(400, 100);
}

// Test sending 1x-2x MTU of data in each ping/pong.
// Should take ~1s, due to interaction between Nagling and Delayed ACK.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(2000, 5);
}

// Test sending 1x-2x MTU of data in each ping/pong with Delayed ACK off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtuWithAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptAckDelay(0);
  TestPingPong(2000, 100);
}

// Test sending 1x-2x MTU of data in each ping/pong with Nagling off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtuWithNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  TestPingPong(2000, 5);
}

// Test sending a ping as pair of short (non-full) segments.
// Should take ~1s, due to Delayed ACK interaction with Nagling.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegments) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptAckDelay(5000);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  TestPingPong(100, 5);
}

// Test sending ping as a pair of short (non-full) segments, with Nagling off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegmentsWithNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  TestPingPong(100, 5);
}

// Test sending <= 1x MTU of data ping/pong, in two segments, no Delayed ACK.
// Should take ~1s.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegmentsWithAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  SetOptAckDelay(0);
  TestPingPong(100, 5);
}

// Test that receive window expands and contract correctly.
TEST_F(PseudoTcpTestReceiveWindow, TestReceiveWindow) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  TestTransfer(1024 * 1000);
}

// Test setting send window size to a very small value.
TEST_F(PseudoTcpTestReceiveWindow, TestSetVerySmallSendWindowSize) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  SetOptSndBuf(900);
  TestTransfer(1024 * 1000);
  EXPECT_EQ(900u, EstimateSendWindowSize());
}

// Test setting receive window size to a value other than default.
TEST_F(PseudoTcpTestReceiveWindow, TestSetReceiveWindowSize) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  TestTransfer(1024 * 1000);
  EXPECT_EQ(100000u, EstimateReceiveWindowSize());
}

/* Test sending data with mismatched MTUs. We should detect this and reduce
// our packet size accordingly.
// TODO(?): This doesn't actually work right now. The current code
// doesn't detect if the MTU is set too high on either side.
TEST_F(PseudoTcpTest, TestSendWithMismatchedMtus) {
  SetLocalMtu(1500);
  SetRemoteMtu(1280);
  TestTransfer(1000000);
}
*/
