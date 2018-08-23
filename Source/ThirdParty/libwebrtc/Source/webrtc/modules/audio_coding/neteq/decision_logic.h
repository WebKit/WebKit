/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_H_
#define MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_H_

#include "modules/audio_coding/neteq/defines.h"
#include "modules/audio_coding/neteq/include/neteq.h"
#include "modules/audio_coding/neteq/tick_timer.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Forward declarations.
class BufferLevelFilter;
class DecoderDatabase;
class DelayManager;
class Expand;
class PacketBuffer;
class SyncBuffer;
struct Packet;

// This is the class for the decision tree implementation.
class DecisionLogic final {
 public:
  // Static factory function which creates different types of objects depending
  // on the |playout_mode|.
  static DecisionLogic* Create(int fs_hz,
                               size_t output_size_samples,
                               bool disallow_time_stretching,
                               DecoderDatabase* decoder_database,
                               const PacketBuffer& packet_buffer,
                               DelayManager* delay_manager,
                               BufferLevelFilter* buffer_level_filter,
                               const TickTimer* tick_timer);

  static const int kReinitAfterExpands = 100;
  static const int kMaxWaitForPacket = 10;

  // Constructor.
  DecisionLogic(int fs_hz,
                size_t output_size_samples,
                bool disallow_time_stretching,
                DecoderDatabase* decoder_database,
                const PacketBuffer& packet_buffer,
                DelayManager* delay_manager,
                BufferLevelFilter* buffer_level_filter,
                const TickTimer* tick_timer);

  ~DecisionLogic();

  // Resets object to a clean state.
  void Reset();

  // Resets parts of the state. Typically done when switching codecs.
  void SoftReset();

  // Sets the sample rate and the output block size.
  void SetSampleRate(int fs_hz, size_t output_size_samples);

  // Returns the operation that should be done next. |sync_buffer| and |expand|
  // are provided for reference. |decoder_frame_length| is the number of samples
  // obtained from the last decoded frame. If there is a packet available, it
  // should be supplied in |next_packet|; otherwise it should be NULL. The mode
  // resulting from the last call to NetEqImpl::GetAudio is supplied in
  // |prev_mode|. If there is a DTMF event to play, |play_dtmf| should be set to
  // true. The output variable |reset_decoder| will be set to true if a reset is
  // required; otherwise it is left unchanged (i.e., it can remain true if it
  // was true before the call).  This method end with calling
  // GetDecisionSpecialized to get the actual return value.
  Operations GetDecision(const SyncBuffer& sync_buffer,
                         const Expand& expand,
                         size_t decoder_frame_length,
                         const Packet* next_packet,
                         Modes prev_mode,
                         bool play_dtmf,
                         size_t generated_noise_samples,
                         bool* reset_decoder);

  // These methods test the |cng_state_| for different conditions.
  bool CngRfc3389On() const { return cng_state_ == kCngRfc3389On; }
  bool CngOff() const { return cng_state_ == kCngOff; }

  // Resets the |cng_state_| to kCngOff.
  void SetCngOff() { cng_state_ = kCngOff; }

  // Reports back to DecisionLogic whether the decision to do expand remains or
  // not. Note that this is necessary, since an expand decision can be changed
  // to kNormal in NetEqImpl::GetDecision if there is still enough data in the
  // sync buffer.
  void ExpandDecision(Operations operation);

  // Adds |value| to |sample_memory_|.
  void AddSampleMemory(int32_t value) { sample_memory_ += value; }

  // Accessors and mutators.
  void set_sample_memory(int32_t value) { sample_memory_ = value; }
  size_t noise_fast_forward() const { return noise_fast_forward_; }
  size_t packet_length_samples() const { return packet_length_samples_; }
  void set_packet_length_samples(size_t value) {
    packet_length_samples_ = value;
  }
  void set_prev_time_scale(bool value) { prev_time_scale_ = value; }

 private:
  // The value 5 sets maximum time-stretch rate to about 100 ms/s.
  static const int kMinTimescaleInterval = 5;

  enum CngState { kCngOff, kCngRfc3389On, kCngInternalOn };

  // Updates the |buffer_level_filter_| with the current buffer level
  // |buffer_size_packets|.
  void FilterBufferLevel(size_t buffer_size_packets, Modes prev_mode);

  // Returns the operation given that the next available packet is a comfort
  // noise payload (RFC 3389 only, not codec-internal).
  Operations CngOperation(Modes prev_mode,
                          uint32_t target_timestamp,
                          uint32_t available_timestamp,
                          size_t generated_noise_samples);

  // Returns the operation given that no packets are available (except maybe
  // a DTMF event, flagged by setting |play_dtmf| true).
  Operations NoPacket(bool play_dtmf);

  // Returns the operation to do given that the expected packet is available.
  Operations ExpectedPacketAvailable(Modes prev_mode, bool play_dtmf);

  // Returns the operation to do given that the expected packet is not
  // available, but a packet further into the future is at hand.
  Operations FuturePacketAvailable(const SyncBuffer& sync_buffer,
                                   const Expand& expand,
                                   size_t decoder_frame_length,
                                   Modes prev_mode,
                                   uint32_t target_timestamp,
                                   uint32_t available_timestamp,
                                   bool play_dtmf,
                                   size_t generated_noise_samples);

  // Checks if enough time has elapsed since the last successful timescale
  // operation was done (i.e., accelerate or preemptive expand).
  bool TimescaleAllowed() const {
    return !timescale_countdown_ || timescale_countdown_->Finished();
  }

  // Checks if the current (filtered) buffer level is under the target level.
  bool UnderTargetLevel() const;

  // Checks if |timestamp_leap| is so long into the future that a reset due
  // to exceeding kReinitAfterExpands will be done.
  bool ReinitAfterExpands(uint32_t timestamp_leap) const;

  // Checks if we still have not done enough expands to cover the distance from
  // the last decoded packet to the next available packet, the distance beeing
  // conveyed in |timestamp_leap|.
  bool PacketTooEarly(uint32_t timestamp_leap) const;

  // Checks if num_consecutive_expands_ >= kMaxWaitForPacket.
  bool MaxWaitForPacket() const;

  DecoderDatabase* decoder_database_;
  const PacketBuffer& packet_buffer_;
  DelayManager* delay_manager_;
  BufferLevelFilter* buffer_level_filter_;
  const TickTimer* tick_timer_;
  int fs_mult_;
  size_t output_size_samples_;
  CngState cng_state_;  // Remember if comfort noise is interrupted by other
                        // event (e.g., DTMF).
  size_t noise_fast_forward_ = 0;
  size_t packet_length_samples_;
  int sample_memory_;
  bool prev_time_scale_;
  bool disallow_time_stretching_;
  std::unique_ptr<TickTimer::Countdown> timescale_countdown_;
  int num_consecutive_expands_;
  const bool postpone_decoding_after_expand_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DecisionLogic);
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_DECISION_LOGIC_H_
