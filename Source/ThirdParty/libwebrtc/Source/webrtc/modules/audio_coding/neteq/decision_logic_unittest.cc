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

#include "api/neteq/neteq_controller.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/buffer_level_filter.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/delay_manager.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"

namespace webrtc {

TEST(DecisionLogic, CreateAndDestroy) {
  int fs_hz = 8000;
  int output_size_samples = fs_hz / 100;  // Samples per 10 ms.
  DecoderDatabase decoder_database(
      new rtc::RefCountedObject<MockAudioDecoderFactory>, absl::nullopt);
  TickTimer tick_timer;
  StatisticsCalculator stats;
  PacketBuffer packet_buffer(10, &tick_timer);
  BufferLevelFilter buffer_level_filter;
  NetEqController::Config config;
  config.tick_timer = &tick_timer;
  config.base_min_delay_ms = 0;
  config.max_packets_in_buffer = 240;
  config.enable_rtx_handling = false;
  config.allow_time_stretching = true;
  auto logic = std::make_unique<DecisionLogic>(std::move(config));
  logic->SetSampleRate(fs_hz, output_size_samples);
}

// TODO(hlundin): Write more tests.

}  // namespace webrtc
