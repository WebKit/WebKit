/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_FAX_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_FAX_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/neteq/decision_logic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Implementation of the DecisionLogic class for playout modes kPlayoutFax and
// kPlayoutOff.
class DecisionLogicFax : public DecisionLogic {
 public:
  // Constructor.
  DecisionLogicFax(int fs_hz,
                   size_t output_size_samples,
                   NetEqPlayoutMode playout_mode,
                   DecoderDatabase* decoder_database,
                   const PacketBuffer& packet_buffer,
                   DelayManager* delay_manager,
                   BufferLevelFilter* buffer_level_filter,
                   const TickTimer* tick_timer)
      : DecisionLogic(fs_hz,
                      output_size_samples,
                      playout_mode,
                      decoder_database,
                      packet_buffer,
                      delay_manager,
                      buffer_level_filter,
                      tick_timer) {}

 protected:
  Operations GetDecisionSpecialized(const SyncBuffer& sync_buffer,
                                    const Expand& expand,
                                    size_t decoder_frame_length,
                                    const Packet* next_packet,
                                    Modes prev_mode,
                                    bool play_dtmf,
                                    bool* reset_decoder,
                                    size_t generated_noise_samples) override;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(DecisionLogicFax);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_FAX_H_
