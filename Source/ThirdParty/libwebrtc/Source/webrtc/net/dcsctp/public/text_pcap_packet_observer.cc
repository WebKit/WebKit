/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/public/text_pcap_packet_observer.h"

#include <cstdint>
#include <span>

#include "net/dcsctp/public/types.h"
#include "rtc_base/logging.h"
#include "rtc_base/text2pcap.h"

namespace dcsctp {

void TextPcapPacketObserver::OnSentPacket(dcsctp::TimeMs now,
                                          std::span<const uint8_t> payload) {
  RTC_LOG(LS_VERBOSE) << "\n"
                      << webrtc::Text2Pcap::DumpPacket(/*outbound=*/true,
                                                       payload, *now)
                      << " # SCTP_PACKET " << name_;
}

void TextPcapPacketObserver::OnReceivedPacket(
    dcsctp::TimeMs now,
    std::span<const uint8_t> payload) {
  RTC_LOG(LS_VERBOSE) << "\n"
                      << webrtc::Text2Pcap::DumpPacket(/*outbound=*/false,
                                                       payload, *now)
                      << " # SCTP_PACKET " << name_;
}

}  // namespace dcsctp
