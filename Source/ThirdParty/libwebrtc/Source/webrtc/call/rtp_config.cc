/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_config.h"

#include <cstdint>

#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

std::string NackConfig::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{rtp_history_ms: " << rtp_history_ms;
  ss << '}';
  return ss.str();
}

std::string UlpfecConfig::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << ", red_rtx_payload_type: " << red_rtx_payload_type;
  ss << '}';
  return ss.str();
}

bool UlpfecConfig::operator==(const UlpfecConfig& other) const {
  return ulpfec_payload_type == other.ulpfec_payload_type &&
         red_payload_type == other.red_payload_type &&
         red_rtx_payload_type == other.red_rtx_payload_type;
}

RtpConfig::RtpConfig() = default;
RtpConfig::RtpConfig(const RtpConfig&) = default;
RtpConfig::~RtpConfig() = default;

RtpConfig::Flexfec::Flexfec() = default;
RtpConfig::Flexfec::Flexfec(const Flexfec&) = default;
RtpConfig::Flexfec::~Flexfec() = default;

std::string RtpConfig::ToString() const {
  char buf[2 * 1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extmap-allow-mixed: " << (extmap_allow_mixed ? "true" : "false");
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", ulpfec: " << ulpfec.ToString();
  ss << ", payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;

  ss << ", flexfec: {payload_type: " << flexfec.payload_type;
  ss << ", ssrc: " << flexfec.ssrc;
  ss << ", protected_media_ssrcs: [";
  for (size_t i = 0; i < flexfec.protected_media_ssrcs.size(); ++i) {
    ss << flexfec.protected_media_ssrcs[i];
    if (i != flexfec.protected_media_ssrcs.size() - 1)
      ss << ", ";
  }
  ss << "]}";

  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

RtpConfig::Rtx::Rtx() = default;
RtpConfig::Rtx::Rtx(const Rtx&) = default;
RtpConfig::Rtx::~Rtx() = default;

std::string RtpConfig::Rtx::ToString() const {
  char buf[1024];
  rtc::SimpleStringBuilder ss(buf);
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}
}  // namespace webrtc
