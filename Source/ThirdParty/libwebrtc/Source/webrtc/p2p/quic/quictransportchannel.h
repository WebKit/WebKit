/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_QUIC_QUICTRANSPORTCHANNEL_H_
#define WEBRTC_P2P_QUIC_QUICTRANSPORTCHANNEL_H_

#include <memory>
#include <string>
#include <vector>

#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_packet_writer.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/p2p/quic/quicconnectionhelper.h"
#include "webrtc/p2p/quic/quicsession.h"

namespace cricket {

enum QuicTransportState {
  // Haven't started QUIC handshake.
  QUIC_TRANSPORT_NEW = 0,
  // Started QUIC handshake.
  QUIC_TRANSPORT_CONNECTING,
  // Negotiated, and has an encrypted connection.
  QUIC_TRANSPORT_CONNECTED,
  // QUIC connection closed due to handshake failure or explicit shutdown.
  QUIC_TRANSPORT_CLOSED,
};

// QuicTransportChannel uses the QUIC protocol to establish encryption with
// another peer, wrapping an existing TransportChannelImpl instance
// (e.g a P2PTransportChannel) responsible for connecting peers.
// Once the wrapped transport channel is connected, QuicTransportChannel
// negotiates the crypto handshake and establishes SRTP keying material.
//
// How it works:
//
//   QuicTransportChannel {
//       QuicSession* quic_;
//       TransportChannelImpl* channel_;
//   }
//
//   - Data written to SendPacket() is passed directly to |channel_| if it is
//     an SRTP packet with the PF_SRTP_BYPASS flag.
//
//   - |quic_| passes outgoing packets to WritePacket(), which transfers them
//     to |channel_| to be sent across the network.
//
//   - Data which comes into QuicTransportChannel::OnReadPacket is checked to
//     see if it is QUIC, and if it is, passed to |quic_|. SRTP packets are
//     signaled upwards as bypass packets.
//
//   - When the QUIC handshake is completed, quic_state() returns
//     QUIC_TRANSPORT_CONNECTED and SRTP keying material can be exported.
//
//   - CreateQuicStream() creates an outgoing QUIC stream. Once the local peer
//     sends data from this stream, the remote peer emits SignalIncomingStream
//     with a QUIC stream of the same id to handle received data.
//
// TODO(mikescarlett): Implement secure QUIC handshake and 0-RTT handshakes.
class QuicTransportChannel : public TransportChannelImpl,
                             public net::QuicPacketWriter,
                             public net::QuicCryptoClientStream::ProofHandler {
 public:
  // |channel| - the TransportChannelImpl we are wrapping.
  explicit QuicTransportChannel(TransportChannelImpl* channel);
  ~QuicTransportChannel() override;

  // TransportChannel overrides.
  // TODO(mikescarlett): Implement certificate authentication.
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;
  // TODO(mikescarlett): Implement fingerprint authentication.
  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;
  // TODO(mikescarlett): Remove this DTLS-specific method when TransportChannel
  // does not require defining it.
  bool IsDtlsActive() const override { return true; }
  // Sends a RTP packet if the PF_SRTP_BYPASS flag is set.
  int SendPacket(const char* data,
                 size_t size,
                 const rtc::PacketOptions& options,
                 int flags) override;
  // Sets up the ciphers to use for SRTP.
  // TODO(mikescarlett): Use SRTP ciphers for negotiation.
  bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) override {
    return true;
  }
  // Determines which SRTP cipher was negotiated.
  // TODO(mikescarlett): Implement QUIC cipher negotiation. This currently
  // returns SRTP_AES128_CM_SHA1_80.
  bool GetSrtpCryptoSuite(int* cipher) override;
  bool SetSslRole(rtc::SSLRole role) override;
  bool GetSslRole(rtc::SSLRole* role) const override;
  // Determines which SSL cipher was negotiated.
  // TODO(mikescarlett): Implement QUIC cipher negotiation.
  bool GetSslCipherSuite(int* cipher) override { return false; }
  // Once QUIC is established (i.e., |quic_state_| is QUIC_TRANSPORT_CONNECTED),
  // this extracts the keys negotiated during the QUIC handshake, for use
  // in external encryption such as for extracting SRTP keys.
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;
  // TODO(mikescarlett): Remove this method once TransportChannel does not
  // require defining it.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const override {
    return nullptr;
  }

  // TransportChannelImpl overrides that we forward to the wrapped transport.
  void SetIceRole(IceRole role) override { channel_->SetIceRole(role); }
  IceRole GetIceRole() const override { return channel_->GetIceRole(); }
  int SetOption(rtc::Socket::Option opt, int value) override {
    return channel_->SetOption(opt, value);
  }
  bool GetOption(rtc::Socket::Option opt, int* value) override {
    return channel_->GetOption(opt, value);
  }
  int GetError() override { return channel_->GetError(); }
  bool GetStats(ConnectionInfos* infos) override {
    return channel_->GetStats(infos);
  }
  const std::string SessionId() const override { return channel_->SessionId(); }
  TransportChannelState GetState() const override {
    return channel_->GetState();
  }
  void SetIceTiebreaker(uint64_t tiebreaker) override {
    channel_->SetIceTiebreaker(tiebreaker);
  }
  void SetIceParameters(const IceParameters& ice_params) override {
    channel_->SetIceParameters(ice_params);
  }
  void SetRemoteIceParameters(const IceParameters& ice_params) override {
    channel_->SetRemoteIceParameters(ice_params);
  }
  void SetRemoteIceMode(IceMode mode) override {
    channel_->SetRemoteIceMode(mode);
  }
  void MaybeStartGathering() override { channel_->MaybeStartGathering(); }
  IceGatheringState gathering_state() const override {
    return channel_->gathering_state();
  }
  void AddRemoteCandidate(const Candidate& candidate) override {
    channel_->AddRemoteCandidate(candidate);
  }
  void RemoveRemoteCandidate(const Candidate& candidate) override {
    channel_->RemoveRemoteCandidate(candidate);
  }
  void SetIceConfig(const IceConfig& config) override {
    channel_->SetIceConfig(config);
  }

  // QuicPacketWriter overrides.
  // Called from net::QuicConnection when |quic_| has packets to write.
  net::WriteResult WritePacket(const char* buffer,
                               size_t buf_len,
                               const net::IPAddress& self_address,
                               const net::IPEndPoint& peer_address,
                               net::PerPacketOptions* options) override;
  // Whether QuicTransportChannel buffers data when unable to write. If this is
  // set to false, then net::QuicConnection buffers unsent packets.
  bool IsWriteBlockedDataBuffered() const override { return false; }
  // Whether QuicTransportChannel is write blocked. If this returns true,
  // outgoing QUIC packets are queued by net::QuicConnection until
  // QuicTransportChannel::OnCanWrite() is called.
  bool IsWriteBlocked() const override;
  // Maximum size of the QUIC packet which can be written.
  net::QuicByteCount GetMaxPacketSize(
      const net::IPEndPoint& peer_address) const override {
    return net::kMaxPacketSize;
  }
  // This method is not used -- call set_writable(bool writable) instead.
  // TODO(miekscarlett): Remove this method once QuicPacketWriter does not
  // require defining it.
  void SetWritable() override {}

  // QuicCryptoClientStream::ProofHandler overrides.
  // Called by client crypto handshake when cached proof is marked valid.
  void OnProofValid(
      const net::QuicCryptoClientConfig::CachedState& cached) override;
  // Called by the client crypto handshake when proof verification details
  // become available, either because proof verification is complete, or when
  // cached details are used.
  void OnProofVerifyDetailsAvailable(
      const net::ProofVerifyDetails& verify_details) override;

  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override {
    channel_->SetMetricsObserver(observer);
  }

  // Returns true if |quic_| has queued data which wasn't written due
  // to |channel_| being write blocked.
  bool HasDataToWrite() const;
  // Writes queued data for |quic_| when |channel_| is no longer write blocked.
  void OnCanWrite();
  // Connectivity state of QuicTransportChannel.
  QuicTransportState quic_state() const { return quic_state_; }
  // Creates a new QUIC stream that can send data.
  ReliableQuicStream* CreateQuicStream();

  TransportChannelImpl* ice_transport_channel() { return channel_.get(); }

  // Emitted when |quic_| creates a QUIC stream to receive data from the remote
  // peer, when the stream did not exist previously.
  sigslot::signal1<ReliableQuicStream*> SignalIncomingStream;
  // Emitted when the QuicTransportChannel state becomes QUIC_TRANSPORT_CLOSED.
  sigslot::signal0<> SignalClosed;

 private:
  // Fingerprint of remote peer.
  struct RemoteFingerprint {
    std::string value;
    std::string algorithm;
  };

  // Callbacks for |channel_|.
  void OnReadableState(TransportChannel* channel);
  void OnWritableState(TransportChannel* channel);
  void OnReadPacket(TransportChannel* channel,
                    const char* data,
                    size_t size,
                    const rtc::PacketTime& packet_time,
                    int flags);
  void OnSentPacket(TransportChannel* channel,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(TransportChannel* channel);
  void OnReceivingState(TransportChannel* channel);
  void OnGatheringState(TransportChannelImpl* channel);
  void OnCandidateGathered(TransportChannelImpl* channel, const Candidate& c);
  void OnRoleConflict(TransportChannelImpl* channel);
  void OnRouteChange(TransportChannel* channel, const Candidate& candidate);
  void OnSelectedCandidatePairChanged(
      TransportChannel* channel,
      CandidatePairInterface* selected_candidate_pair,
      int last_sent_packet_id,
      bool ready_to_send);
  void OnChannelStateChanged(TransportChannelImpl* channel);

  // Callbacks for |quic_|.
  // Called when |quic_| has established the crypto handshake.
  void OnHandshakeComplete();
  // Called when |quic_| has closed the connection.
  void OnConnectionClosed(net::QuicErrorCode error, bool from_peer);
  // Called when |quic_| has created a new QUIC stream for incoming data.
  void OnIncomingStream(ReliableQuicStream* stream);

  // Called by OnReadPacket() when a QUIC packet is received.
  bool HandleQuicPacket(const char* data, size_t size);
  // Sets up the QUIC handshake.
  bool MaybeStartQuic();
  // Creates the QUIC connection and |quic_|.
  bool CreateQuicSession();
  // Creates the crypto stream and initializes the handshake.
  bool StartQuicHandshake();
  // Sets the QuicTransportChannel connectivity state.
  void set_quic_state(QuicTransportState state);

  // Everything should occur on this thread.
  rtc::Thread* network_thread_;
  // Underlying channel which is responsible for connecting with the remote peer
  // and sending/receiving packets across the network.
  std::unique_ptr<TransportChannelImpl> channel_;
  // Connectivity state of QuicTransportChannel.
  QuicTransportState quic_state_ = QUIC_TRANSPORT_NEW;
  // QUIC session which establishes the crypto handshake and converts data
  // to/from QUIC packets.
  std::unique_ptr<QuicSession> quic_;
  // Non-crypto config for |quic_|.
  net::QuicConfig config_;
  // Helper for net::QuicConnection that provides timing and
  // random number generation.
  QuicConnectionHelper helper_;
  // This peer's role in the QUIC crypto handshake. SSL_CLIENT implies this peer
  // initiates the handshake, while SSL_SERVER implies the remote peer initiates
  // the handshake. This must be set before we start QUIC.
  rtc::Optional<rtc::SSLRole> ssl_role_;
  // Config for QUIC crypto client stream, used when |ssl_role_| is SSL_CLIENT.
  std::unique_ptr<net::QuicCryptoClientConfig> quic_crypto_client_config_;
  // Config for QUIC crypto server stream, used when |ssl_role_| is SSL_SERVER.
  std::unique_ptr<net::QuicCryptoServerConfig> quic_crypto_server_config_;
  // Used by QUIC crypto server stream to track most recently compressed certs.
  std::unique_ptr<net::QuicCompressedCertsCache> quic_compressed_certs_cache_;
  // This peer's certificate.
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  // Fingerprint of the remote peer. This must be set before we start QUIC.
  rtc::Optional<RemoteFingerprint> remote_fingerprint_;

  RTC_DISALLOW_COPY_AND_ASSIGN(QuicTransportChannel);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_QUIC_QUICTRANSPORTCHANNEL_H_
