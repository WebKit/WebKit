/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_QUIC_QUICSESSION_H_
#define WEBRTC_P2P_QUIC_QUICSESSION_H_

#include <memory>
#include <string>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_session.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/p2p/quic/reliablequicstream.h"

namespace cricket {

// This class provides a QUIC session over peer-to-peer transport that
// negotiates the crypto handshake (using QuicCryptoHandshake) and provides
// reading/writing of data using QUIC packets.
class QuicSession : public net::QuicSession, public sigslot::has_slots<> {
 public:
  QuicSession(std::unique_ptr<net::QuicConnection> connection,
              const net::QuicConfig& config);
  ~QuicSession() override;

  // Initiates client crypto handshake by sending client hello.
  void StartClientHandshake(net::QuicCryptoClientStream* crypto_stream);

  // Responds to a client who has inititated the crypto handshake.
  void StartServerHandshake(net::QuicCryptoServerStream* crypto_stream);

  // QuicSession overrides.
  net::QuicCryptoStream* GetCryptoStream() override {
    return crypto_stream_.get();
  }
  ReliableQuicStream* CreateOutgoingDynamicStream(
      net::SpdyPriority priority) override;

  // QuicSession optional overrides.
  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;
  void CloseStream(net::QuicStreamId stream_id) override;

  // QuicConnectionVisitorInterface overrides.
  void OnConnectionClosed(net::QuicErrorCode error,
                          const std::string& error_details,
                          net::ConnectionCloseSource source) override;

  // Exports keying material for SRTP.
  bool ExportKeyingMaterial(base::StringPiece label,
                            base::StringPiece context,
                            size_t result_len,
                            std::string* result);

  // Decrypts an incoming QUIC packet to a data stream.
  bool OnReadPacket(const char* data, size_t data_len);

  // Called when peers have established forward-secure encryption
  sigslot::signal0<> SignalHandshakeComplete;
  // Called when connection closes locally, or remotely by peer.
  sigslot::signal2<net::QuicErrorCode, bool> SignalConnectionClosed;
  // Called when an incoming QUIC stream is created so we can process data
  // from it by registering a listener to
  // ReliableQuicStream::SignalDataReceived.
  sigslot::signal1<ReliableQuicStream*> SignalIncomingStream;

 protected:
  // Sets the QUIC crypto stream and takes ownership of it.
  void SetCryptoStream(net::QuicCryptoStream* crypto_stream);

  // QuicSession override.
  ReliableQuicStream* CreateIncomingDynamicStream(
      net::QuicStreamId id) override;

  virtual ReliableQuicStream* CreateDataStream(net::QuicStreamId id,
                                               net::SpdyPriority priority);

 private:
  std::unique_ptr<net::QuicCryptoStream> crypto_stream_;
  net::QuicClock clock_;  // For recording packet receipt time

  RTC_DISALLOW_COPY_AND_ASSIGN(QuicSession);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_QUIC_QUICSESSION_H_
