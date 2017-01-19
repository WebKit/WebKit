/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/reliablequicstream.h"

#include <memory>
#include <string>

#include "net/base/ip_address_number.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/stream.h"
#include "webrtc/p2p/quic/quicconnectionhelper.h"

using cricket::QuicConnectionHelper;
using cricket::ReliableQuicStream;

using net::IPAddress;
using net::IPEndPoint;
using net::PerPacketOptions;
using net::Perspective;
using net::QuicAckListenerInterface;
using net::QuicConfig;
using net::QuicConnection;
using net::QuicConsumedData;
using net::QuicCryptoStream;
using net::QuicErrorCode;
using net::QuicIOVector;
using net::QuicPacketWriter;
using net::QuicRstStreamErrorCode;
using net::QuicSession;
using net::QuicStreamId;
using net::QuicStreamOffset;
using net::SpdyPriority;

using rtc::SR_SUCCESS;
using rtc::SR_BLOCK;

// Arbitrary number for a stream's write blocked priority.
static const SpdyPriority kDefaultPriority = 3;
static const net::QuicStreamId kStreamId = 5;

// QuicSession that does not create streams and writes data from
// ReliableQuicStream to a string.
class MockQuicSession : public QuicSession {
 public:
  MockQuicSession(QuicConnection* connection,
                  const QuicConfig& config,
                  std::string* write_buffer)
      : QuicSession(connection, config), write_buffer_(write_buffer) {}

  // Writes outgoing data from ReliableQuicStream to a string.
  QuicConsumedData WritevData(
      QuicStreamId id,
      QuicIOVector iovector,
      QuicStreamOffset offset,
      bool fin,
      QuicAckListenerInterface* ack_notifier_delegate) override {
    if (!writable_) {
      return QuicConsumedData(0, false);
    }

    const char* data = reinterpret_cast<const char*>(iovector.iov->iov_base);
    size_t len = iovector.total_length;
    write_buffer_->append(data, len);
    return QuicConsumedData(len, false);
  }

  net::ReliableQuicStream* CreateIncomingDynamicStream(
      QuicStreamId id) override {
    return new ReliableQuicStream(kStreamId, this);
  }

  net::ReliableQuicStream* CreateOutgoingDynamicStream(
      SpdyPriority priority) override {
    return nullptr;
  }

  QuicCryptoStream* GetCryptoStream() override { return nullptr; }

  // Called by ReliableQuicStream when they want to close stream.
  void SendRstStream(QuicStreamId id,
                     QuicRstStreamErrorCode error,
                     QuicStreamOffset bytes_written) override {}

  // Sets whether data is written to buffer, or else if this is write blocked.
  void set_writable(bool writable) { writable_ = writable; }

  // Tracks whether the stream is write blocked and its priority.
  void register_write_blocked_stream(QuicStreamId stream_id,
                                     SpdyPriority priority) {
    write_blocked_streams()->RegisterStream(stream_id, priority);
  }

 private:
  // Stores written data from ReliableQuicStream.
  std::string* write_buffer_;
  // Whether data is written to write_buffer_.
  bool writable_ = true;
};

// Packet writer that does nothing. This is required for QuicConnection but
// isn't used for writing data.
class DummyPacketWriter : public QuicPacketWriter {
 public:
  DummyPacketWriter() {}

  // QuicPacketWriter overrides.
  net::WriteResult WritePacket(const char* buffer,
                               size_t buf_len,
                               const IPAddress& self_address,
                               const IPEndPoint& peer_address,
                               PerPacketOptions* options) override {
    return net::WriteResult(net::WRITE_STATUS_ERROR, 0);
  }

  bool IsWriteBlockedDataBuffered() const override { return false; }

  bool IsWriteBlocked() const override { return false; };

  void SetWritable() override {}

  net::QuicByteCount GetMaxPacketSize(
      const net::IPEndPoint& peer_address) const override {
    return 0;
  }
};

class ReliableQuicStreamTest : public ::testing::Test,
                               public sigslot::has_slots<> {
 public:
  ReliableQuicStreamTest() {}

  void CreateReliableQuicStream() {

    // Arbitrary values for QuicConnection.
    QuicConnectionHelper* quic_helper =
        new QuicConnectionHelper(rtc::Thread::Current());
    Perspective perspective = Perspective::IS_SERVER;
    net::IPAddress ip(0, 0, 0, 0);

    bool owns_writer = true;

    QuicConnection* connection = new QuicConnection(
        0, IPEndPoint(ip, 0), quic_helper, new DummyPacketWriter(), owns_writer,
        perspective, net::QuicSupportedVersions());

    session_.reset(
        new MockQuicSession(connection, QuicConfig(), &write_buffer_));
    stream_.reset(new ReliableQuicStream(kStreamId, session_.get()));
    stream_->SignalDataReceived.connect(
        this, &ReliableQuicStreamTest::OnDataReceived);
    stream_->SignalClosed.connect(this, &ReliableQuicStreamTest::OnClosed);
    stream_->SignalQueuedBytesWritten.connect(
        this, &ReliableQuicStreamTest::OnQueuedBytesWritten);

    session_->register_write_blocked_stream(stream_->id(), kDefaultPriority);
  }

  void OnDataReceived(QuicStreamId id, const char* data, size_t length) {
    ASSERT_EQ(id, stream_->id());
    read_buffer_.append(data, length);
  }

  void OnClosed(QuicStreamId id, int err) { closed_ = true; }

  void OnQueuedBytesWritten(QuicStreamId id, uint64_t queued_bytes_written) {
    queued_bytes_written_ = queued_bytes_written;
  }

 protected:
  std::unique_ptr<ReliableQuicStream> stream_;
  std::unique_ptr<MockQuicSession> session_;

  // Data written by the ReliableQuicStream.
  std::string write_buffer_;
  // Data read by the ReliableQuicStream.
  std::string read_buffer_;
  // Whether the ReliableQuicStream is closed.
  bool closed_ = false;
  // Bytes written by OnCanWrite().
  uint64_t queued_bytes_written_;
};

// Write an entire string.
TEST_F(ReliableQuicStreamTest, WriteDataWhole) {
  CreateReliableQuicStream();
  EXPECT_EQ(SR_SUCCESS, stream_->Write("Foo bar", 7));

  EXPECT_EQ("Foo bar", write_buffer_);
}

// Write part of a string.
TEST_F(ReliableQuicStreamTest, WriteDataPartial) {
  CreateReliableQuicStream();
  EXPECT_EQ(SR_SUCCESS, stream_->Write("Hello, World!", 8));
  EXPECT_EQ("Hello, W", write_buffer_);
}

// Test that strings are buffered correctly.
TEST_F(ReliableQuicStreamTest, BufferData) {
  CreateReliableQuicStream();

  session_->set_writable(false);
  EXPECT_EQ(SR_BLOCK, stream_->Write("Foo bar", 7));

  EXPECT_EQ(0ul, write_buffer_.size());
  EXPECT_TRUE(stream_->HasBufferedData());

  session_->set_writable(true);
  stream_->OnCanWrite();
  EXPECT_EQ(7ul, queued_bytes_written_);

  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_EQ("Foo bar", write_buffer_);

  EXPECT_EQ(SR_SUCCESS, stream_->Write("xyzzy", 5));
  EXPECT_EQ("Foo barxyzzy", write_buffer_);
}

// Read an entire string.
TEST_F(ReliableQuicStreamTest, ReadDataWhole) {
  CreateReliableQuicStream();
  net::QuicStreamFrame frame(kStreamId, false, 0, "Hello, World!");
  stream_->OnStreamFrame(frame);

  EXPECT_EQ("Hello, World!", read_buffer_);
}

// Read part of a string.
TEST_F(ReliableQuicStreamTest, ReadDataPartial) {
  CreateReliableQuicStream();
  net::QuicStreamFrame frame(kStreamId, false, 0, "Hello, World!");
  frame.frame_length = 5;
  stream_->OnStreamFrame(frame);

  EXPECT_EQ("Hello", read_buffer_);
}

// Test that closing the stream results in a callback.
TEST_F(ReliableQuicStreamTest, CloseStream) {
  CreateReliableQuicStream();
  EXPECT_FALSE(closed_);
  stream_->OnClose();
  EXPECT_TRUE(closed_);
}
