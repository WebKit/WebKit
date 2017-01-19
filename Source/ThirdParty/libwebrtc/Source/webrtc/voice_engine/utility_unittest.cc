/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "webrtc/base/format_macros.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/utility.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {
namespace {

class UtilityTest : public ::testing::Test {
 protected:
  UtilityTest() {
    src_frame_.sample_rate_hz_ = 16000;
    src_frame_.samples_per_channel_ = src_frame_.sample_rate_hz_ / 100;
    src_frame_.num_channels_ = 1;
    dst_frame_.CopyFrom(src_frame_);
    golden_frame_.CopyFrom(src_frame_);
  }

  void RunResampleTest(int src_channels,
                       int src_sample_rate_hz,
                       int dst_channels,
                       int dst_sample_rate_hz);

  PushResampler<int16_t> resampler_;
  AudioFrame src_frame_;
  AudioFrame dst_frame_;
  AudioFrame golden_frame_;
};

// Sets the signal value to increase by |data| with every sample. Floats are
// used so non-integer values result in rounding error, but not an accumulating
// error.
void SetMonoFrame(AudioFrame* frame, float data, int sample_rate_hz) {
  memset(frame->data_, 0, sizeof(frame->data_));
  frame->num_channels_ = 1;
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = sample_rate_hz / 100;
  for (size_t i = 0; i < frame->samples_per_channel_; i++) {
    frame->data_[i] = static_cast<int16_t>(data * i);
  }
}

// Keep the existing sample rate.
void SetMonoFrame(AudioFrame* frame, float data) {
  SetMonoFrame(frame, data, frame->sample_rate_hz_);
}

// Sets the signal value to increase by |left| and |right| with every sample in
// each channel respectively.
void SetStereoFrame(AudioFrame* frame, float left, float right,
                    int sample_rate_hz) {
  memset(frame->data_, 0, sizeof(frame->data_));
  frame->num_channels_ = 2;
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = sample_rate_hz / 100;
  for (size_t i = 0; i < frame->samples_per_channel_; i++) {
    frame->data_[i * 2] = static_cast<int16_t>(left * i);
    frame->data_[i * 2 + 1] = static_cast<int16_t>(right * i);
  }
}

// Keep the existing sample rate.
void SetStereoFrame(AudioFrame* frame, float left, float right) {
  SetStereoFrame(frame, left, right, frame->sample_rate_hz_);
}

void VerifyParams(const AudioFrame& ref_frame, const AudioFrame& test_frame) {
  EXPECT_EQ(ref_frame.num_channels_, test_frame.num_channels_);
  EXPECT_EQ(ref_frame.samples_per_channel_, test_frame.samples_per_channel_);
  EXPECT_EQ(ref_frame.sample_rate_hz_, test_frame.sample_rate_hz_);
}

// Computes the best SNR based on the error between |ref_frame| and
// |test_frame|. It allows for up to a |max_delay| in samples between the
// signals to compensate for the resampling delay.
float ComputeSNR(const AudioFrame& ref_frame, const AudioFrame& test_frame,
                 size_t max_delay) {
  VerifyParams(ref_frame, test_frame);
  float best_snr = 0;
  size_t best_delay = 0;
  for (size_t delay = 0; delay <= max_delay; delay++) {
    float mse = 0;
    float variance = 0;
    for (size_t i = 0; i < ref_frame.samples_per_channel_ *
        ref_frame.num_channels_ - delay; i++) {
      int error = ref_frame.data_[i] - test_frame.data_[i + delay];
      mse += error * error;
      variance += ref_frame.data_[i] * ref_frame.data_[i];
    }
    float snr = 100;  // We assign 100 dB to the zero-error case.
    if (mse > 0)
      snr = 10 * log10(variance / mse);
    if (snr > best_snr) {
      best_snr = snr;
      best_delay = delay;
    }
  }
  printf("SNR=%.1f dB at delay=%" PRIuS "\n", best_snr, best_delay);
  return best_snr;
}

void VerifyFramesAreEqual(const AudioFrame& ref_frame,
                          const AudioFrame& test_frame) {
  VerifyParams(ref_frame, test_frame);
  for (size_t i = 0;
       i < ref_frame.samples_per_channel_ * ref_frame.num_channels_; i++) {
    EXPECT_EQ(ref_frame.data_[i], test_frame.data_[i]);
  }
}

void UtilityTest::RunResampleTest(int src_channels,
                                  int src_sample_rate_hz,
                                  int dst_channels,
                                  int dst_sample_rate_hz) {
  PushResampler<int16_t> resampler;  // Create a new one with every test.
  const int16_t kSrcLeft = 30;  // Shouldn't overflow for any used sample rate.
  const int16_t kSrcRight = 15;
  const float resampling_factor = (1.0 * src_sample_rate_hz) /
      dst_sample_rate_hz;
  const float dst_left = resampling_factor * kSrcLeft;
  const float dst_right = resampling_factor * kSrcRight;
  const float dst_mono = (dst_left + dst_right) / 2;
  if (src_channels == 1)
    SetMonoFrame(&src_frame_, kSrcLeft, src_sample_rate_hz);
  else
    SetStereoFrame(&src_frame_, kSrcLeft, kSrcRight, src_sample_rate_hz);

  if (dst_channels == 1) {
    SetMonoFrame(&dst_frame_, 0, dst_sample_rate_hz);
    if (src_channels == 1)
      SetMonoFrame(&golden_frame_, dst_left, dst_sample_rate_hz);
    else
      SetMonoFrame(&golden_frame_, dst_mono, dst_sample_rate_hz);
  } else {
    SetStereoFrame(&dst_frame_, 0, 0, dst_sample_rate_hz);
    if (src_channels == 1)
      SetStereoFrame(&golden_frame_, dst_left, dst_left, dst_sample_rate_hz);
    else
      SetStereoFrame(&golden_frame_, dst_left, dst_right, dst_sample_rate_hz);
  }

  // The sinc resampler has a known delay, which we compute here. Multiplying by
  // two gives us a crude maximum for any resampling, as the old resampler
  // typically (but not always) has lower delay.
  static const size_t kInputKernelDelaySamples = 16;
  const size_t max_delay = static_cast<size_t>(
      static_cast<double>(dst_sample_rate_hz) / src_sample_rate_hz *
      kInputKernelDelaySamples * dst_channels * 2);
  printf("(%d, %d Hz) -> (%d, %d Hz) ",  // SNR reported on the same line later.
      src_channels, src_sample_rate_hz, dst_channels, dst_sample_rate_hz);
  RemixAndResample(src_frame_, &resampler, &dst_frame_);

  if (src_sample_rate_hz == 96000 && dst_sample_rate_hz == 8000) {
    // The sinc resampler gives poor SNR at this extreme conversion, but we
    // expect to see this rarely in practice.
    EXPECT_GT(ComputeSNR(golden_frame_, dst_frame_, max_delay), 14.0f);
  } else {
    EXPECT_GT(ComputeSNR(golden_frame_, dst_frame_, max_delay), 46.0f);
  }
}

TEST_F(UtilityTest, RemixAndResampleCopyFrameSucceeds) {
  // Stereo -> stereo.
  SetStereoFrame(&src_frame_, 10, 10);
  SetStereoFrame(&dst_frame_, 0, 0);
  RemixAndResample(src_frame_, &resampler_, &dst_frame_);
  VerifyFramesAreEqual(src_frame_, dst_frame_);

  // Mono -> mono.
  SetMonoFrame(&src_frame_, 20);
  SetMonoFrame(&dst_frame_, 0);
  RemixAndResample(src_frame_, &resampler_, &dst_frame_);
  VerifyFramesAreEqual(src_frame_, dst_frame_);
}

TEST_F(UtilityTest, RemixAndResampleMixingOnlySucceeds) {
  // Stereo -> mono.
  SetStereoFrame(&dst_frame_, 0, 0);
  SetMonoFrame(&src_frame_, 10);
  SetStereoFrame(&golden_frame_, 10, 10);
  RemixAndResample(src_frame_, &resampler_, &dst_frame_);
  VerifyFramesAreEqual(dst_frame_, golden_frame_);

  // Mono -> stereo.
  SetMonoFrame(&dst_frame_, 0);
  SetStereoFrame(&src_frame_, 10, 20);
  SetMonoFrame(&golden_frame_, 15);
  RemixAndResample(src_frame_, &resampler_, &dst_frame_);
  VerifyFramesAreEqual(golden_frame_, dst_frame_);
}

TEST_F(UtilityTest, RemixAndResampleSucceeds) {
  const int kSampleRates[] = {8000, 16000, 32000, 44100, 48000, 96000};
  const int kSampleRatesSize = sizeof(kSampleRates) / sizeof(*kSampleRates);
  const int kChannels[] = {1, 2};
  const int kChannelsSize = sizeof(kChannels) / sizeof(*kChannels);
  for (int src_rate = 0; src_rate < kSampleRatesSize; src_rate++) {
    for (int dst_rate = 0; dst_rate < kSampleRatesSize; dst_rate++) {
      for (int src_channel = 0; src_channel < kChannelsSize; src_channel++) {
        for (int dst_channel = 0; dst_channel < kChannelsSize; dst_channel++) {
          RunResampleTest(kChannels[src_channel], kSampleRates[src_rate],
                          kChannels[dst_channel], kSampleRates[dst_rate]);
        }
      }
    }
  }
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
