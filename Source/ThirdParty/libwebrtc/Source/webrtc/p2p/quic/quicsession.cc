/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quicsession.h"

#include <string>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"

namespace cricket {

// Default priority for incoming QUIC streams.
// TODO(mikescarlett): Determine if this value is correct.
static const net::SpdyPriority kDefaultPriority = 3;

QuicSession::QuicSession(std::unique_ptr<net::QuicConnection> connection,
                         const net::QuicConfig& config)
    : net::QuicSession(connection.release(), config) {}

QuicSession::~QuicSession() {}

void QuicSession::StartClientHandshake(
    net::QuicCryptoClientStream* crypto_stream) {
  SetCryptoStream(crypto_stream);
  net::QuicSession::Initialize();
  crypto_stream->CryptoConnect();
}

void QuicSession::StartServerHandshake(
    net::QuicCryptoServerStream* crypto_stream) {
  SetCryptoStream(crypto_stream);
  net::QuicSession::Initialize();
}

void QuicSession::SetCryptoStream(net::QuicCryptoStream* crypto_stream) {
  crypto_stream_.reset(crypto_stream);
}

bool QuicSession::ExportKeyingMaterial(base::StringPiece label,
                                       base::StringPiece context,
                                       size_t result_len,
                                       std::string* result) {
  return crypto_stream_->ExportKeyingMaterial(label, context, result_len,
                                              result);
}

void QuicSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  net::QuicSession::OnCryptoHandshakeEvent(event);
  if (event == HANDSHAKE_CONFIRMED) {
    LOG(LS_INFO) << "QuicSession handshake complete";
    RTC_DCHECK(IsEncryptionEstablished());
    RTC_DCHECK(IsCryptoHandshakeConfirmed());

    SignalHandshakeComplete();
  }
}

void QuicSession::CloseStream(net::QuicStreamId stream_id) {
  if (IsClosedStream(stream_id)) {
    // When CloseStream has been called recursively (via
    // ReliableQuicStream::OnClose), the stream is already closed so return.
    return;
  }
  write_blocked_streams()->UnregisterStream(stream_id);
  net::QuicSession::CloseStream(stream_id);
}

ReliableQuicStream* QuicSession::CreateIncomingDynamicStream(
    net::QuicStreamId id) {
  ReliableQuicStream* stream = CreateDataStream(id, kDefaultPriority);
  if (stream) {
    SignalIncomingStream(stream);
  }
  return stream;
}

ReliableQuicStream* QuicSession::CreateOutgoingDynamicStream(
    net::SpdyPriority priority) {
  return CreateDataStream(GetNextOutgoingStreamId(), priority);
}

ReliableQuicStream* QuicSession::CreateDataStream(net::QuicStreamId id,
                                                  net::SpdyPriority priority) {
  if (crypto_stream_ == nullptr || !crypto_stream_->encryption_established()) {
    // Encryption not active so no stream created
    return nullptr;
  }
  ReliableQuicStream* stream = new ReliableQuicStream(id, this);
  if (stream) {
    // Make QuicSession take ownership of the stream.
    ActivateStream(stream);
    // Register the stream to the QuicWriteBlockedList. |priority| is clamped
    // between 0 and 7, with 0 being the highest priority and 7 the lowest
    // priority.
    write_blocked_streams()->RegisterStream(stream->id(), priority);
  }
  return stream;
}

void QuicSession::OnConnectionClosed(net::QuicErrorCode error,
                                     const std::string& error_details,
                                     net::ConnectionCloseSource source) {
  net::QuicSession::OnConnectionClosed(error, error_details, source);
  SignalConnectionClosed(error,
                         source == net::ConnectionCloseSource::FROM_PEER);
}

bool QuicSession::OnReadPacket(const char* data, size_t data_len) {
  net::QuicReceivedPacket packet(data, data_len, clock_.Now());
  ProcessUdpPacket(connection()->self_address(), connection()->peer_address(),
                   packet);
  return true;
}

}  // namespace cricket
