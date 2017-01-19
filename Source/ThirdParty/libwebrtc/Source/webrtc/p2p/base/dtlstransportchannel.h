/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_DTLSTRANSPORTCHANNEL_H_
#define WEBRTC_P2P_BASE_DTLSTRANSPORTCHANNEL_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/bufferqueue.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stream.h"

namespace rtc {
class PacketTransportInterface;
}

namespace cricket {

// A bridge between a packet-oriented/channel-type interface on
// the bottom and a StreamInterface on the top.
class StreamInterfaceChannel : public rtc::StreamInterface {
 public:
  explicit StreamInterfaceChannel(TransportChannel* channel);

  // Push in a packet; this gets pulled out from Read().
  bool OnPacketReceived(const char* data, size_t size);

  // Implementations of StreamInterface
  rtc::StreamState GetState() const override { return state_; }
  void Close() override;
  rtc::StreamResult Read(void* buffer,
                         size_t buffer_len,
                         size_t* read,
                         int* error) override;
  rtc::StreamResult Write(const void* data,
                          size_t data_len,
                          size_t* written,
                          int* error) override;

 private:
  TransportChannel* channel_;  // owned by DtlsTransportChannelWrapper
  rtc::StreamState state_;
  rtc::BufferQueue packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(StreamInterfaceChannel);
};


// This class provides a DTLS SSLStreamAdapter inside a TransportChannel-style
// packet-based interface, wrapping an existing TransportChannel instance
// (e.g a P2PTransportChannel)
// Here's the way this works:
//
//   DtlsTransportChannelWrapper {
//       SSLStreamAdapter* dtls_ {
//           StreamInterfaceChannel downward_ {
//               TransportChannelImpl* channel_;
//           }
//       }
//   }
//
//   - Data which comes into DtlsTransportChannelWrapper from the underlying
//     channel_ via OnReadPacket() is checked for whether it is DTLS
//     or not, and if it is, is passed to DtlsTransportChannelWrapper::
//     HandleDtlsPacket, which pushes it into to downward_.
//     dtls_ is listening for events on downward_, so it immediately calls
//     downward_->Read().
//
//   - Data written to DtlsTransportChannelWrapper is passed either to
//      downward_ or directly to channel_, depending on whether DTLS is
//     negotiated and whether the flags include PF_SRTP_BYPASS
//
//   - The SSLStreamAdapter writes to downward_->Write()
//     which translates it into packet writes on channel_.
class DtlsTransportChannelWrapper : public TransportChannelImpl {
 public:
  // The parameters here are:
  // channel -- the TransportChannel we are wrapping
  explicit DtlsTransportChannelWrapper(TransportChannelImpl* channel);
  ~DtlsTransportChannelWrapper() override;

  void SetIceRole(IceRole role) override { channel_->SetIceRole(role); }
  IceRole GetIceRole() const override { return channel_->GetIceRole(); }
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;

  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;

  // Returns false if no local certificate was set, or if the peer doesn't
  // support DTLS.
  bool IsDtlsActive() const override { return dtls_active_; }

  // Called to send a packet (via DTLS, if turned on).
  int SendPacket(const char* data,
                 size_t size,
                 const rtc::PacketOptions& options,
                 int flags) override;

  // TransportChannel calls that we forward to the wrapped transport.
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

  virtual bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version);

  // Set up the ciphers to use for DTLS-SRTP. If this method is not called
  // before DTLS starts, or |ciphers| is empty, SRTP keys won't be negotiated.
  // This method should be called before SetupDtls.
  bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) override;

  // Find out which DTLS-SRTP cipher was negotiated
  bool GetSrtpCryptoSuite(int* cipher) override;

  bool GetSslRole(rtc::SSLRole* role) const override;
  bool SetSslRole(rtc::SSLRole role) override;

  // Find out which DTLS cipher was negotiated
  bool GetSslCipherSuite(int* cipher) override;

  // Once DTLS has been established, this method retrieves the certificate in
  // use by the remote peer, for use in external identity verification.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate() const override;

  // Once DTLS has established (i.e., this channel is writable), this method
  // extracts the keys negotiated during the DTLS handshake, for use in external
  // encryption. DTLS-SRTP uses this to extract the needed SRTP keys.
  // See the SSLStreamAdapter documentation for info on the specific parameters.
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override {
    return (dtls_.get()) ? dtls_->ExportKeyingMaterial(label, context,
                                                       context_len,
                                                       use_context,
                                                       result, result_len)
        : false;
  }

  // TransportChannelImpl calls.
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

  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override {
    channel_->SetMetricsObserver(observer);
  }

  void SetIceConfig(const IceConfig& config) override {
    channel_->SetIceConfig(config);
  }

  // Needed by DtlsTransport.
  TransportChannelImpl* channel() { return channel_; }

  // For informational purposes. Tells if the DTLS handshake has finished.
  // This may be true even if writable() is false, if the remote fingerprint
  // has not yet been verified.
  bool IsDtlsConnected();

 private:
  void OnWritableState(rtc::PacketTransportInterface* transport);
  void OnReadPacket(rtc::PacketTransportInterface* transport,
                    const char* data,
                    size_t size,
                    const rtc::PacketTime& packet_time,
                    int flags);
  void OnSentPacket(rtc::PacketTransportInterface* transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(rtc::PacketTransportInterface* transport);
  void OnReceivingState(rtc::PacketTransportInterface* transport);
  void OnDtlsEvent(rtc::StreamInterface* stream_, int sig, int err);
  bool SetupDtls();
  void MaybeStartDtls();
  bool HandleDtlsPacket(const char* data, size_t size);
  void OnGatheringState(TransportChannelImpl* channel);
  void OnCandidateGathered(TransportChannelImpl* channel, const Candidate& c);
  void OnCandidatesRemoved(TransportChannelImpl* channel,
                           const Candidates& candidates);
  void OnRoleConflict(TransportChannelImpl* channel);
  void OnRouteChange(TransportChannel* channel, const Candidate& candidate);
  void OnSelectedCandidatePairChanged(
      TransportChannel* channel,
      CandidatePairInterface* selected_candidate_pair,
      int last_sent_packet_id,
      bool ready_to_send);
  void OnChannelStateChanged(TransportChannelImpl* channel);
  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  rtc::Thread* network_thread_;  // Everything should occur on this thread.
  // Underlying channel, not owned by this class.
  TransportChannelImpl* const channel_;
  std::unique_ptr<rtc::SSLStreamAdapter> dtls_;  // The DTLS stream
  StreamInterfaceChannel* downward_;  // Wrapper for channel_, owned by dtls_.
  std::vector<int> srtp_ciphers_;     // SRTP ciphers to use with DTLS.
  bool dtls_active_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  rtc::SSLRole ssl_role_;
  rtc::SSLProtocolVersion ssl_max_version_;
  rtc::Buffer remote_fingerprint_value_;
  std::string remote_fingerprint_algorithm_;

  // Cached DTLS ClientHello packet that was received before we started the
  // DTLS handshake. This could happen if the hello was received before the
  // transport channel became writable, or before a remote fingerprint was
  // received.
  rtc::Buffer cached_client_hello_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DtlsTransportChannelWrapper);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_DTLSTRANSPORTCHANNEL_H_
