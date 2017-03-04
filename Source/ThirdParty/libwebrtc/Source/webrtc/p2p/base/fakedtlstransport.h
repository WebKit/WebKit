/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKEDTLSTRANSPORT_H_
#define WEBRTC_P2P_BASE_FAKEDTLSTRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/fakesslidentity.h"
#include "webrtc/p2p/base/dtlstransportinternal.h"
#include "webrtc/p2p/base/fakeicetransport.h"

namespace cricket {

// Fake DTLS transport which is implemented by wrapping a fake ICE transport.
// Doesn't interact directly with fake ICE transport for anything other than
// sending packets.
class FakeDtlsTransport : public DtlsTransportInternal {
 public:
  explicit FakeDtlsTransport(FakeIceTransport* ice_transport)
      : ice_transport_(ice_transport),
        transport_name_(ice_transport->transport_name()),
        component_(ice_transport->component()),
        dtls_fingerprint_("", nullptr, 0) {
    ice_transport_->SignalReadPacket.connect(
        this, &FakeDtlsTransport::OnIceTransportReadPacket);
  }

  // If this constructor is called, a new fake ICE transport will be created,
  // and this FakeDtlsTransport will take the ownership.
  explicit FakeDtlsTransport(const std::string& name, int component)
      : owned_ice_transport_(new FakeIceTransport(name, component)),
        transport_name_(owned_ice_transport_->transport_name()),
        component_(owned_ice_transport_->component()),
        dtls_fingerprint_("", nullptr, 0) {
    ice_transport_ = owned_ice_transport_.get();
    ice_transport_->SignalReadPacket.connect(
        this, &FakeDtlsTransport::OnIceTransportReadPacket);
  }

  ~FakeDtlsTransport() override {
    if (dest_ && dest_->dest_ == this) {
      dest_->dest_ = nullptr;
    }
  }

  // Get inner fake ICE transport.
  FakeIceTransport* fake_ice_transport() { return ice_transport_; }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { ice_transport_->SetAsync(async); }
  void SetAsyncDelay(int delay_ms) { ice_transport_->SetAsyncDelay(delay_ms); }

  // SetWritable, SetReceiving and SetDestination are the main methods that can
  // be used for testing, to simulate connectivity or lack thereof.
  void SetWritable(bool writable) {
    ice_transport_->SetWritable(writable);
    set_writable(writable);
  }
  void SetReceiving(bool receiving) {
    ice_transport_->SetReceiving(receiving);
    set_receiving(receiving);
  }

  // Simulates the two DTLS transports connecting to each other.
  // If |asymmetric| is true this method only affects this FakeDtlsTransport.
  // If false, it affects |dest| as well.
  void SetDestination(FakeDtlsTransport* dest, bool asymmetric = false) {
    if (dest == dest_) {
      return;
    }
    RTC_DCHECK(!dest || !dest_)
        << "Changing fake destination from one to another is not supported.";
    if (dest && !dest_) {
      // This simulates the DTLS handshake.
      dest_ = dest;
      if (local_cert_ && dest_->local_cert_) {
        do_dtls_ = true;
        NegotiateSrtpCiphers();
      }
      SetWritable(true);
      if (!asymmetric) {
        dest->SetDestination(this, true);
      }
      ice_transport_->SetDestination(
          static_cast<FakeIceTransport*>(dest->ice_transport()), asymmetric);
    } else {
      // Simulates loss of connectivity, by asymmetrically forgetting dest_.
      dest_ = nullptr;
      SetWritable(false);
      ice_transport_->SetDestination(nullptr, asymmetric);
    }
  }

  // Fake DtlsTransportInternal implementation.
  DtlsTransportState dtls_state() const override { return dtls_state_; }
  const std::string& transport_name() const override { return transport_name_; }
  int component() const override { return component_; }
  const rtc::SSLFingerprint& dtls_fingerprint() const {
    return dtls_fingerprint_;
  }
  bool SetRemoteFingerprint(const std::string& alg,
                            const uint8_t* digest,
                            size_t digest_len) override {
    dtls_fingerprint_ = rtc::SSLFingerprint(alg, digest, digest_len);
    return true;
  }
  bool SetSslRole(rtc::SSLRole role) override {
    ssl_role_ = role;
    return true;
  }
  bool GetSslRole(rtc::SSLRole* role) const override {
    *role = ssl_role_;
    return true;
  }
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    local_cert_ = certificate;
    return true;
  }
  void SetRemoteSSLCertificate(rtc::FakeSSLCertificate* cert) {
    remote_cert_ = cert;
  }
  bool IsDtlsActive() const override { return do_dtls_; }
  bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) override {
    srtp_ciphers_ = ciphers;
    return true;
  }
  bool GetSrtpCryptoSuite(int* crypto_suite) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      *crypto_suite = chosen_crypto_suite_;
      return true;
    }
    return false;
  }
  bool GetSslCipherSuite(int* cipher_suite) override { return false; }
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override {
    return local_cert_;
  }
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const override {
    return remote_cert_ ? std::unique_ptr<rtc::SSLCertificate>(
                              remote_cert_->GetReference())
                        : nullptr;
  }
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      memset(result, 0xff, result_len);
      return true;
    }

    return false;
  }
  void set_ssl_max_protocol_version(rtc::SSLProtocolVersion version) {
    ssl_max_version_ = version;
  }
  rtc::SSLProtocolVersion ssl_max_protocol_version() const {
    return ssl_max_version_;
  }
  bool SetSrtpCiphers(const std::vector<std::string>& ciphers) override {
    std::vector<int> crypto_suites;
    for (const auto cipher : ciphers) {
      crypto_suites.push_back(rtc::SrtpCryptoSuiteFromName(cipher));
    }
    return SetSrtpCryptoSuites(crypto_suites);
  }

  IceTransportInternal* ice_transport() override { return ice_transport_; }

  // PacketTransportInternal implementation, which passes through to fake ICE
  // transport for sending actual packets.
  bool writable() const override { return writable_; }
  bool receiving() const override { return receiving_; }
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    // We expect only SRTP packets to be sent through this interface.
    if (flags != PF_SRTP_BYPASS && flags != 0) {
      return -1;
    }
    return ice_transport_->SendPacket(data, len, options, flags);
  }
  int SetOption(rtc::Socket::Option opt, int value) override {
    return ice_transport_->SetOption(opt, value);
  }
  bool GetOption(rtc::Socket::Option opt, int* value) override {
    return ice_transport_->GetOption(opt, value);
  }
  int GetError() override { return ice_transport_->GetError(); }

 private:
  void OnIceTransportReadPacket(PacketTransportInternal* ice_,
                                const char* data,
                                size_t len,
                                const rtc::PacketTime& time,
                                int flags) {
    SignalReadPacket(this, data, len, time, flags);
  }

  void NegotiateSrtpCiphers() {
    for (std::vector<int>::const_iterator it1 = srtp_ciphers_.begin();
         it1 != srtp_ciphers_.end(); ++it1) {
      for (std::vector<int>::const_iterator it2 = dest_->srtp_ciphers_.begin();
           it2 != dest_->srtp_ciphers_.end(); ++it2) {
        if (*it1 == *it2) {
          chosen_crypto_suite_ = *it1;
          return;
        }
      }
    }
  }

  void set_receiving(bool receiving) {
    if (receiving_ == receiving) {
      return;
    }
    receiving_ = receiving;
    SignalReceivingState(this);
  }

  void set_writable(bool writable) {
    if (writable_ == writable) {
      return;
    }
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }

  FakeIceTransport* ice_transport_;
  std::unique_ptr<FakeIceTransport> owned_ice_transport_;
  std::string transport_name_;
  int component_;
  FakeDtlsTransport* dest_ = nullptr;
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert_;
  rtc::FakeSSLCertificate* remote_cert_ = nullptr;
  bool do_dtls_ = false;
  std::vector<int> srtp_ciphers_;
  int chosen_crypto_suite_ = rtc::SRTP_INVALID_CRYPTO_SUITE;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  rtc::SSLFingerprint dtls_fingerprint_;
  rtc::SSLRole ssl_role_ = rtc::SSL_CLIENT;

  DtlsTransportState dtls_state_ = DTLS_TRANSPORT_NEW;

  bool receiving_ = false;
  bool writable_ = false;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKEDTLSTRANSPORT_H_
