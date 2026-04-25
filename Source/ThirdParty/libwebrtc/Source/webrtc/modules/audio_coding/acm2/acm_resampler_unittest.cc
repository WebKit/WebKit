/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_resampler.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/audio/audio_frame.h"
#include "test/gtest.h"

namespace webrtc {
namespace acm2 {

TEST(ResamplerHelperTest, MaybeResampleCheckForMaxSize) {
  ResamplerHelper resampler;
  AudioFrame audio_frame;

  // Create an audio frame that requires resampling from 32kHz to 48kHz
  // with a very high number of channels (24).
  const int kCurrentSampleRateHz = 32000;
  const int kDesiredSampleRateHz = 48000;
  const size_t kChannels = 24;

  // 10 ms of data at 32kHz = 320 samples per channel.
  std::vector<int16_t> dummy_data(320 * 24, 0);
  audio_frame.UpdateFrame(0, dummy_data.data(), 320, kCurrentSampleRateHz,
                          AudioFrame::kNormalSpeech, AudioFrame::kVadActive,
                          kChannels);

  // The resampler prime path will attempt to allocate a buffer that is
  // kChannels * (kDesiredSampleRateHz / 100) = 24 * 480 = 11520 samples,
  // which exceeds AudioFrame::kMaxDataSizeSamples (7680).
  const bool resample_success =
      resampler.MaybeResample(kDesiredSampleRateHz, &audio_frame);

  // Verify that MaybeResample correctly detects the buffer size condition and
  // safely aborts the operation by returning false and muting the frame.
  EXPECT_FALSE(resample_success);
  EXPECT_TRUE(audio_frame.muted());
  EXPECT_EQ(audio_frame.sample_rate_hz_, kDesiredSampleRateHz);
  EXPECT_EQ(audio_frame.num_channels_, kChannels);
}

TEST(ResamplerHelperTest, MaybeResampleValidMaxSize) {
  ResamplerHelper resampler;
  AudioFrame audio_frame;

  // Ensure that resampling within the valid buffer size does not trigger the
  // muting behavior. We'll use a valid number of channels (e.g. 1) that will
  // not exceed the bounds.
  const int kCurrentSampleRateHz = 32000;
  const int kDesiredSampleRateHz = 48000;
  const size_t kChannels = 1;

  std::vector<int16_t> dummy_data(320 * 1, 1000);
  audio_frame.UpdateFrame(0, dummy_data.data(), 320, kCurrentSampleRateHz,
                          AudioFrame::kNormalSpeech, AudioFrame::kVadActive,
                          kChannels);

  const bool resample_success =
      resampler.MaybeResample(kDesiredSampleRateHz, &audio_frame);

  EXPECT_TRUE(resample_success);
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_EQ(audio_frame.sample_rate_hz_, kDesiredSampleRateHz);
  EXPECT_EQ(audio_frame.num_channels_, kChannels);
}

}  // namespace acm2
}  // namespace webrtc
