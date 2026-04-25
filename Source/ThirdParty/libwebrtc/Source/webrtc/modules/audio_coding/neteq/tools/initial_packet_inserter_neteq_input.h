/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_INITIAL_PACKET_INSERTER_NETEQ_INPUT_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_INITIAL_PACKET_INSERTER_NETEQ_INPUT_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {
namespace test {

// Wrapper class that can insert a number of packets at the start of the
// simulation.
class InitialPacketInserterNetEqInput final : public NetEqInput {
 public:
  InitialPacketInserterNetEqInput(std::unique_ptr<NetEqInput> source,
                                  int number_of_initial_packets,
                                  int sample_rate_hz);
  std::optional<int64_t> NextPacketTime() const override;
  std::optional<int64_t> NextOutputEventTime() const override;
  std::optional<SetMinimumDelayInfo> NextSetMinimumDelayInfo() const override;
  std::unique_ptr<RtpPacketReceived> PopPacket() override;
  void AdvanceOutputEvent() override;
  void AdvanceSetMinimumDelay() override;
  bool ended() const override;
  const RtpPacketReceived* NextPacket() const override;

 private:
  const std::unique_ptr<NetEqInput> source_;
  int packets_to_insert_;
  const int sample_rate_hz_;
  std::unique_ptr<RtpPacketReceived> first_packet_;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_INITIAL_PACKET_INSERTER_NETEQ_INPUT_H_
