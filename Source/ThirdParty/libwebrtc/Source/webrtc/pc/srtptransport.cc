/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtptransport.h"

#include <string>
#include <vector>

#include "media/base/rtputils.h"
#include "pc/rtptransport.h"
#include "pc/srtpsession.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/base64.h"
#include "rtc_base/copyonwritebuffer.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

SrtpTransport::SrtpTransport(bool rtcp_mux_enabled)
    : RtpTransportInternalAdapter(new RtpTransport(rtcp_mux_enabled)) {
  // Own the raw pointer |transport| from the base class.
  rtp_transport_.reset(transport_);
  RTC_DCHECK(rtp_transport_);
  ConnectToRtpTransport();
}

SrtpTransport::SrtpTransport(
    std::unique_ptr<RtpTransportInternal> rtp_transport)
    : RtpTransportInternalAdapter(rtp_transport.get()),
      rtp_transport_(std::move(rtp_transport)) {
  RTC_DCHECK(rtp_transport_);
  ConnectToRtpTransport();
}

void SrtpTransport::ConnectToRtpTransport() {
  rtp_transport_->SignalPacketReceived.connect(
      this, &SrtpTransport::OnPacketReceived);
  rtp_transport_->SignalReadyToSend.connect(this,
                                            &SrtpTransport::OnReadyToSend);
  rtp_transport_->SignalNetworkRouteChanged.connect(
      this, &SrtpTransport::OnNetworkRouteChanged);
  rtp_transport_->SignalWritableState.connect(this,
                                              &SrtpTransport::OnWritableState);
  rtp_transport_->SignalSentPacket.connect(this, &SrtpTransport::OnSentPacket);
}

bool SrtpTransport::SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                                  const rtc::PacketOptions& options,
                                  int flags) {
  return SendPacket(false, packet, options, flags);
}

bool SrtpTransport::SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                                   const rtc::PacketOptions& options,
                                   int flags) {
  return SendPacket(true, packet, options, flags);
}

bool SrtpTransport::SendPacket(bool rtcp,
                               rtc::CopyOnWriteBuffer* packet,
                               const rtc::PacketOptions& options,
                               int flags) {
  if (!IsActive()) {
    RTC_LOG(LS_ERROR)
        << "Failed to send the packet because SRTP transport is inactive.";
    return false;
  }

  rtc::PacketOptions updated_options = options;
  rtc::CopyOnWriteBuffer cp = *packet;
  TRACE_EVENT0("webrtc", "SRTP Encode");
  bool res;
  uint8_t* data = packet->data();
  int len = static_cast<int>(packet->size());
  if (!rtcp) {
// If ENABLE_EXTERNAL_AUTH flag is on then packet authentication is not done
// inside libsrtp for a RTP packet. A external HMAC module will be writing
// a fake HMAC value. This is ONLY done for a RTP packet.
// Socket layer will update rtp sendtime extension header if present in
// packet with current time before updating the HMAC.
#if !defined(ENABLE_EXTERNAL_AUTH)
    res = ProtectRtp(data, len, static_cast<int>(packet->capacity()), &len);
#else
    if (!IsExternalAuthActive()) {
      res = ProtectRtp(data, len, static_cast<int>(packet->capacity()), &len);
    } else {
      updated_options.packet_time_params.rtp_sendtime_extension_id =
          rtp_abs_sendtime_extn_id_;
      res = ProtectRtp(data, len, static_cast<int>(packet->capacity()), &len,
                       &updated_options.packet_time_params.srtp_packet_index);
      // If protection succeeds, let's get auth params from srtp.
      if (res) {
        uint8_t* auth_key = NULL;
        int key_len;
        res = GetRtpAuthParams(
            &auth_key, &key_len,
            &updated_options.packet_time_params.srtp_auth_tag_len);
        if (res) {
          updated_options.packet_time_params.srtp_auth_key.resize(key_len);
          updated_options.packet_time_params.srtp_auth_key.assign(
              auth_key, auth_key + key_len);
        }
      }
    }
#endif
    if (!res) {
      int seq_num = -1;
      uint32_t ssrc = 0;
      cricket::GetRtpSeqNum(data, len, &seq_num);
      cricket::GetRtpSsrc(data, len, &ssrc);
      RTC_LOG(LS_ERROR) << "Failed to protect RTP packet: size=" << len
                        << ", seqnum=" << seq_num << ", SSRC=" << ssrc;
      return false;
    }
  } else {
    res = ProtectRtcp(data, len, static_cast<int>(packet->capacity()), &len);
    if (!res) {
      int type = -1;
      cricket::GetRtcpType(data, len, &type);
      RTC_LOG(LS_ERROR) << "Failed to protect RTCP packet: size=" << len
                        << ", type=" << type;
      return false;
    }
  }

  // Update the length of the packet now that we've added the auth tag.
  packet->SetSize(len);
  return rtcp ? rtp_transport_->SendRtcpPacket(packet, updated_options, flags)
              : rtp_transport_->SendRtpPacket(packet, updated_options, flags);
}

void SrtpTransport::OnPacketReceived(bool rtcp,
                                     rtc::CopyOnWriteBuffer* packet,
                                     const rtc::PacketTime& packet_time) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING)
        << "Inactive SRTP transport received a packet. Drop it.";
    return;
  }

  TRACE_EVENT0("webrtc", "SRTP Decode");
  char* data = packet->data<char>();
  int len = static_cast<int>(packet->size());
  bool res;
  if (!rtcp) {
    res = UnprotectRtp(data, len, &len);
    if (!res) {
      int seq_num = -1;
      uint32_t ssrc = 0;
      cricket::GetRtpSeqNum(data, len, &seq_num);
      cricket::GetRtpSsrc(data, len, &ssrc);
      RTC_LOG(LS_ERROR) << "Failed to unprotect RTP packet: size=" << len
                        << ", seqnum=" << seq_num << ", SSRC=" << ssrc;
      return;
    }
  } else {
    res = UnprotectRtcp(data, len, &len);
    if (!res) {
      int type = -1;
      cricket::GetRtcpType(data, len, &type);
      RTC_LOG(LS_ERROR) << "Failed to unprotect RTCP packet: size=" << len
                        << ", type=" << type;
      return;
    }
  }

  packet->SetSize(len);
  SignalPacketReceived(rtcp, packet, packet_time);
}

void SrtpTransport::OnNetworkRouteChanged(

    rtc::Optional<rtc::NetworkRoute> network_route) {
  // Only append the SRTP overhead when there is a selected network route.
  if (network_route) {
    int srtp_overhead = 0;
    if (IsActive()) {
      GetSrtpOverhead(&srtp_overhead);
    }
    network_route->packet_overhead += srtp_overhead;
  }
  SignalNetworkRouteChanged(network_route);
}

bool SrtpTransport::SetRtpParams(int send_cs,
                                 const uint8_t* send_key,
                                 int send_key_len,
                                 const std::vector<int>& send_extension_ids,
                                 int recv_cs,
                                 const uint8_t* recv_key,
                                 int recv_key_len,
                                 const std::vector<int>& recv_extension_ids) {
  // If parameters are being set for the first time, we should create new SRTP
  // sessions and call "SetSend/SetRecv". Otherwise we should call
  // "UpdateSend"/"UpdateRecv" on the existing sessions, which will internally
  // call "srtp_update".
  bool new_sessions = false;
  if (!send_session_) {
    RTC_DCHECK(!recv_session_);
    CreateSrtpSessions();
    new_sessions = true;
  }
  bool ret = new_sessions
                 ? send_session_->SetSend(send_cs, send_key, send_key_len,
                                          send_extension_ids)
                 : send_session_->UpdateSend(send_cs, send_key, send_key_len,
                                             send_extension_ids);
  if (!ret) {
    ResetParams();
    return false;
  }

  ret = new_sessions ? recv_session_->SetRecv(recv_cs, recv_key, recv_key_len,
                                              recv_extension_ids)
                     : recv_session_->UpdateRecv(
                           recv_cs, recv_key, recv_key_len, recv_extension_ids);
  if (!ret) {
    ResetParams();
    return false;
  }

  RTC_LOG(LS_INFO) << "SRTP " << (new_sessions ? "activated" : "updated")
                   << " with negotiated parameters:"
                   << " send cipher_suite " << send_cs << " recv cipher_suite "
                   << recv_cs;
  return true;
}

bool SrtpTransport::SetRtcpParams(int send_cs,
                                  const uint8_t* send_key,
                                  int send_key_len,
                                  const std::vector<int>& send_extension_ids,
                                  int recv_cs,
                                  const uint8_t* recv_key,
                                  int recv_key_len,
                                  const std::vector<int>& recv_extension_ids) {
  // This can only be called once, but can be safely called after
  // SetRtpParams
  if (send_rtcp_session_ || recv_rtcp_session_) {
    RTC_LOG(LS_ERROR) << "Tried to set SRTCP Params when filter already active";
    return false;
  }

  send_rtcp_session_.reset(new cricket::SrtpSession());
  if (!send_rtcp_session_->SetSend(send_cs, send_key, send_key_len,
                                   send_extension_ids)) {
    return false;
  }

  recv_rtcp_session_.reset(new cricket::SrtpSession());
  if (!recv_rtcp_session_->SetRecv(recv_cs, recv_key, recv_key_len,
                                   recv_extension_ids)) {
    return false;
  }

  RTC_LOG(LS_INFO) << "SRTCP activated with negotiated parameters:"
                   << " send cipher_suite " << send_cs << " recv cipher_suite "
                   << recv_cs;

  return true;
}

bool SrtpTransport::IsActive() const {
  return send_session_ && recv_session_;
}

void SrtpTransport::ResetParams() {
  send_session_ = nullptr;
  recv_session_ = nullptr;
  send_rtcp_session_ = nullptr;
  recv_rtcp_session_ = nullptr;
  RTC_LOG(LS_INFO) << "The params in SRTP transport are reset.";
}

void SrtpTransport::CreateSrtpSessions() {
  send_session_.reset(new cricket::SrtpSession());
  recv_session_.reset(new cricket::SrtpSession());

  if (external_auth_enabled_) {
    send_session_->EnableExternalAuth();
  }
}

bool SrtpTransport::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len);
}

bool SrtpTransport::ProtectRtp(void* p,
                               int in_len,
                               int max_len,
                               int* out_len,
                               int64_t* index) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len, index);
}

bool SrtpTransport::ProtectRtcp(void* p,
                                int in_len,
                                int max_len,
                                int* out_len) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to ProtectRtcp: SRTP not active";
    return false;
  }
  if (send_rtcp_session_) {
    return send_rtcp_session_->ProtectRtcp(p, in_len, max_len, out_len);
  } else {
    RTC_CHECK(send_session_);
    return send_session_->ProtectRtcp(p, in_len, max_len, out_len);
  }
}

bool SrtpTransport::UnprotectRtp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to UnprotectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(recv_session_);
  return recv_session_->UnprotectRtp(p, in_len, out_len);
}

bool SrtpTransport::UnprotectRtcp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to UnprotectRtcp: SRTP not active";
    return false;
  }
  if (recv_rtcp_session_) {
    return recv_rtcp_session_->UnprotectRtcp(p, in_len, out_len);
  } else {
    RTC_CHECK(recv_session_);
    return recv_session_->UnprotectRtcp(p, in_len, out_len);
  }
}

bool SrtpTransport::GetRtpAuthParams(uint8_t** key,
                                     int* key_len,
                                     int* tag_len) {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to GetRtpAuthParams: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  return send_session_->GetRtpAuthParams(key, key_len, tag_len);
}

bool SrtpTransport::GetSrtpOverhead(int* srtp_overhead) const {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING) << "Failed to GetSrtpOverhead: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  *srtp_overhead = send_session_->GetSrtpOverhead();
  return true;
}

void SrtpTransport::EnableExternalAuth() {
  RTC_DCHECK(!IsActive());
  external_auth_enabled_ = true;
}

bool SrtpTransport::IsExternalAuthEnabled() const {
  return external_auth_enabled_;
}

bool SrtpTransport::IsExternalAuthActive() const {
  if (!IsActive()) {
    RTC_LOG(LS_WARNING)
        << "Failed to check IsExternalAuthActive: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  return send_session_->IsExternalAuthActive();
}

}  // namespace webrtc
