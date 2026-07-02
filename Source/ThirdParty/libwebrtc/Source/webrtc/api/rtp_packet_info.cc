/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtp_packet_info.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "api/rtp_headers.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

RtpPacketInfo::RtpPacketInfo()
    : sequence_number_(0),
      ssrc_(0),
      rtp_timestamp_(0),
      receive_time_(Timestamp::MinusInfinity()) {}

RtpPacketInfo::RtpPacketInfo(const RtpPacketReceived& rtp_packet)
    : sequence_number_(rtp_packet.SequenceNumber()),
      ssrc_(rtp_packet.Ssrc()),
      csrcs_(rtp_packet.Csrcs()),
      rtp_timestamp_(rtp_packet.Timestamp()),
      receive_time_(rtp_packet.arrival_time()) {
  AudioLevel audio_level;
  if (rtp_packet.GetExtension<AudioLevelExtension>(&audio_level)) {
    audio_level_ = audio_level.level();
  }

  AbsoluteCaptureTime capture_time;
  if (rtp_packet.GetExtension<AbsoluteCaptureTimeExtension>(&capture_time)) {
    absolute_capture_time_ = std::move(capture_time);
  }
}

RtpPacketInfo::RtpPacketInfo(uint32_t ssrc,
                             std::vector<uint32_t> csrcs,
                             uint32_t rtp_timestamp,
                             Timestamp receive_time)
    : sequence_number_(0),
      ssrc_(ssrc),
      csrcs_(std::move(csrcs)),
      rtp_timestamp_(rtp_timestamp),
      receive_time_(receive_time) {}

RtpPacketInfo::RtpPacketInfo(const RTPHeader& rtp_header,
                             Timestamp receive_time)
    : sequence_number_(rtp_header.sequenceNumber),
      ssrc_(rtp_header.ssrc),
      rtp_timestamp_(rtp_header.timestamp),
      receive_time_(receive_time) {
  const auto& extension = rtp_header.extension;
  const auto csrcs_count = std::min<size_t>(rtp_header.numCSRCs, kRtpCsrcSize);

  csrcs_.assign(&rtp_header.arrOfCSRCs[0], &rtp_header.arrOfCSRCs[csrcs_count]);

  if (extension.audio_level()) {
    audio_level_ = extension.audio_level()->level();
  }

  absolute_capture_time_ = extension.absolute_capture_time;
}

bool operator==(const RtpPacketInfo& lhs, const RtpPacketInfo& rhs) = default;

}  // namespace webrtc
