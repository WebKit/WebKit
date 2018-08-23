/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_CONFIG_H_
#define CALL_RTP_CONFIG_H_

#include <string>
#include <vector>

#include "api/rtp_headers.h"
#include "api/rtpparameters.h"

namespace webrtc {
// Currently only VP8/VP9 specific.
struct RtpPayloadState {
  int16_t picture_id = -1;
  uint8_t tl0_pic_idx = 0;
  int64_t shared_frame_id = 0;
};
// Settings for NACK, see RFC 4585 for details.
struct NackConfig {
  NackConfig() : rtp_history_ms(0) {}
  std::string ToString() const;
  // Send side: the time RTP packets are stored for retransmissions.
  // Receive side: the time the receiver is prepared to wait for
  // retransmissions.
  // Set to '0' to disable.
  int rtp_history_ms;
};

// Settings for ULPFEC forward error correction.
// Set the payload types to '-1' to disable.
struct UlpfecConfig {
  UlpfecConfig()
      : ulpfec_payload_type(-1),
        red_payload_type(-1),
        red_rtx_payload_type(-1) {}
  std::string ToString() const;
  bool operator==(const UlpfecConfig& other) const;

  // Payload type used for ULPFEC packets.
  int ulpfec_payload_type;

  // Payload type used for RED packets.
  int red_payload_type;

  // RTX payload type for RED payload.
  int red_rtx_payload_type;
};

static const size_t kDefaultMaxPacketSize = 1500 - 40;  // TCP over IPv4.
struct RtpConfig {
  RtpConfig();
  RtpConfig(const RtpConfig&);
  ~RtpConfig();
  std::string ToString() const;

  std::vector<uint32_t> ssrcs;

  // The value to send in the MID RTP header extension if the extension is
  // included in the list of extensions.
  std::string mid;

  // See RtcpMode for description.
  RtcpMode rtcp_mode = RtcpMode::kCompound;

  // Max RTP packet size delivered to send transport from VideoEngine.
  size_t max_packet_size = kDefaultMaxPacketSize;

  // RTP header extensions to use for this send stream.
  std::vector<RtpExtension> extensions;

  // TODO(nisse): For now, these are fixed, but we'd like to support
  // changing codec without recreating the VideoSendStream. Then these
  // fields must be removed, and association between payload type and codec
  // must move above the per-stream level. Ownership could be with
  // RtpTransportControllerSend, with a reference from PayloadRouter, where
  // the latter would be responsible for mapping the codec type of encoded
  // images to the right payload type.
  std::string payload_name;
  int payload_type = -1;

  // See NackConfig for description.
  NackConfig nack;

  // See UlpfecConfig for description.
  UlpfecConfig ulpfec;

  struct Flexfec {
    Flexfec();
    Flexfec(const Flexfec&);
    ~Flexfec();
    // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
    int payload_type = -1;

    // SSRC of FlexFEC stream.
    uint32_t ssrc = 0;

    // Vector containing a single element, corresponding to the SSRC of the
    // media stream being protected by this FlexFEC stream.
    // The vector MUST have size 1.
    //
    // TODO(brandtr): Update comment above when we support
    // multistream protection.
    std::vector<uint32_t> protected_media_ssrcs;
  } flexfec;

  // Settings for RTP retransmission payload format, see RFC 4588 for
  // details.
  struct Rtx {
    Rtx();
    Rtx(const Rtx&);
    ~Rtx();
    std::string ToString() const;
    // SSRCs to use for the RTX streams.
    std::vector<uint32_t> ssrcs;

    // Payload type to use for the RTX stream.
    int payload_type = -1;
  } rtx;

  // RTCP CNAME, see RFC 3550.
  std::string c_name;
};

struct RtcpConfig {
  RtcpConfig();
  RtcpConfig(const RtcpConfig&);
  ~RtcpConfig();
  std::string ToString() const;

  // Time interval between RTCP report for video
  int64_t video_report_interval_ms = 1000;
  // Time interval between RTCP report for audio
  int64_t audio_report_interval_ms = 5000;
};
}  // namespace webrtc
#endif  // CALL_RTP_CONFIG_H_
