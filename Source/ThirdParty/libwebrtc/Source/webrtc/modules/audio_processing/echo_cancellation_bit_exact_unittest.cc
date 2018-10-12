/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/echo_cancellation_impl.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 100;

void SetupComponent(int sample_rate_hz,
                    EchoCancellationImpl::SuppressionLevel suppression_level,
                    bool drift_compensation_enabled,
                    EchoCancellationImpl* echo_canceller) {
  echo_canceller->Initialize(sample_rate_hz, 1, 1, 1);
  echo_canceller->Enable(true);
  echo_canceller->set_suppression_level(suppression_level);
  echo_canceller->enable_drift_compensation(drift_compensation_enabled);

  Config config;
  config.Set<DelayAgnostic>(new DelayAgnostic(true));
  config.Set<ExtendedFilter>(new ExtendedFilter(true));
  echo_canceller->SetExtraOptions(config);
}

void ProcessOneFrame(int sample_rate_hz,
                     int stream_delay_ms,
                     bool drift_compensation_enabled,
                     int stream_drift_samples,
                     AudioBuffer* render_audio_buffer,
                     AudioBuffer* capture_audio_buffer,
                     EchoCancellationImpl* echo_canceller) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    render_audio_buffer->SplitIntoFrequencyBands();
    capture_audio_buffer->SplitIntoFrequencyBands();
  }

  std::vector<float> render_audio;
  EchoCancellationImpl::PackRenderAudioBuffer(
      render_audio_buffer, 1, render_audio_buffer->num_channels(),
      &render_audio);
  echo_canceller->ProcessRenderAudio(render_audio);

  if (drift_compensation_enabled) {
    echo_canceller->set_stream_drift_samples(stream_drift_samples);
  }

  echo_canceller->ProcessCaptureAudio(capture_audio_buffer, stream_delay_ms);

  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    capture_audio_buffer->MergeFrequencyBands();
  }
}

void RunBitexactnessTest(
    int sample_rate_hz,
    size_t num_channels,
    int stream_delay_ms,
    bool drift_compensation_enabled,
    int stream_drift_samples,
    EchoCancellationImpl::SuppressionLevel suppression_level,
    bool stream_has_echo_reference,
    const rtc::ArrayView<const float>& output_reference) {
  rtc::CriticalSection crit_render;
  rtc::CriticalSection crit_capture;
  EchoCancellationImpl echo_canceller(&crit_render, &crit_capture);
  SetupComponent(sample_rate_hz, suppression_level, drift_compensation_enabled,
                 &echo_canceller);

  const int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig render_config(sample_rate_hz, num_channels, false);
  AudioBuffer render_buffer(
      render_config.num_frames(), render_config.num_channels(),
      render_config.num_frames(), 1, render_config.num_frames());
  test::InputAudioFile render_file(
      test::GetApmRenderTestVectorFileName(sample_rate_hz));
  std::vector<float> render_input(samples_per_channel * num_channels);

  const StreamConfig capture_config(sample_rate_hz, num_channels, false);
  AudioBuffer capture_buffer(
      capture_config.num_frames(), capture_config.num_channels(),
      capture_config.num_frames(), 1, capture_config.num_frames());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);

  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &render_file, render_input);
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(render_config, render_input, &render_buffer);
    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    ProcessOneFrame(sample_rate_hz, stream_delay_ms, drift_compensation_enabled,
                    stream_drift_samples, &render_buffer, &capture_buffer,
                    &echo_canceller);
  }

  // Extract and verify the test results.
  std::vector<float> capture_output;
  test::ExtractVectorFromAudioBuffer(capture_config, &capture_buffer,
                                     &capture_output);

  EXPECT_EQ(stream_has_echo_reference, echo_canceller.stream_has_echo());

  // Compare the output with the reference. Only the first values of the output
  // from last frame processed are compared in order not having to specify all
  // preceeding frames as testvectors. As the algorithm being tested has a
  // memory, testing only the last frame implicitly also tests the preceeding
  // frames.
  const float kElementErrorBound = 1.0f / 32768.0f;
  EXPECT_TRUE(test::VerifyDeinterleavedArray(
      capture_config.num_frames(), capture_config.num_channels(),
      output_reference, capture_output, kElementErrorBound));
}

const bool kStreamHasEchoReference = true;

}  // namespace

// TODO(peah): Activate all these tests for ARM and ARM64 once the issue on the
// Chromium ARM and ARM64 boths have been identified. This is tracked in the
// issue https://bugs.chromium.org/p/webrtc/issues/detail?id=5711.

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono8kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono8kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {-0.000646f, -0.001525f, 0.002688f};
  RunBitexactnessTest(8000, 1, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(16000, 1, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono32kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono32kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {-0.000671f, 0.000061f, -0.000031f};
  RunBitexactnessTest(32000, 1, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono48kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono48kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {-0.001403f, -0.001411f, -0.000755f};
  RunBitexactnessTest(48000, 1, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_LowLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_LowLevel_NoDrift_StreamDelay0) {
#endif
#if defined(WEBRTC_MAC)
  const float kOutputReference[] = {-0.000145f, 0.000179f, 0.000917f};
#else
  const float kOutputReference[] = {-0.000009f, 0.000363f, 0.001094f};
#endif
  RunBitexactnessTest(16000, 1, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kLowSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_ModerateLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_ModerateLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(
      16000, 1, 0, false, 0,
      EchoCancellationImpl::SuppressionLevel::kModerateSuppression,
      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay10) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_HighLevel_NoDrift_StreamDelay10) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(16000, 1, 10, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_NoDrift_StreamDelay20) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_HighLevel_NoDrift_StreamDelay20) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(16000, 1, 20, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_Drift0_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_HighLevel_Drift0_StreamDelay0) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(16000, 1, 0, true, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Mono16kHz_HighLevel_Drift5_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Mono16kHz_HighLevel_Drift5_StreamDelay0) {
#endif
  const float kOutputReference[] = {0.000055f, 0.000421f, 0.001149f};
  RunBitexactnessTest(16000, 1, 0, true, 5,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Stereo8kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Stereo8kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
#if defined(WEBRTC_MAC)
  const float kOutputReference[] = {-0.000392f, -0.001449f, 0.003004f,
                                    -0.000392f, -0.001449f, 0.003004f};
#else
  const float kOutputReference[] = {-0.000464f, -0.001525f, 0.002933f,
                                    -0.000464f, -0.001525f, 0.002933f};
#endif
  RunBitexactnessTest(8000, 2, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Stereo16kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Stereo16kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {0.000166f, 0.000735f, 0.000841f,
                                    0.000166f, 0.000735f, 0.000841f};
  RunBitexactnessTest(16000, 2, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Stereo32kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Stereo32kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
#if defined(WEBRTC_MAC)
  const float kOutputReference[] = {-0.000458f, 0.000244f, 0.000153f,
                                    -0.000458f, 0.000244f, 0.000153f};
#else
  const float kOutputReference[] = {-0.000427f, 0.000183f, 0.000183f,
                                    -0.000427f, 0.000183f, 0.000183f};
#endif
  RunBitexactnessTest(32000, 2, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

#if !(defined(WEBRTC_ARCH_ARM64) || defined(WEBRTC_ARCH_ARM) || \
      defined(WEBRTC_ANDROID))
TEST(EchoCancellationBitExactnessTest,
     Stereo48kHz_HighLevel_NoDrift_StreamDelay0) {
#else
TEST(EchoCancellationBitExactnessTest,
     DISABLED_Stereo48kHz_HighLevel_NoDrift_StreamDelay0) {
#endif
  const float kOutputReference[] = {-0.001101f, -0.001101f, -0.000449f,
                                    -0.001101f, -0.001101f, -0.000449f};
  RunBitexactnessTest(48000, 2, 0, false, 0,
                      EchoCancellationImpl::SuppressionLevel::kHighSuppression,
                      kStreamHasEchoReference, kOutputReference);
}

}  // namespace webrtc
