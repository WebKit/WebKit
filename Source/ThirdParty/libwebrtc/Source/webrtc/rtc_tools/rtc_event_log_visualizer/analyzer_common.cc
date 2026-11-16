
/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyzer_common.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

bool IsRtxSsrc(const ParsedRtcEventLog& parsed_log,
               PacketDirection direction,
               uint32_t ssrc) {
  if (direction == kIncomingPacket) {
    return parsed_log.incoming_rtx_ssrcs().find(ssrc) !=
           parsed_log.incoming_rtx_ssrcs().end();
  } else {
    return parsed_log.outgoing_rtx_ssrcs().find(ssrc) !=
           parsed_log.outgoing_rtx_ssrcs().end();
  }
}

bool IsVideoSsrc(const ParsedRtcEventLog& parsed_log,
                 PacketDirection direction,
                 uint32_t ssrc) {
  if (direction == kIncomingPacket) {
    return parsed_log.incoming_video_ssrcs().find(ssrc) !=
           parsed_log.incoming_video_ssrcs().end();
  } else {
    return parsed_log.outgoing_video_ssrcs().find(ssrc) !=
           parsed_log.outgoing_video_ssrcs().end();
  }
}

bool IsAudioSsrc(const ParsedRtcEventLog& parsed_log,
                 PacketDirection direction,
                 uint32_t ssrc) {
  if (direction == kIncomingPacket) {
    return parsed_log.incoming_audio_ssrcs().find(ssrc) !=
           parsed_log.incoming_audio_ssrcs().end();
  } else {
    return parsed_log.outgoing_audio_ssrcs().find(ssrc) !=
           parsed_log.outgoing_audio_ssrcs().end();
  }
}

std::string GetStreamName(const ParsedRtcEventLog& parsed_log,
                          PacketDirection direction,
                          uint32_t ssrc) {
  StringBuilder name;
  if (IsAudioSsrc(parsed_log, direction, ssrc)) {
    name << "Audio ";
  } else if (IsVideoSsrc(parsed_log, direction, ssrc)) {
    name << "Video ";
  } else {
    name << "Unknown ";
  }
  if (IsRtxSsrc(parsed_log, direction, ssrc)) {
    name << "RTX ";
  }
  if (direction == kIncomingPacket)
    name << "(In) ";
  else
    name << "(Out) ";
  name << "SSRC " << ssrc;
  return name.Release();
}

std::string GetLayerName(LayerDescription layer) {
  StringBuilder name;
  name << "SSRC " << layer.ssrc << " sl " << layer.spatial_layer << ", tl "
       << layer.temporal_layer;
  return name.Release();
}

std::string SsrcToString(uint32_t ssrc) {
  StringBuilder ss;
  ss << "SSRC " << ssrc;
  return ss.Release();
}

// Checks whether an SSRC is contained in the list of desired SSRCs.
// Note that an empty SSRC list matches every SSRC.
bool MatchingSsrc(uint32_t ssrc, const std::vector<uint32_t>& desired_ssrc) {
  if (desired_ssrc.empty())
    return true;
  return std::find(desired_ssrc.begin(), desired_ssrc.end(), ssrc) !=
         desired_ssrc.end();
}

// Computes the difference `later` - `earlier` where `later` and `earlier`
// are counters that wrap at `modulus`. The difference is chosen to have the
// least absolute value. For example if `modulus` is 8, then the difference will
// be chosen in the range [-3, 4]. If `modulus` is 9, then the difference will
// be in [-4, 4].
int64_t WrappingDifference(uint32_t later, uint32_t earlier, int64_t modulus) {
  RTC_DCHECK_LE(1, modulus);
  RTC_DCHECK_LT(later, modulus);
  RTC_DCHECK_LT(earlier, modulus);
  int64_t difference =
      static_cast<int64_t>(later) - static_cast<int64_t>(earlier);
  int64_t max_difference = modulus / 2;
  int64_t min_difference = max_difference - modulus + 1;
  if (difference > max_difference) {
    difference -= modulus;
  }
  if (difference < min_difference) {
    difference += modulus;
  }
  if (difference > max_difference / 2 || difference < min_difference / 2) {
    RTC_LOG(LS_WARNING) << "Difference between" << later << " and " << earlier
                        << " expected to be in the range ("
                        << min_difference / 2 << "," << max_difference / 2
                        << ") but is " << difference
                        << ". Correct unwrapping is uncertain.";
  }
  return difference;
}

std::string GetDirectionAsString(PacketDirection direction) {
  if (direction == kIncomingPacket) {
    return "Incoming";
  } else {
    return "Outgoing";
  }
}

std::string GetDirectionAsShortString(PacketDirection direction) {
  if (direction == kIncomingPacket) {
    return "In";
  } else {
    return "Out";
  }
}

}  // namespace webrtc
