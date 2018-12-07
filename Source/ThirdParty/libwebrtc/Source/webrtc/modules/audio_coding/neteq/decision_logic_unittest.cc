/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for DecisionLogic class and derived classes.

#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/buffer_level_filter.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/delay_manager.h"
#include "modules/audio_coding/neteq/delay_peak_detector.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/tick_timer.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"

namespace webrtc {

TEST(DecisionLogic, CreateAndDestroy) {
  int fs_hz = 8000;
  int output_size_samples = fs_hz / 100;  // Samples per 10 ms.
  DecoderDatabase decoder_database(
      new rtc::RefCountedObject<MockAudioDecoderFactory>, absl::nullopt);
  TickTimer tick_timer;
  PacketBuffer packet_buffer(10, &tick_timer);
  DelayPeakDetector delay_peak_detector(&tick_timer);
  DelayManager delay_manager(240, 0, &delay_peak_detector, &tick_timer);
  BufferLevelFilter buffer_level_filter;
  DecisionLogic* logic = DecisionLogic::Create(
      fs_hz, output_size_samples, false, &decoder_database, packet_buffer,
      &delay_manager, &buffer_level_filter, &tick_timer);
  delete logic;
}

TEST(DecisionLogic, PostponeDecodingAfterExpansionSettings) {
  constexpr int kDefaultPostponeDecodingLevel = 50;
  constexpr int kFsHz = 8000;
  constexpr int kOutputSizeSamples = kFsHz / 100;  // Samples per 10 ms.
  DecoderDatabase decoder_database(
      new rtc::RefCountedObject<MockAudioDecoderFactory>, absl::nullopt);
  TickTimer tick_timer;
  PacketBuffer packet_buffer(10, &tick_timer);
  DelayPeakDetector delay_peak_detector(&tick_timer);
  DelayManager delay_manager(240, 0, &delay_peak_detector, &tick_timer);
  BufferLevelFilter buffer_level_filter;
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqPostponeDecodingAfterExpand/Enabled/");
    DecisionLogic logic(kFsHz, kOutputSizeSamples, false, &decoder_database,
                        packet_buffer, &delay_manager, &buffer_level_filter,
                        &tick_timer);
    EXPECT_EQ(kDefaultPostponeDecodingLevel,
              logic.postpone_decoding_level_for_test());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqPostponeDecodingAfterExpand/Enabled-65/");
    DecisionLogic logic(kFsHz, kOutputSizeSamples, false, &decoder_database,
                        packet_buffer, &delay_manager, &buffer_level_filter,
                        &tick_timer);
    EXPECT_EQ(65, logic.postpone_decoding_level_for_test());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqPostponeDecodingAfterExpand/Disabled/");
    DecisionLogic logic(kFsHz, kOutputSizeSamples, false, &decoder_database,
                        packet_buffer, &delay_manager, &buffer_level_filter,
                        &tick_timer);
    EXPECT_EQ(0, logic.postpone_decoding_level_for_test());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqPostponeDecodingAfterExpand/Enabled--1/");
    DecisionLogic logic(kFsHz, kOutputSizeSamples, false, &decoder_database,
                        packet_buffer, &delay_manager, &buffer_level_filter,
                        &tick_timer);
    EXPECT_EQ(kDefaultPostponeDecodingLevel,
              logic.postpone_decoding_level_for_test());
  }
  {
    test::ScopedFieldTrials field_trial(
        "WebRTC-Audio-NetEqPostponeDecodingAfterExpand/Enabled-101/");
    DecisionLogic logic(kFsHz, kOutputSizeSamples, false, &decoder_database,
                        packet_buffer, &delay_manager, &buffer_level_filter,
                        &tick_timer);
    EXPECT_EQ(kDefaultPostponeDecodingLevel,
              logic.postpone_decoding_level_for_test());
  }
}

// TODO(hlundin): Write more tests.

}  // namespace webrtc
