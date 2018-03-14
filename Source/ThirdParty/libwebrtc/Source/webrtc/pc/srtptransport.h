/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SRTPTRANSPORT_H_
#define PC_SRTPTRANSPORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "p2p/base/icetransportinternal.h"
#include "pc/rtptransportinternaladapter.h"
#include "pc/srtpfilter.h"
#include "pc/srtpsession.h"
#include "rtc_base/checks.h"

namespace webrtc {

// This class will eventually be a wrapper around RtpTransportInternal
// that protects and unprotects sent and received RTP packets.
class SrtpTransport : public RtpTransportInternalAdapter {
 public:
  explicit SrtpTransport(bool rtcp_mux_enabled);

  explicit SrtpTransport(std::unique_ptr<RtpTransportInternal> rtp_transport);

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  // The transport becomes active if the send_session_ and recv_session_ are
  // created.
  bool IsActive() const;

  // TODO(zstein): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override { return nullptr; }

  // Create new send/recv sessions and set the negotiated crypto keys for RTP
  // packet encryption. The keys can either come from SDES negotiation or DTLS
  // handshake.
  bool SetRtpParams(int send_cs,
                    const uint8_t* send_key,
                    int send_key_len,
                    const std::vector<int>& send_extension_ids,
                    int recv_cs,
                    const uint8_t* recv_key,
                    int recv_key_len,
                    const std::vector<int>& recv_extension_ids);

  // Create new send/recv sessions and set the negotiated crypto keys for RTCP
  // packet encryption. The keys can either come from SDES negotiation or DTLS
  // handshake.
  bool SetRtcpParams(int send_cs,
                     const uint8_t* send_key,
                     int send_key_len,
                     const std::vector<int>& send_extension_ids,
                     int recv_cs,
                     const uint8_t* recv_key,
                     int recv_key_len,
                     const std::vector<int>& recv_extension_ids);

  void ResetParams();

  // If external auth is enabled, SRTP will write a dummy auth tag that then
  // later must get replaced before the packet is sent out. Only supported for
  // non-GCM cipher suites and can be checked through "IsExternalAuthActive"
  // if it is actually used. This method is only valid before the RTP params
  // have been set.
  void EnableExternalAuth();
  bool IsExternalAuthEnabled() const;

  // A SrtpTransport supports external creation of the auth tag if a non-GCM
  // cipher is used. This method is only valid after the RTP params have
  // been set.
  bool IsExternalAuthActive() const;

  // Returns srtp overhead for rtp packets.
  bool GetSrtpOverhead(int* srtp_overhead) const;

  // Returns rtp auth params from srtp context.
  bool GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len);

  // Cache RTP Absoulute SendTime extension header ID. This is only used when
  // external authentication is enabled.
  void CacheRtpAbsSendTimeHeaderExtension(int rtp_abs_sendtime_extn_id) {
    rtp_abs_sendtime_extn_id_ = rtp_abs_sendtime_extn_id;
  }

 private:
  void ConnectToRtpTransport();
  void CreateSrtpSessions();

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags);

  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer* packet,
                        const rtc::PacketTime& packet_time);
  void OnReadyToSend(bool ready) { SignalReadyToSend(ready); }
  void OnNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute> network_route);

  void OnWritableState(bool writable) { SignalWritableState(writable); }

  void OnSentPacket(const rtc::SentPacket& sent_packet) {
    SignalSentPacket(sent_packet);
  }

  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);

  // Overloaded version, outputs packet index.
  bool ProtectRtp(void* data,
                  int in_len,
                  int max_len,
                  int* out_len,
                  int64_t* index);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);

  // Decrypts/verifies an invidiual RTP/RTCP packet.
  // If an HMAC is used, this will decrease the packet size.
  bool UnprotectRtp(void* data, int in_len, int* out_len);

  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  const std::string content_name_;
  std::unique_ptr<RtpTransportInternal> rtp_transport_;

  std::unique_ptr<cricket::SrtpSession> send_session_;
  std::unique_ptr<cricket::SrtpSession> recv_session_;
  std::unique_ptr<cricket::SrtpSession> send_rtcp_session_;
  std::unique_ptr<cricket::SrtpSession> recv_rtcp_session_;

  bool external_auth_enabled_ = false;

  int rtp_abs_sendtime_extn_id_ = -1;
};

}  // namespace webrtc

#endif  // PC_SRTPTRANSPORT_H_
