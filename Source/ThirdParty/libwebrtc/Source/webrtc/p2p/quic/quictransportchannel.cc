/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quictransportchannel.h"

#include <utility>

#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_crypto_client_config.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/common.h"

namespace {

// QUIC public header constants for net::QuicConnection. These are arbitrary
// given that |channel_| only receives packets specific to this channel,
// in which case we already know the QUIC packets have the correct destination.
const net::QuicConnectionId kConnectionId = 0;
const net::IPAddress kConnectionIpAddress(0, 0, 0, 0);
const net::IPEndPoint kConnectionIpEndpoint(kConnectionIpAddress, 0);

// Arbitrary server port number for net::QuicCryptoClientConfig.
const int kQuicServerPort = 0;

// QUIC connection timeout. This is large so that |channel_| can
// be responsible for connection timeout.
const int kIdleConnectionStateLifetime = 1000;  // seconds

// Length of HKDF input keying material, equal to its number of bytes.
// https://tools.ietf.org/html/rfc5869#section-2.2.
// TODO(mikescarlett): Verify that input keying material length is correct.
const size_t kInputKeyingMaterialLength = 32;

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
const size_t kMinRtpPacketLen = 12;

bool IsRtpPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

// Function for detecting QUIC packets based off
// https://tools.ietf.org/html/draft-tsvwg-quic-protocol-02#section-6.
const size_t kMinQuicPacketLen = 2;

bool IsQuicPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kMinQuicPacketLen && (u[0] & 0x80) == 0);
}

// Used by QuicCryptoServerConfig to provide dummy proof credentials.
// TODO(mikescarlett): Remove when secure P2P QUIC handshake is possible.
class DummyProofSource : public net::ProofSource {
 public:
  DummyProofSource() {}
  ~DummyProofSource() override {}

  // ProofSource override.
  bool GetProof(const net::IPAddress& server_ip,
                const std::string& hostname,
                const std::string& server_config,
                net::QuicVersion quic_version,
                base::StringPiece chlo_hash,
                bool ecdsa_ok,
                scoped_refptr<net::ProofSource::Chain>* out_chain,
                std::string* out_signature,
                std::string* out_leaf_cert_sct) override {
    LOG(LS_INFO) << "GetProof() providing dummy credentials for insecure QUIC";
    std::vector<std::string> certs;
    certs.push_back("Dummy cert");
    *out_chain = new ProofSource::Chain(certs);
    *out_signature = "Dummy signature";
    *out_leaf_cert_sct = "Dummy timestamp";
    return true;
  }
};

// Used by QuicCryptoClientConfig to ignore the peer's credentials
// and establish an insecure QUIC connection.
// TODO(mikescarlett): Remove when secure P2P QUIC handshake is possible.
class InsecureProofVerifier : public net::ProofVerifier {
 public:
  InsecureProofVerifier() {}
  ~InsecureProofVerifier() override {}

  // ProofVerifier override.
  net::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      net::QuicVersion quic_version,
      base::StringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const net::ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<net::ProofVerifyDetails>* verify_details,
      net::ProofVerifierCallback* callback) override {
    LOG(LS_INFO) << "VerifyProof() ignoring credentials and returning success";
    return net::QUIC_SUCCESS;
  }
};

}  // namespace

namespace cricket {

QuicTransportChannel::QuicTransportChannel(TransportChannelImpl* channel)
    : TransportChannelImpl(channel->transport_name(), channel->component()),
      network_thread_(rtc::Thread::Current()),
      channel_(channel),
      helper_(network_thread_) {
  channel_->SignalWritableState.connect(this,
                                        &QuicTransportChannel::OnWritableState);
  channel_->SignalReadPacket.connect(this, &QuicTransportChannel::OnReadPacket);
  channel_->SignalSentPacket.connect(this, &QuicTransportChannel::OnSentPacket);
  channel_->SignalReadyToSend.connect(this,
                                      &QuicTransportChannel::OnReadyToSend);
  channel_->SignalGatheringState.connect(
      this, &QuicTransportChannel::OnGatheringState);
  channel_->SignalCandidateGathered.connect(
      this, &QuicTransportChannel::OnCandidateGathered);
  channel_->SignalRoleConflict.connect(this,
                                       &QuicTransportChannel::OnRoleConflict);
  channel_->SignalRouteChange.connect(this,
                                      &QuicTransportChannel::OnRouteChange);
  channel_->SignalSelectedCandidatePairChanged.connect(
      this, &QuicTransportChannel::OnSelectedCandidatePairChanged);
  channel_->SignalStateChanged.connect(
      this, &QuicTransportChannel::OnChannelStateChanged);
  channel_->SignalReceivingState.connect(
      this, &QuicTransportChannel::OnReceivingState);

  // Set the QUIC connection timeout.
  config_.SetIdleConnectionStateLifetime(
      net::QuicTime::Delta::FromSeconds(kIdleConnectionStateLifetime),
      net::QuicTime::Delta::FromSeconds(kIdleConnectionStateLifetime));
  // Set the bytes reserved for the QUIC connection ID to zero.
  config_.SetBytesForConnectionIdToSend(0);
}

QuicTransportChannel::~QuicTransportChannel() {}

bool QuicTransportChannel::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  if (!certificate) {
    LOG_J(LS_ERROR, this)
        << "No local certificate was supplied. Not doing QUIC.";
    return false;
  }
  if (!local_certificate_) {
    local_certificate_ = certificate;
    return true;
  }
  if (certificate == local_certificate_) {
    // This may happen during renegotiation.
    LOG_J(LS_INFO, this) << "Ignoring identical certificate";
    return true;
  }
  LOG_J(LS_ERROR, this)
      << "Local certificate of the QUIC connection already set. "
         "Can't change the local certificate once it's active.";
  return false;
}

rtc::scoped_refptr<rtc::RTCCertificate>
QuicTransportChannel::GetLocalCertificate() const {
  return local_certificate_;
}

bool QuicTransportChannel::SetSslRole(rtc::SSLRole role) {
  if (ssl_role_ && *ssl_role_ == role) {
    LOG_J(LS_WARNING, this) << "Ignoring SSL Role identical to current role.";
    return true;
  }
  if (quic_state_ != QUIC_TRANSPORT_CONNECTED) {
    ssl_role_ = rtc::Optional<rtc::SSLRole>(role);
    return true;
  }
  LOG_J(LS_ERROR, this)
      << "SSL Role can't be reversed after the session is setup.";
  return false;
}

bool QuicTransportChannel::GetSslRole(rtc::SSLRole* role) const {
  if (!ssl_role_) {
    return false;
  }
  *role = *ssl_role_;
  return true;
}

bool QuicTransportChannel::SetRemoteFingerprint(const std::string& digest_alg,
                                                const uint8_t* digest,
                                                size_t digest_len) {
  if (digest_alg.empty()) {
    RTC_DCHECK(!digest_len);
    LOG_J(LS_ERROR, this) << "Remote peer doesn't support digest algorithm.";
    return false;
  }
  std::string remote_fingerprint_value(reinterpret_cast<const char*>(digest),
                                       digest_len);
  // Once we have the local certificate, the same remote fingerprint can be set
  // multiple times. This may happen during renegotiation.
  if (remote_fingerprint_ &&
      remote_fingerprint_->value == remote_fingerprint_value &&
      remote_fingerprint_->algorithm == digest_alg) {
    LOG_J(LS_INFO, this)
        << "Ignoring identical remote fingerprint and algorithm";
    return true;
  }
  remote_fingerprint_ = rtc::Optional<RemoteFingerprint>(RemoteFingerprint());
  remote_fingerprint_->value = remote_fingerprint_value;
  remote_fingerprint_->algorithm = digest_alg;
  return true;
}

bool QuicTransportChannel::ExportKeyingMaterial(const std::string& label,
                                                const uint8_t* context,
                                                size_t context_len,
                                                bool use_context,
                                                uint8_t* result,
                                                size_t result_len) {
  std::string quic_context(reinterpret_cast<const char*>(context), context_len);
  std::string quic_result;
  if (!quic_->ExportKeyingMaterial(label, quic_context, result_len,
                                   &quic_result)) {
    return false;
  }
  quic_result.copy(reinterpret_cast<char*>(result), result_len);
  return true;
}

bool QuicTransportChannel::GetSrtpCryptoSuite(int* cipher) {
  *cipher = rtc::SRTP_AES128_CM_SHA1_80;
  return true;
}

// Called from upper layers to send a media packet.
int QuicTransportChannel::SendPacket(const char* data,
                                     size_t size,
                                     const rtc::PacketOptions& options,
                                     int flags) {
  if ((flags & PF_SRTP_BYPASS) && IsRtpPacket(data, size)) {
    return channel_->SendPacket(data, size, options);
  }
  LOG(LS_ERROR) << "Failed to send an invalid SRTP bypass packet using QUIC.";
  return -1;
}

// The state transition logic here is as follows:
//     - Before the QUIC handshake is complete, the QUIC channel is unwritable.
//     - When |channel_| goes writable we start the QUIC handshake.
//     - Once the QUIC handshake completes, the state is that of the
//       |channel_| again.
void QuicTransportChannel::OnWritableState(TransportChannel* channel) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(channel == channel_.get());
  LOG_J(LS_VERBOSE, this)
      << "QuicTransportChannel: channel writable state changed to "
      << channel_->writable();
  switch (quic_state_) {
    case QUIC_TRANSPORT_NEW:
      // Start the QUIC handshake when |channel_| is writable.
      // This will fail if the SSL role or remote fingerprint are not set.
      // Otherwise failure could result from network or QUIC errors.
      MaybeStartQuic();
      break;
    case QUIC_TRANSPORT_CONNECTED:
      // Note: SignalWritableState fired by set_writable.
      set_writable(channel_->writable());
      if (HasDataToWrite()) {
        OnCanWrite();
      }
      break;
    case QUIC_TRANSPORT_CONNECTING:
      // This channel is not writable until the QUIC handshake finishes. It
      // might have been write blocked.
      if (HasDataToWrite()) {
        OnCanWrite();
      }
      break;
    case QUIC_TRANSPORT_CLOSED:
      // TODO(mikescarlett): Allow the QUIC connection to be reset if it drops
      // due to a non-failure.
      break;
  }
}

void QuicTransportChannel::OnReceivingState(TransportChannel* channel) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(channel == channel_.get());
  LOG_J(LS_VERBOSE, this)
      << "QuicTransportChannel: channel receiving state changed to "
      << channel_->receiving();
  if (quic_state_ == QUIC_TRANSPORT_CONNECTED) {
    // Note: SignalReceivingState fired by set_receiving.
    set_receiving(channel_->receiving());
  }
}

void QuicTransportChannel::OnReadPacket(TransportChannel* channel,
                                        const char* data,
                                        size_t size,
                                        const rtc::PacketTime& packet_time,
                                        int flags) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  RTC_DCHECK(channel == channel_.get());
  RTC_DCHECK(flags == 0);

  switch (quic_state_) {
    case QUIC_TRANSPORT_NEW:
      // This would occur if other peer is ready to start QUIC but this peer
      // hasn't started QUIC.
      LOG_J(LS_INFO, this) << "Dropping packet received before QUIC started.";
      break;
    case QUIC_TRANSPORT_CONNECTING:
    case QUIC_TRANSPORT_CONNECTED:
      // We should only get QUIC or SRTP packets; STUN's already been demuxed.
      // Is this potentially a QUIC packet?
      if (IsQuicPacket(data, size)) {
        if (!HandleQuicPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Failed to handle QUIC packet.";
          return;
        }
      } else {
        // If this is an RTP packet, signal upwards as a bypass packet.
        if (!IsRtpPacket(data, size)) {
          LOG_J(LS_ERROR, this)
              << "Received unexpected non-QUIC, non-RTP packet.";
          return;
        }
        SignalReadPacket(this, data, size, packet_time, PF_SRTP_BYPASS);
      }
      break;
    case QUIC_TRANSPORT_CLOSED:
      // This shouldn't be happening. Drop the packet.
      break;
  }
}

void QuicTransportChannel::OnSentPacket(TransportChannel* channel,
                                        const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  SignalSentPacket(this, sent_packet);
}

void QuicTransportChannel::OnReadyToSend(TransportChannel* channel) {
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void QuicTransportChannel::OnGatheringState(TransportChannelImpl* channel) {
  RTC_DCHECK(channel == channel_.get());
  SignalGatheringState(this);
}

void QuicTransportChannel::OnCandidateGathered(TransportChannelImpl* channel,
                                               const Candidate& c) {
  RTC_DCHECK(channel == channel_.get());
  SignalCandidateGathered(this, c);
}

void QuicTransportChannel::OnRoleConflict(TransportChannelImpl* channel) {
  RTC_DCHECK(channel == channel_.get());
  SignalRoleConflict(this);
}

void QuicTransportChannel::OnRouteChange(TransportChannel* channel,
                                         const Candidate& candidate) {
  RTC_DCHECK(channel == channel_.get());
  SignalRouteChange(this, candidate);
}

void QuicTransportChannel::OnSelectedCandidatePairChanged(
    TransportChannel* channel,
    CandidatePairInterface* selected_candidate_pair,
    int last_sent_packet_id,
    bool ready_to_send) {
  RTC_DCHECK(channel == channel_.get());
  SignalSelectedCandidatePairChanged(this, selected_candidate_pair,
                                     last_sent_packet_id, ready_to_send);
}

void QuicTransportChannel::OnChannelStateChanged(
    TransportChannelImpl* channel) {
  RTC_DCHECK(channel == channel_.get());
  SignalStateChanged(this);
}

bool QuicTransportChannel::MaybeStartQuic() {
  if (!channel_->writable()) {
    LOG_J(LS_ERROR, this) << "Couldn't start QUIC handshake.";
    return false;
  }
  if (!CreateQuicSession() || !StartQuicHandshake()) {
    LOG_J(LS_WARNING, this)
        << "Underlying channel is writable but cannot start "
           "the QUIC handshake.";
    return false;
  }
  // Verify connection is not closed due to QUIC bug or network failure.
  // A closed connection should not happen since |channel_| is writable.
  if (!quic_->connection()->connected()) {
    LOG_J(LS_ERROR, this)
        << "QUIC connection should not be closed if underlying "
           "channel is writable.";
    return false;
  }
  // Indicate that |quic_| is ready to receive QUIC packets.
  set_quic_state(QUIC_TRANSPORT_CONNECTING);
  return true;
}

bool QuicTransportChannel::CreateQuicSession() {
  if (!ssl_role_ || !remote_fingerprint_) {
    return false;
  }
  net::Perspective perspective = (*ssl_role_ == rtc::SSL_CLIENT)
                                     ? net::Perspective::IS_CLIENT
                                     : net::Perspective::IS_SERVER;
  bool owns_writer = false;
  std::unique_ptr<net::QuicConnection> connection(new net::QuicConnection(
      kConnectionId, kConnectionIpEndpoint, &helper_, this, owns_writer,
      perspective, net::QuicSupportedVersions()));
  quic_.reset(new QuicSession(std::move(connection), config_));
  quic_->SignalHandshakeComplete.connect(
      this, &QuicTransportChannel::OnHandshakeComplete);
  quic_->SignalConnectionClosed.connect(
      this, &QuicTransportChannel::OnConnectionClosed);
  quic_->SignalIncomingStream.connect(this,
                                      &QuicTransportChannel::OnIncomingStream);
  return true;
}

bool QuicTransportChannel::StartQuicHandshake() {
  if (*ssl_role_ == rtc::SSL_CLIENT) {
    // Unique identifier for remote peer.
    net::QuicServerId server_id(remote_fingerprint_->value, kQuicServerPort);
    // Perform authentication of remote peer; owned by QuicCryptoClientConfig.
    // TODO(mikescarlett): Actually verify proof.
    net::ProofVerifier* proof_verifier = new InsecureProofVerifier();
    quic_crypto_client_config_.reset(
        new net::QuicCryptoClientConfig(proof_verifier));
    net::QuicCryptoClientStream* crypto_stream =
        new net::QuicCryptoClientStream(server_id, quic_.get(),
                                        new net::ProofVerifyContext(),
                                        quic_crypto_client_config_.get(), this);
    quic_->StartClientHandshake(crypto_stream);
    LOG_J(LS_INFO, this) << "QuicTransportChannel: Started client handshake.";
  } else {
    RTC_DCHECK_EQ(*ssl_role_, rtc::SSL_SERVER);
    // Provide credentials to remote peer; owned by QuicCryptoServerConfig.
    // TODO(mikescarlett): Actually provide credentials.
    net::ProofSource* proof_source = new DummyProofSource();
    // Input keying material to HKDF, per http://tools.ietf.org/html/rfc5869.
    // This is pseudorandom so that HKDF-Extract outputs a pseudorandom key,
    // since QuicCryptoServerConfig does not use a salt value.
    std::string source_address_token_secret;
    if (!rtc::CreateRandomString(kInputKeyingMaterialLength,
                                 &source_address_token_secret)) {
      LOG_J(LS_ERROR, this)
          << "Error generating input keying material for HKDF.";
      return false;
    }
    quic_crypto_server_config_.reset(new net::QuicCryptoServerConfig(
        source_address_token_secret, helper_.GetRandomGenerator(),
        proof_source));
    // Provide server with serialized config string to prove ownership.
    net::QuicCryptoServerConfig::ConfigOptions options;
    quic_crypto_server_config_->AddDefaultConfig(helper_.GetRandomGenerator(),
                                                 helper_.GetClock(), options);
    quic_compressed_certs_cache_.reset(new net::QuicCompressedCertsCache(
        net::QuicCompressedCertsCache::kQuicCompressedCertsCacheSize));
    // TODO(mikescarlett): Add support for stateless rejects.
    bool use_stateless_rejects_if_peer_supported = false;
    net::QuicCryptoServerStream* crypto_stream =
        new net::QuicCryptoServerStream(quic_crypto_server_config_.get(),
                                        quic_compressed_certs_cache_.get(),
                                        use_stateless_rejects_if_peer_supported,
                                        quic_.get());
    quic_->StartServerHandshake(crypto_stream);
    LOG_J(LS_INFO, this) << "QuicTransportChannel: Started server handshake.";
  }
  return true;
}

bool QuicTransportChannel::HandleQuicPacket(const char* data, size_t size) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  return quic_->OnReadPacket(data, size);
}

net::WriteResult QuicTransportChannel::WritePacket(
    const char* buffer,
    size_t buf_len,
    const net::IPAddress& self_address,
    const net::IPEndPoint& peer_address,
    net::PerPacketOptions* options) {
  // QUIC should never call this if IsWriteBlocked, but just in case...
  if (IsWriteBlocked()) {
    return net::WriteResult(net::WRITE_STATUS_BLOCKED, EWOULDBLOCK);
  }
  // TODO(mikescarlett): Figure out how to tell QUIC "I dropped your packet, but
  // don't block" without the QUIC connection tearing itself down.
  int sent = channel_->SendPacket(buffer, buf_len, rtc::PacketOptions());
  int bytes_written = sent > 0 ? sent : 0;
  return net::WriteResult(net::WRITE_STATUS_OK, bytes_written);
}

// TODO(mikescarlett): Implement check for whether |channel_| is currently
// write blocked so that |quic_| does not try to write packet. This is
// necessary because |channel_| can be writable yet write blocked and
// channel_->GetError() is not flushed when there is no error.
bool QuicTransportChannel::IsWriteBlocked() const {
  return !channel_->writable();
}

void QuicTransportChannel::OnHandshakeComplete() {
  set_quic_state(QUIC_TRANSPORT_CONNECTED);
  set_writable(true);
  // OnReceivingState might have been called before the QUIC channel was
  // connected, in which case the QUIC channel is now receiving.
  if (channel_->receiving()) {
    set_receiving(true);
  }
}

void QuicTransportChannel::OnConnectionClosed(net::QuicErrorCode error,
                                              bool from_peer) {
  LOG_J(LS_INFO, this) << "Connection closed by "
                       << (from_peer ? "other" : "this") << " peer "
                       << "with QUIC error " << error;
  // TODO(mikescarlett): Allow the QUIC session to be reset when the connection
  // does not close due to failure.
  set_quic_state(QUIC_TRANSPORT_CLOSED);
  set_writable(false);
  SignalClosed();
}

void QuicTransportChannel::OnProofValid(
    const net::QuicCryptoClientConfig::CachedState& cached) {
  LOG_J(LS_INFO, this) << "Cached proof marked valid";
}

void QuicTransportChannel::OnProofVerifyDetailsAvailable(
    const net::ProofVerifyDetails& verify_details) {
  LOG_J(LS_INFO, this) << "Proof verify details available from"
                       << " QuicCryptoClientStream";
}

bool QuicTransportChannel::HasDataToWrite() const {
  return quic_ && quic_->HasDataToWrite();
}

void QuicTransportChannel::OnCanWrite() {
  RTC_DCHECK(quic_ != nullptr);
  quic_->connection()->OnCanWrite();
}

void QuicTransportChannel::set_quic_state(QuicTransportState state) {
  LOG_J(LS_VERBOSE, this) << "set_quic_state from:" << quic_state_ << " to "
                          << state;
  quic_state_ = state;
}

ReliableQuicStream* QuicTransportChannel::CreateQuicStream() {
  if (quic_) {
    net::SpdyPriority priority = 0;  // Priority of the QUIC stream
    return quic_->CreateOutgoingDynamicStream(priority);
  }
  return nullptr;
}

void QuicTransportChannel::OnIncomingStream(ReliableQuicStream* stream) {
  SignalIncomingStream(stream);
}

}  // namespace cricket
