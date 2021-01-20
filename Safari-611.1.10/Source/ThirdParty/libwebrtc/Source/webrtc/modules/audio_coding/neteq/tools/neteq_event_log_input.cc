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
#include <memory>

#include "modules/audio_coding/neteq/tools/rtc_event_log_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

NetEqEventLogInput* NetEqEventLogInput::CreateFromFile(
    const std::string& file_name,
    absl::optional<uint32_t> ssrc_filter) {
  auto event_log_src =
      RtcEventLogSource::CreateFromFile(file_name, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return new NetEqEventLogInput(std::move(event_log_src));
}

NetEqEventLogInput* NetEqEventLogInput::CreateFromString(
    const std::string& file_contents,
    absl::optional<uint32_t> ssrc_filter) {
  auto event_log_src =
      RtcEventLogSource::CreateFromString(file_contents, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return new NetEqEventLogInput(std::move(event_log_src));
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

NetEqEventLogInput::NetEqEventLogInput(
    std::unique_ptr<RtcEventLogSource> source)
    : source_(std::move(source)) {
  LoadNextPacket();
  AdvanceOutputEvent();
}

}  // namespace test
}  // namespace webrtc
