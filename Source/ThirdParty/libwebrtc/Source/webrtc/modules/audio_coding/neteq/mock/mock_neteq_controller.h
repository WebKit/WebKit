/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_NETEQ_CONTROLLER_H_
#define MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_NETEQ_CONTROLLER_H_

#include "api/neteq/neteq_controller.h"
#include "test/gmock.h"

namespace webrtc {

class MockNetEqController : public NetEqController {
 public:
  MockNetEqController() = default;
  virtual ~MockNetEqController() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD0(SoftReset, void());
  MOCK_METHOD2(GetDecision,
               NetEq::Operation(const NetEqStatus& neteq_status,
                                bool* reset_decoder));
  MOCK_METHOD6(Update,
               void(uint16_t sequence_number,
                    uint32_t timestamp,
                    uint32_t last_played_out_timestamp,
                    bool new_codec,
                    bool cng_or_dtmf,
                    size_t packet_length_samples));
  MOCK_METHOD0(RegisterEmptyPacket, void());
  MOCK_METHOD2(SetSampleRate, void(int fs_hz, size_t output_size_samples));
  MOCK_METHOD1(SetMaximumDelay, bool(int delay_ms));
  MOCK_METHOD1(SetMinimumDelay, bool(int delay_ms));
  MOCK_METHOD1(SetBaseMinimumDelay, bool(int delay_ms));
  MOCK_CONST_METHOD0(GetBaseMinimumDelay, int());
  MOCK_CONST_METHOD0(CngRfc3389On, bool());
  MOCK_CONST_METHOD0(CngOff, bool());
  MOCK_METHOD0(SetCngOff, void());
  MOCK_METHOD1(ExpandDecision, void(NetEq::Operation operation));
  MOCK_METHOD1(AddSampleMemory, void(int32_t value));
  MOCK_METHOD0(TargetLevelMs, int());
  MOCK_METHOD6(PacketArrived,
               absl::optional<int>(bool last_cng_or_dtmf,
                                   size_t packet_length_samples,
                                   bool should_update_stats,
                                   uint16_t main_sequence_number,
                                   uint32_t main_timestamp,
                                   int fs_hz));
  MOCK_CONST_METHOD0(PeakFound, bool());
  MOCK_CONST_METHOD0(GetFilteredBufferLevel, int());
  MOCK_METHOD1(set_sample_memory, void(int32_t value));
  MOCK_CONST_METHOD0(noise_fast_forward, size_t());
  MOCK_CONST_METHOD0(packet_length_samples, size_t());
  MOCK_METHOD1(set_packet_length_samples, void(size_t value));
  MOCK_METHOD1(set_prev_time_scale, void(bool value));
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_MOCK_MOCK_NETEQ_CONTROLLER_H_
