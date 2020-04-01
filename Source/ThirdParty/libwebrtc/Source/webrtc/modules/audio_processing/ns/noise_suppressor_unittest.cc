/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/ns/noise_suppressor.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(int sample_rate_hz,
                             size_t num_channels,
                             NsConfig::SuppressionLevel level) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz << ", num_channels: " << num_channels
     << ", level: " << static_cast<int>(level);
  return ss.Release();
}

void PopulateInputFrameWithIdenticalChannels(size_t num_channels,
                                             size_t num_bands,
                                             size_t frame_index,
                                             AudioBuffer* audio) {
  for (size_t ch = 0; ch < num_channels; ++ch) {
    for (size_t b = 0; b < num_bands; ++b) {
      for (size_t i = 0; i < 160; ++i) {
        float value = static_cast<int>(frame_index * 160 + i);
        audio->split_bands(ch)[b][i] = (value > 0 ? 5000 * b + value : 0);
      }
    }
  }
}

void VerifyIdenticalChannels(size_t num_channels,
                             size_t num_bands,
                             size_t frame_index,
                             const AudioBuffer& audio) {
  EXPECT_GT(num_channels, 1u);
  for (size_t ch = 1; ch < num_channels; ++ch) {
    for (size_t b = 0; b < num_bands; ++b) {
      for (size_t i = 0; i < 160; ++i) {
        EXPECT_EQ(audio.split_bands_const(ch)[b][i],
                  audio.split_bands_const(0)[b][i]);
      }
    }
  }
}

}  // namespace

// Verifies that the same noise reduction effect is applied to all channels.
TEST(NoiseSuppressor, IdenticalChannelEffects) {
  for (auto rate : {16000, 32000, 48000}) {
    for (auto num_channels : {1, 4, 8}) {
      for (auto level :
           {NsConfig::SuppressionLevel::k6dB, NsConfig::SuppressionLevel::k12dB,
            NsConfig::SuppressionLevel::k18dB,
            NsConfig::SuppressionLevel::k21dB}) {
        SCOPED_TRACE(ProduceDebugText(rate, num_channels, level));

        const size_t num_bands = rate / 16000;
        // const int frame_length = rtc::CheckedDivExact(rate, 100);
        AudioBuffer audio(rate, num_channels, rate, num_channels, rate,
                          num_channels);
        NsConfig cfg;
        NoiseSuppressor ns(cfg, rate, num_channels);
        for (size_t frame_index = 0; frame_index < 1000; ++frame_index) {
          if (rate > 16000) {
            audio.SplitIntoFrequencyBands();
          }

          PopulateInputFrameWithIdenticalChannels(num_channels, num_bands,
                                                  frame_index, &audio);

          ns.Analyze(audio);
          ns.Process(&audio);
          if (num_channels > 1) {
            VerifyIdenticalChannels(num_channels, num_bands, frame_index,
                                    audio);
          }
        }
      }
    }
  }
}

}  // namespace webrtc
