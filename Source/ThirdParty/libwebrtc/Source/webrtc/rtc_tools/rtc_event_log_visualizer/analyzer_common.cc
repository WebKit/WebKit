
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

#include <cstdint>
#include <string>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
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
  char buffer[200];
  SimpleStringBuilder name(buffer);
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
  return name.str();
}

std::string GetLayerName(LayerDescription layer) {
  char buffer[100];
  SimpleStringBuilder name(buffer);
  name << "SSRC " << layer.ssrc << " sl " << layer.spatial_layer << ", tl "
       << layer.temporal_layer;
  return name.str();
}

}  // namespace webrtc
