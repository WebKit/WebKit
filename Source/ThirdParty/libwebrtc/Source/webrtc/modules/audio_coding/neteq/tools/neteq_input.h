/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_

#include <algorithm>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "modules/audio_coding/neteq/tools/packet_source.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace test {

// Interface class for input to the NetEqTest class.
class NetEqInput {
 public:
  struct PacketData {
    PacketData();
    ~PacketData();
    std::string ToString() const;

    RTPHeader header;
    rtc::Buffer payload;
    int64_t time_ms;
  };

  virtual ~NetEqInput() = default;

  // Returns at what time (in ms) NetEq::InsertPacket should be called next, or
  // empty if the source is out of packets.
  virtual absl::optional<int64_t> NextPacketTime() const = 0;

  // Returns at what time (in ms) NetEq::GetAudio should be called next, or
  // empty if no more output events are available.
  virtual absl::optional<int64_t> NextOutputEventTime() const = 0;

  // Returns the time (in ms) for the next event from either NextPacketTime()
  // or NextOutputEventTime(), or empty if both are out of events.
  absl::optional<int64_t> NextEventTime() const {
    const auto a = NextPacketTime();
    const auto b = NextOutputEventTime();
    // Return the minimum of non-empty `a` and `b`, or empty if both are empty.
    if (a) {
      return b ? std::min(*a, *b) : a;
    }
    return b ? b : absl::nullopt;
  }

  // Returns the next packet to be inserted into NetEq. The packet following the
  // returned one is pre-fetched in the NetEqInput object, such that future
  // calls to NextPacketTime() or NextHeader() will return information from that
  // packet.
  virtual std::unique_ptr<PacketData> PopPacket() = 0;

  // Move to the next output event. This will make NextOutputEventTime() return
  // a new value (potentially the same if several output events share the same
  // time).
  virtual void AdvanceOutputEvent() = 0;

  // Returns true if the source has come to an end. An implementation must
  // eventually return true from this method, or the test will end up in an
  // infinite loop.
  virtual bool ended() const = 0;

  // Returns the RTP header for the next packet, i.e., the packet that will be
  // delivered next by PopPacket().
  virtual absl::optional<RTPHeader> NextHeader() const = 0;
};

// Wrapper class to impose a time limit on a NetEqInput object, typically
// another time limit than what the object itself provides. For example, an
// input taken from a file can be cut shorter by wrapping it in this class.
class TimeLimitedNetEqInput : public NetEqInput {
 public:
  TimeLimitedNetEqInput(std::unique_ptr<NetEqInput> input, int64_t duration_ms);
  ~TimeLimitedNetEqInput() override;
  absl::optional<int64_t> NextPacketTime() const override;
  absl::optional<int64_t> NextOutputEventTime() const override;
  std::unique_ptr<PacketData> PopPacket() override;
  void AdvanceOutputEvent() override;
  bool ended() const override;
  absl::optional<RTPHeader> NextHeader() const override;

 private:
  void MaybeSetEnded();

  std::unique_ptr<NetEqInput> input_;
  const absl::optional<int64_t> start_time_ms_;
  const int64_t duration_ms_;
  bool ended_ = false;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_
