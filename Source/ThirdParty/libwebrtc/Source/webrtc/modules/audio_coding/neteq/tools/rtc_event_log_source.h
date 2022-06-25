/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_RTC_EVENT_LOG_SOURCE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_RTC_EVENT_LOG_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/audio_coding/neteq/tools/packet_source.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class RtpHeaderParser;

namespace test {

class Packet;

class RtcEventLogSource : public PacketSource {
 public:
  // Creates an RtcEventLogSource reading from `file_name`. If the file cannot
  // be opened, or has the wrong format, NULL will be returned.
  static std::unique_ptr<RtcEventLogSource> CreateFromFile(
      const std::string& file_name,
      absl::optional<uint32_t> ssrc_filter);
  // Same as above, but uses a string with the file contents.
  static std::unique_ptr<RtcEventLogSource> CreateFromString(
      const std::string& file_contents,
      absl::optional<uint32_t> ssrc_filter);

  virtual ~RtcEventLogSource();

  std::unique_ptr<Packet> NextPacket() override;

  // Returns the timestamp of the next audio output event, in milliseconds. The
  // maximum value of int64_t is returned if there are no more audio output
  // events available.
  int64_t NextAudioOutputEventMs();

 private:
  RtcEventLogSource();

  bool Initialize(const ParsedRtcEventLog& parsed_log,
                  absl::optional<uint32_t> ssrc_filter);

  std::vector<std::unique_ptr<Packet>> rtp_packets_;
  size_t rtp_packet_index_ = 0;
  std::vector<int64_t> audio_outputs_;
  size_t audio_output_index_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogSource);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_RTC_EVENT_LOG_SOURCE_H_
