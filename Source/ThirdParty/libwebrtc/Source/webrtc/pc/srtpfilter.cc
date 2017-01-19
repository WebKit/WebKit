/*
 *  Copyright 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/srtpfilter.h"

#include <string.h>

#include <algorithm>

#include "third_party/libsrtp/include/srtp.h"
#include "third_party/libsrtp/include/srtp_priv.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/media/base/rtputils.h"
#include "webrtc/pc/externalhmac.h"

namespace cricket {

#ifndef HAVE_SRTP

// This helper function is used on systems that don't (yet) have SRTP,
// to log that the functions that require it won't do anything.
namespace {
bool SrtpNotAvailable(const char *func) {
  LOG(LS_ERROR) << func << ": SRTP is not available on your system.";
  return false;
}
}  // anonymous namespace

#endif  // !HAVE_SRTP

// NOTE: This is called from ChannelManager D'tor.
void ShutdownSrtp() {
#ifdef HAVE_SRTP
  // If srtp_dealloc is not executed then this will clear all existing sessions.
  // This should be called when application is shutting down.
  SrtpSession::Terminate();
#endif
}

SrtpFilter::SrtpFilter()
    : state_(ST_INIT),
      signal_silent_time_in_ms_(0) {
}

SrtpFilter::~SrtpFilter() {
}

bool SrtpFilter::IsActive() const {
  return state_ >= ST_ACTIVE;
}

bool SrtpFilter::SetOffer(const std::vector<CryptoParams>& offer_params,
                          ContentSource source) {
  if (!ExpectOffer(source)) {
     LOG(LS_ERROR) << "Wrong state to update SRTP offer";
     return false;
  }
  return StoreParams(offer_params, source);
}

bool SrtpFilter::SetAnswer(const std::vector<CryptoParams>& answer_params,
                           ContentSource source) {
  return DoSetAnswer(answer_params, source, true);
}

bool SrtpFilter::SetProvisionalAnswer(
    const std::vector<CryptoParams>& answer_params,
    ContentSource source) {
  return DoSetAnswer(answer_params, source, false);
}

bool SrtpFilter::SetRtpParams(int send_cs,
                              const uint8_t* send_key,
                              int send_key_len,
                              int recv_cs,
                              const uint8_t* recv_key,
                              int recv_key_len) {
  if (IsActive()) {
    LOG(LS_ERROR) << "Tried to set SRTP Params when filter already active";
    return false;
  }
  CreateSrtpSessions();
  if (!send_session_->SetSend(send_cs, send_key, send_key_len))
    return false;

  if (!recv_session_->SetRecv(recv_cs, recv_key, recv_key_len))
    return false;

  state_ = ST_ACTIVE;

  LOG(LS_INFO) << "SRTP activated with negotiated parameters:"
               << " send cipher_suite " << send_cs
               << " recv cipher_suite " << recv_cs;
  return true;
}

// This function is provided separately because DTLS-SRTP behaves
// differently in RTP/RTCP mux and non-mux modes.
//
// - In the non-muxed case, RTP and RTCP are keyed with different
//   keys (from different DTLS handshakes), and so we need a new
//   SrtpSession.
// - In the muxed case, they are keyed with the same keys, so
//   this function is not needed
bool SrtpFilter::SetRtcpParams(int send_cs,
                               const uint8_t* send_key,
                               int send_key_len,
                               int recv_cs,
                               const uint8_t* recv_key,
                               int recv_key_len) {
  // This can only be called once, but can be safely called after
  // SetRtpParams
  if (send_rtcp_session_ || recv_rtcp_session_) {
    LOG(LS_ERROR) << "Tried to set SRTCP Params when filter already active";
    return false;
  }

  send_rtcp_session_.reset(new SrtpSession());
  SignalSrtpError.repeat(send_rtcp_session_->SignalSrtpError);
  send_rtcp_session_->set_signal_silent_time(signal_silent_time_in_ms_);
  if (!send_rtcp_session_->SetRecv(send_cs, send_key, send_key_len))
    return false;

  recv_rtcp_session_.reset(new SrtpSession());
  SignalSrtpError.repeat(recv_rtcp_session_->SignalSrtpError);
  recv_rtcp_session_->set_signal_silent_time(signal_silent_time_in_ms_);
  if (!recv_rtcp_session_->SetRecv(recv_cs, recv_key, recv_key_len))
    return false;

  LOG(LS_INFO) << "SRTCP activated with negotiated parameters:"
               << " send cipher_suite " << send_cs
               << " recv cipher_suite " << recv_cs;

  return true;
}

bool SrtpFilter::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len);
}

bool SrtpFilter::ProtectRtp(void* p,
                            int in_len,
                            int max_len,
                            int* out_len,
                            int64_t* index) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len, index);
}

bool SrtpFilter::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtcp: SRTP not active";
    return false;
  }
  if (send_rtcp_session_) {
    return send_rtcp_session_->ProtectRtcp(p, in_len, max_len, out_len);
  } else {
    RTC_CHECK(send_session_);
    return send_session_->ProtectRtcp(p, in_len, max_len, out_len);
  }
}

bool SrtpFilter::UnprotectRtp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(recv_session_);
  return recv_session_->UnprotectRtp(p, in_len, out_len);
}

bool SrtpFilter::UnprotectRtcp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtcp: SRTP not active";
    return false;
  }
  if (recv_rtcp_session_) {
    return recv_rtcp_session_->UnprotectRtcp(p, in_len, out_len);
  } else {
    RTC_CHECK(recv_session_);
    return recv_session_->UnprotectRtcp(p, in_len, out_len);
  }
}

bool SrtpFilter::GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to GetRtpAuthParams: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  return send_session_->GetRtpAuthParams(key, key_len, tag_len);
}

bool SrtpFilter::GetSrtpOverhead(int* srtp_overhead) const {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to GetSrtpOverhead: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  *srtp_overhead = send_session_->GetSrtpOverhead();
  return true;
}

void SrtpFilter::set_signal_silent_time(int signal_silent_time_in_ms) {
  signal_silent_time_in_ms_ = signal_silent_time_in_ms;
  if (IsActive()) {
    RTC_CHECK(send_session_);
    send_session_->set_signal_silent_time(signal_silent_time_in_ms);
    RTC_CHECK(recv_session_);
    recv_session_->set_signal_silent_time(signal_silent_time_in_ms);
    if (send_rtcp_session_)
      send_rtcp_session_->set_signal_silent_time(signal_silent_time_in_ms);
    if (recv_rtcp_session_)
      recv_rtcp_session_->set_signal_silent_time(signal_silent_time_in_ms);
  }
}

bool SrtpFilter::ExpectOffer(ContentSource source) {
  return ((state_ == ST_INIT) ||
          (state_ == ST_ACTIVE) ||
          (state_  == ST_SENTOFFER && source == CS_LOCAL) ||
          (state_  == ST_SENTUPDATEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDUPDATEDOFFER && source == CS_REMOTE));
}

bool SrtpFilter::StoreParams(const std::vector<CryptoParams>& params,
                             ContentSource source) {
  offer_params_ = params;
  if (state_ == ST_INIT) {
    state_ = (source == CS_LOCAL) ? ST_SENTOFFER : ST_RECEIVEDOFFER;
  } else if (state_ == ST_ACTIVE) {
    state_ =
        (source == CS_LOCAL) ? ST_SENTUPDATEDOFFER : ST_RECEIVEDUPDATEDOFFER;
  }
  return true;
}

bool SrtpFilter::ExpectAnswer(ContentSource source) {
  return ((state_ == ST_SENTOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_SENTUPDATEDOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDUPDATEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_SENTPRANSWER_NO_CRYPTO && source == CS_LOCAL) ||
          (state_ == ST_SENTPRANSWER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDPRANSWER_NO_CRYPTO && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDPRANSWER && source == CS_REMOTE));
}

bool SrtpFilter::DoSetAnswer(const std::vector<CryptoParams>& answer_params,
                             ContentSource source,
                             bool final) {
  if (!ExpectAnswer(source)) {
    LOG(LS_ERROR) << "Invalid state for SRTP answer";
    return false;
  }

  // If the answer doesn't requests crypto complete the negotiation of an
  // unencrypted session.
  // Otherwise, finalize the parameters and apply them.
  if (answer_params.empty()) {
    if (final) {
      return ResetParams();
    } else {
      // Need to wait for the final answer to decide if
      // we should go to Active state.
      state_ = (source == CS_LOCAL) ? ST_SENTPRANSWER_NO_CRYPTO :
                                      ST_RECEIVEDPRANSWER_NO_CRYPTO;
      return true;
    }
  }
  CryptoParams selected_params;
  if (!NegotiateParams(answer_params, &selected_params))
    return false;
  const CryptoParams& send_params =
      (source == CS_REMOTE) ? selected_params : answer_params[0];
  const CryptoParams& recv_params =
      (source == CS_REMOTE) ? answer_params[0] : selected_params;
  if (!ApplyParams(send_params, recv_params)) {
    return false;
  }

  if (final) {
    offer_params_.clear();
    state_ = ST_ACTIVE;
  } else {
    state_ =
        (source == CS_LOCAL) ? ST_SENTPRANSWER : ST_RECEIVEDPRANSWER;
  }
  return true;
}

void SrtpFilter::CreateSrtpSessions() {
  send_session_.reset(new SrtpSession());
  applied_send_params_ = CryptoParams();
  recv_session_.reset(new SrtpSession());
  applied_recv_params_ = CryptoParams();

  SignalSrtpError.repeat(send_session_->SignalSrtpError);
  SignalSrtpError.repeat(recv_session_->SignalSrtpError);

  send_session_->set_signal_silent_time(signal_silent_time_in_ms_);
  recv_session_->set_signal_silent_time(signal_silent_time_in_ms_);
}

bool SrtpFilter::NegotiateParams(const std::vector<CryptoParams>& answer_params,
                                 CryptoParams* selected_params) {
  // We're processing an accept. We should have exactly one set of params,
  // unless the offer didn't mention crypto, in which case we shouldn't be here.
  bool ret = (answer_params.size() == 1U && !offer_params_.empty());
  if (ret) {
    // We should find a match between the answer params and the offered params.
    std::vector<CryptoParams>::const_iterator it;
    for (it = offer_params_.begin(); it != offer_params_.end(); ++it) {
      if (answer_params[0].Matches(*it)) {
        break;
      }
    }

    if (it != offer_params_.end()) {
      *selected_params = *it;
    } else {
      ret = false;
    }
  }

  if (!ret) {
    LOG(LS_WARNING) << "Invalid parameters in SRTP answer";
  }
  return ret;
}

bool SrtpFilter::ApplyParams(const CryptoParams& send_params,
                             const CryptoParams& recv_params) {
  // TODO(jiayl): Split this method to apply send and receive CryptoParams
  // independently, so that we can skip one method when either send or receive
  // CryptoParams is unchanged.
  if (applied_send_params_.cipher_suite == send_params.cipher_suite &&
      applied_send_params_.key_params == send_params.key_params &&
      applied_recv_params_.cipher_suite == recv_params.cipher_suite &&
      applied_recv_params_.key_params == recv_params.key_params) {
    LOG(LS_INFO) << "Applying the same SRTP parameters again. No-op.";

    // We do not want to reset the ROC if the keys are the same. So just return.
    return true;
  }

  int send_suite = rtc::SrtpCryptoSuiteFromName(send_params.cipher_suite);
  int recv_suite = rtc::SrtpCryptoSuiteFromName(recv_params.cipher_suite);
  if (send_suite == rtc::SRTP_INVALID_CRYPTO_SUITE ||
      recv_suite == rtc::SRTP_INVALID_CRYPTO_SUITE) {
    LOG(LS_WARNING) << "Unknown crypto suite(s) received:"
                    << " send cipher_suite " << send_params.cipher_suite
                    << " recv cipher_suite " << recv_params.cipher_suite;
    return false;
  }

  int send_key_len, send_salt_len;
  int recv_key_len, recv_salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(send_suite, &send_key_len,
                                     &send_salt_len) ||
      !rtc::GetSrtpKeyAndSaltLengths(recv_suite, &recv_key_len,
                                     &recv_salt_len)) {
    LOG(LS_WARNING) << "Could not get lengths for crypto suite(s):"
                    << " send cipher_suite " << send_params.cipher_suite
                    << " recv cipher_suite " << recv_params.cipher_suite;
    return false;
  }

  // TODO(juberti): Zero these buffers after use.
  bool ret;
  rtc::Buffer send_key(send_key_len + send_salt_len);
  rtc::Buffer recv_key(recv_key_len + recv_salt_len);
  ret = (ParseKeyParams(send_params.key_params, send_key.data(),
                        send_key.size()) &&
         ParseKeyParams(recv_params.key_params, recv_key.data(),
                        recv_key.size()));
  if (ret) {
    CreateSrtpSessions();
    ret = (send_session_->SetSend(
               rtc::SrtpCryptoSuiteFromName(send_params.cipher_suite),
               send_key.data(), send_key.size()) &&
           recv_session_->SetRecv(
               rtc::SrtpCryptoSuiteFromName(recv_params.cipher_suite),
               recv_key.data(), recv_key.size()));
  }
  if (ret) {
    LOG(LS_INFO) << "SRTP activated with negotiated parameters:"
                 << " send cipher_suite " << send_params.cipher_suite
                 << " recv cipher_suite " << recv_params.cipher_suite;
    applied_send_params_ = send_params;
    applied_recv_params_ = recv_params;
  } else {
    LOG(LS_WARNING) << "Failed to apply negotiated SRTP parameters";
  }
  return ret;
}

bool SrtpFilter::ResetParams() {
  offer_params_.clear();
  state_ = ST_INIT;
  send_session_ = nullptr;
  recv_session_ = nullptr;
  send_rtcp_session_ = nullptr;
  recv_rtcp_session_ = nullptr;
  LOG(LS_INFO) << "SRTP reset to init state";
  return true;
}

bool SrtpFilter::ParseKeyParams(const std::string& key_params,
                                uint8_t* key,
                                size_t len) {
  // example key_params: "inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2"

  // Fail if key-method is wrong.
  if (key_params.find("inline:") != 0) {
    return false;
  }

  // Fail if base64 decode fails, or the key is the wrong size.
  std::string key_b64(key_params.substr(7)), key_str;
  if (!rtc::Base64::Decode(key_b64, rtc::Base64::DO_STRICT,
                           &key_str, nullptr) || key_str.size() != len) {
    return false;
  }

  memcpy(key, key_str.c_str(), len);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// SrtpSession

#ifdef HAVE_SRTP

bool SrtpSession::inited_ = false;

// This lock protects SrtpSession::inited_.
rtc::GlobalLockPod SrtpSession::lock_;

SrtpSession::SrtpSession()
    : session_(nullptr),
      rtp_auth_tag_len_(0),
      rtcp_auth_tag_len_(0),
      srtp_stat_(new SrtpStat()),
      last_send_seq_num_(-1) {
  SignalSrtpError.repeat(srtp_stat_->SignalSrtpError);
}

SrtpSession::~SrtpSession() {
  if (session_) {
    srtp_set_user_data(session_, nullptr);
    srtp_dealloc(session_);
  }
}

bool SrtpSession::SetSend(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_outbound, cs, key, len);
}

bool SrtpSession::SetRecv(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_inbound, cs, key, len);
}

bool SrtpSession::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + rtp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect(session_, p, out_len);
  uint32_t ssrc;
  if (GetRtpSsrc(p, in_len, &ssrc)) {
    srtp_stat_->AddProtectRtpResult(ssrc, err);
  }
  int seq_num;
  GetRtpSeqNum(p, in_len, &seq_num);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet, seqnum="
                    << seq_num << ", err=" << err << ", last seqnum="
                    << last_send_seq_num_;
    return false;
  }
  last_send_seq_num_ = seq_num;
  return true;
}

bool SrtpSession::ProtectRtp(void* p,
                             int in_len,
                             int max_len,
                             int* out_len,
                             int64_t* index) {
  if (!ProtectRtp(p, in_len, max_len, out_len)) {
    return false;
  }
  return (index) ? GetSendStreamPacketIndex(p, in_len, index) : true;
}

bool SrtpSession::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + sizeof(uint32_t) + rtcp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect_rtcp(session_, p, out_len);
  srtp_stat_->AddProtectRtcpResult(err);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtp(void* p, int in_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect(session_, p, out_len);
  uint32_t ssrc;
  if (GetRtpSsrc(p, in_len, &ssrc)) {
    srtp_stat_->AddUnprotectRtpResult(ssrc, err);
  }
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtcp(void* p, int in_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect_rtcp(session_, p, out_len);
  srtp_stat_->AddUnprotectRtcpResult(err);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len) {
#if defined(ENABLE_EXTERNAL_AUTH)
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ExternalHmacContext* external_hmac = nullptr;
  // stream_template will be the reference context for other streams.
  // Let's use it for getting the keys.
  srtp_stream_ctx_t* srtp_context = session_->stream_template;
  if (srtp_context && srtp_context->rtp_auth) {
    external_hmac = reinterpret_cast<ExternalHmacContext*>(
        srtp_context->rtp_auth->state);
  }

  if (!external_hmac) {
    LOG(LS_ERROR) << "Failed to get auth keys from libsrtp!.";
    return false;
  }

  *key = external_hmac->key;
  *key_len = external_hmac->key_length;
  *tag_len = rtp_auth_tag_len_;
  return true;
#else
  return false;
#endif
}

int SrtpSession::GetSrtpOverhead() const {
  return rtp_auth_tag_len_;
}

bool SrtpSession::GetSendStreamPacketIndex(void* p,
                                           int in_len,
                                           int64_t* index) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  srtp_hdr_t* hdr = reinterpret_cast<srtp_hdr_t*>(p);
  srtp_stream_ctx_t* stream = srtp_get_stream(session_, hdr->ssrc);
  if (!stream) {
    return false;
  }

  // Shift packet index, put into network byte order
  *index = static_cast<int64_t>(
      rtc::NetworkToHost64(
          srtp_rdbx_get_packet_index(&stream->rtp_rdbx) << 16));
  return true;
}

void SrtpSession::set_signal_silent_time(int signal_silent_time_in_ms) {
  srtp_stat_->set_signal_silent_time(signal_silent_time_in_ms);
}

bool SrtpSession::SetKey(int type, int cs, const uint8_t* key, size_t len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (session_) {
    LOG(LS_ERROR) << "Failed to create SRTP session: "
                  << "SRTP session already created";
    return false;
  }

  if (!Init()) {
    return false;
  }

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));
  if (cs == rtc::SRTP_AES128_CM_SHA1_80) {
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == rtc::SRTP_AES128_CM_SHA1_32) {
    // RTP HMAC is shortened to 32 bits, but RTCP remains 80 bits.
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
#if !defined(ENABLE_EXTERNAL_AUTH)
    // TODO(jbauch): Re-enable once https://crbug.com/628400 is resolved.
  } else if (cs == rtc::SRTP_AEAD_AES_128_GCM) {
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
  } else if (cs == rtc::SRTP_AEAD_AES_256_GCM) {
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
#endif  // ENABLE_EXTERNAL_AUTH
  } else {
    LOG(LS_WARNING) << "Failed to create SRTP session: unsupported"
                    << " cipher_suite " << cs;
    return false;
  }

  int expected_key_len;
  int expected_salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(cs, &expected_key_len,
      &expected_salt_len)) {
    // This should never happen.
    LOG(LS_WARNING) << "Failed to create SRTP session: unsupported"
                    << " cipher_suite without length information" << cs;
    return false;
  }

  if (!key ||
      len != static_cast<size_t>(expected_key_len + expected_salt_len)) {
    LOG(LS_WARNING) << "Failed to create SRTP session: invalid key";
    return false;
  }

  policy.ssrc.type = static_cast<srtp_ssrc_type_t>(type);
  policy.ssrc.value = 0;
  policy.key = const_cast<uint8_t*>(key);
  // TODO(astor) parse window size from WSH session-param
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;
  // If external authentication option is enabled, supply custom auth module
  // id EXTERNAL_HMAC_SHA1 in the policy structure.
  // We want to set this option only for rtp packets.
  // By default policy structure is initialized to HMAC_SHA1.
#if defined(ENABLE_EXTERNAL_AUTH)
  // Enable external HMAC authentication only for outgoing streams.
  if (type == ssrc_any_outbound) {
    policy.rtp.auth_type = EXTERNAL_HMAC_SHA1;
  }
#endif
  policy.next = nullptr;

  int err = srtp_create(&session_, &policy);
  if (err != srtp_err_status_ok) {
    session_ = nullptr;
    LOG(LS_ERROR) << "Failed to create SRTP session, err=" << err;
    return false;
  }

  srtp_set_user_data(session_, this);
  rtp_auth_tag_len_ = policy.rtp.auth_tag_len;
  rtcp_auth_tag_len_ = policy.rtcp.auth_tag_len;
  return true;
}

bool SrtpSession::Init() {
  rtc::GlobalLockScope ls(&lock_);

  if (!inited_) {
    int err;
    err = srtp_init();
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to init SRTP, err=" << err;
      return false;
    }

    err = srtp_install_event_handler(&SrtpSession::HandleEventThunk);
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to install SRTP event handler, err=" << err;
      return false;
    }
#if defined(ENABLE_EXTERNAL_AUTH)
    err = external_crypto_init();
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to initialize fake auth, err=" << err;
      return false;
    }
#endif
    inited_ = true;
  }

  return true;
}

void SrtpSession::Terminate() {
  rtc::GlobalLockScope ls(&lock_);

  if (inited_) {
    int err = srtp_shutdown();
    if (err) {
      LOG(LS_ERROR) << "srtp_shutdown failed. err=" << err;
      return;
    }
    inited_ = false;
  }
}

void SrtpSession::HandleEvent(const srtp_event_data_t* ev) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  switch (ev->event) {
    case event_ssrc_collision:
      LOG(LS_INFO) << "SRTP event: SSRC collision";
      break;
    case event_key_soft_limit:
      LOG(LS_INFO) << "SRTP event: reached soft key usage limit";
      break;
    case event_key_hard_limit:
      LOG(LS_INFO) << "SRTP event: reached hard key usage limit";
      break;
    case event_packet_index_limit:
      LOG(LS_INFO) << "SRTP event: reached hard packet limit (2^48 packets)";
      break;
    default:
      LOG(LS_INFO) << "SRTP event: unknown " << ev->event;
      break;
  }
}

void SrtpSession::HandleEventThunk(srtp_event_data_t* ev) {
  // Callback will be executed from same thread that calls the "srtp_protect"
  // and "srtp_unprotect" functions.
  SrtpSession* session = static_cast<SrtpSession*>(
      srtp_get_user_data(ev->session));
  if (session) {
    session->HandleEvent(ev);
  }
}

#else   // !HAVE_SRTP

// On some systems, SRTP is not (yet) available.

SrtpSession::SrtpSession() {
  LOG(WARNING) << "SRTP implementation is missing.";
}

SrtpSession::~SrtpSession() {
}

bool SrtpSession::SetSend(const std::string& cs, const uint8_t* key, int len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::SetRecv(const std::string& cs, const uint8_t* key, int len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::ProtectRtp(void* data, int in_len, int max_len,
                             int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::ProtectRtcp(void* data, int in_len, int max_len,
                              int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::UnprotectRtp(void* data, int in_len, int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::UnprotectRtcp(void* data, int in_len, int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

void SrtpSession::set_signal_silent_time(uint32_t signal_silent_time) {
  // Do nothing.
}

#endif  // HAVE_SRTP

///////////////////////////////////////////////////////////////////////////////
// SrtpStat

#ifdef HAVE_SRTP

SrtpStat::SrtpStat()
    : signal_silent_time_(1000) {
}

void SrtpStat::AddProtectRtpResult(uint32_t ssrc, int result) {
  FailureKey key;
  key.ssrc = ssrc;
  key.mode = SrtpFilter::PROTECT;
  switch (result) {
    case srtp_err_status_ok:
      key.error = SrtpFilter::ERROR_NONE;
      break;
    case srtp_err_status_auth_fail:
      key.error = SrtpFilter::ERROR_AUTH;
      break;
    default:
      key.error = SrtpFilter::ERROR_FAIL;
  }
  HandleSrtpResult(key);
}

void SrtpStat::AddUnprotectRtpResult(uint32_t ssrc, int result) {
  FailureKey key;
  key.ssrc = ssrc;
  key.mode = SrtpFilter::UNPROTECT;
  switch (result) {
    case srtp_err_status_ok:
      key.error = SrtpFilter::ERROR_NONE;
      break;
    case srtp_err_status_auth_fail:
      key.error = SrtpFilter::ERROR_AUTH;
      break;
    case srtp_err_status_replay_fail:
    case srtp_err_status_replay_old:
      key.error = SrtpFilter::ERROR_REPLAY;
      break;
    default:
      key.error = SrtpFilter::ERROR_FAIL;
  }
  HandleSrtpResult(key);
}

void SrtpStat::AddProtectRtcpResult(int result) {
  AddProtectRtpResult(0U, result);
}

void SrtpStat::AddUnprotectRtcpResult(int result) {
  AddUnprotectRtpResult(0U, result);
}

void SrtpStat::HandleSrtpResult(const SrtpStat::FailureKey& key) {
  // Handle some cases where error should be signalled right away. For other
  // errors, trigger error for the first time seeing it.  After that, silent
  // the same error for a certain amount of time (default 1 sec).
  if (key.error != SrtpFilter::ERROR_NONE) {
    // For errors, signal first time and wait for 1 sec.
    FailureStat* stat = &(failures_[key]);
    int64_t current_time = rtc::TimeMillis();
    if (stat->last_signal_time == 0 ||
        rtc::TimeDiff(current_time, stat->last_signal_time) >
            signal_silent_time_) {
      SignalSrtpError(key.ssrc, key.mode, key.error);
      stat->last_signal_time = current_time;
    }
  }
}

#else   // !HAVE_SRTP

// On some systems, SRTP is not (yet) available.

SrtpStat::SrtpStat()
    : signal_silent_time_(1000) {
  LOG(WARNING) << "SRTP implementation is missing.";
}

void SrtpStat::AddProtectRtpResult(uint32_t ssrc, int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddUnprotectRtpResult(uint32_t ssrc, int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddProtectRtcpResult(int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddUnprotectRtcpResult(int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::HandleSrtpResult(const SrtpStat::FailureKey& key) {
  SrtpNotAvailable(__FUNCTION__);
}

#endif  // HAVE_SRTP

}  // namespace cricket
