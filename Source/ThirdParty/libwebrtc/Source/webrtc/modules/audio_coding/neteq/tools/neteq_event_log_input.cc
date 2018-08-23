/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"

#include <limits>

#include "modules/audio_coding/neteq/tools/rtc_event_log_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

NetEqEventLogInput::NetEqEventLogInput(const std::string& file_name,
                                       const RtpHeaderExtensionMap& hdr_ext_map)
    : source_(RtcEventLogSource::Create(file_name)) {
  for (const auto& ext_pair : hdr_ext_map) {
    source_->RegisterRtpHeaderExtension(ext_pair.second, ext_pair.first);
  }
  LoadNextPacket();
  AdvanceOutputEvent();
}

absl::optional<int64_t> NetEqEventLogInput::NextOutputEventTime() const {
  return next_output_event_ms_;
}

void NetEqEventLogInput::AdvanceOutputEvent() {
  next_output_event_ms_ = source_->NextAudioOutputEventMs();
  if (*next_output_event_ms_ == std::numeric_limits<int64_t>::max()) {
    next_output_event_ms_ = absl::nullopt;
  }
}

PacketSource* NetEqEventLogInput::source() {
  return source_.get();
}

}  // namespace test
}  // namespace webrtc
