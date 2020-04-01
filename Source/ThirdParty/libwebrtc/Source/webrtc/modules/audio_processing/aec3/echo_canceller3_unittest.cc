/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_canceller3.h"

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/frame_blocker.h"
#include "modules/audio_processing/aec3/mock/mock_block_processor.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "modules/audio_processing/utility/cascaded_biquad_filter.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::StrictMock;

// Populates the frame with linearly increasing sample values for each band,
// with a band-specific offset, in order to allow simple bitexactness
// verification for each band.
void PopulateInputFrame(size_t frame_length,
                        size_t num_bands,
                        size_t frame_index,
                        float* const* frame,
                        int offset) {
  for (size_t k = 0; k < num_bands; ++k) {
    for (size_t i = 0; i < frame_length; ++i) {
      float value = static_cast<int>(frame_index * frame_length + i) + offset;
      frame[k][i] = (value > 0 ? 5000 * k + value : 0);
    }
  }
}

// Populates the frame with linearly increasing sample values.
void PopulateInputFrame(size_t frame_length,
                        size_t frame_index,
                        float* frame,
                        int offset) {
  for (size_t i = 0; i < frame_length; ++i) {
    float value = static_cast<int>(frame_index * frame_length + i) + offset;
    frame[i] = std::max(value, 0.f);
  }
}

// Verifies the that samples in the output frame are identical to the samples
// that were produced for the input frame, with an offset in order to compensate
// for buffering delays.
bool VerifyOutputFrameBitexactness(size_t frame_length,
                                   size_t num_bands,
                                   size_t frame_index,
                                   const float* const* frame,
                                   int offset) {
  float reference_frame_data[kMaxNumBands][2 * kSubFrameLength];
  float* reference_frame[kMaxNumBands];
  for (size_t k = 0; k < num_bands; ++k) {
    reference_frame[k] = &reference_frame_data[k][0];
  }

  PopulateInputFrame(frame_length, num_bands, frame_index, reference_frame,
                     offset);
  for (size_t k = 0; k < num_bands; ++k) {
    for (size_t i = 0; i < frame_length; ++i) {
      if (reference_frame[k][i] != frame[k][i]) {
        return false;
      }
    }
  }

  return true;
}

bool VerifyOutputFrameBitexactness(rtc::ArrayView<const float> reference,
                                   rtc::ArrayView<const float> frame,
                                   int offset) {
  for (size_t k = 0; k < frame.size(); ++k) {
    int reference_index = static_cast<int>(k) + offset;
    if (reference_index >= 0) {
      if (reference[reference_index] != frame[k]) {
        return false;
      }
    }
  }
  return true;
}

// Class for testing that the capture data is properly received by the block
// processor and that the processor data is properly passed to the
// EchoCanceller3 output.
class CaptureTransportVerificationProcessor : public BlockProcessor {
 public:
  explicit CaptureTransportVerificationProcessor(size_t num_bands) {}
  ~CaptureTransportVerificationProcessor() override = default;

  void ProcessCapture(
      bool level_change,
      bool saturated_microphone_signal,
      std::vector<std::vector<std::vector<float>>>* linear_output,
      std::vector<std::vector<std::vector<float>>>* capture_block) override {}

  void BufferRender(
      const std::vector<std::vector<std::vector<float>>>& block) override {}

  void UpdateEchoLeakageStatus(bool leakage_detected) override {}

  void GetMetrics(EchoControl::Metrics* metrics) const override {}

  void SetAudioBufferDelay(int delay_ms) override {}

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(CaptureTransportVerificationProcessor);
};

// Class for testing that the render data is properly received by the block
// processor.
class RenderTransportVerificationProcessor : public BlockProcessor {
 public:
  explicit RenderTransportVerificationProcessor(size_t num_bands) {}
  ~RenderTransportVerificationProcessor() override = default;

  void ProcessCapture(
      bool level_change,
      bool saturated_microphone_signal,
      std::vector<std::vector<std::vector<float>>>* linear_output,
      std::vector<std::vector<std::vector<float>>>* capture_block) override {
    std::vector<std::vector<std::vector<float>>> render_block =
        received_render_blocks_.front();
    received_render_blocks_.pop_front();
    capture_block->swap(render_block);
  }

  void BufferRender(
      const std::vector<std::vector<std::vector<float>>>& block) override {
    received_render_blocks_.push_back(block);
  }

  void UpdateEchoLeakageStatus(bool leakage_detected) override {}

  void GetMetrics(EchoControl::Metrics* metrics) const override {}

  void SetAudioBufferDelay(int delay_ms) override {}

 private:
  std::deque<std::vector<std::vector<std::vector<float>>>>
      received_render_blocks_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderTransportVerificationProcessor);
};

class EchoCanceller3Tester {
 public:
  explicit EchoCanceller3Tester(int sample_rate_hz)
      : sample_rate_hz_(sample_rate_hz),
        num_bands_(NumBandsForRate(sample_rate_hz_)),
        frame_length_(160),
        fullband_frame_length_(rtc::CheckedDivExact(sample_rate_hz_, 100)),
        capture_buffer_(fullband_frame_length_ * 100,
                        1,
                        fullband_frame_length_ * 100,
                        1,
                        fullband_frame_length_ * 100,
                        1),
        render_buffer_(fullband_frame_length_ * 100,
                       1,
                       fullband_frame_length_ * 100,
                       1,
                       fullband_frame_length_ * 100,
                       1) {}

  // Verifies that the capture data is properly received by the block processor
  // and that the processor data is properly passed to the EchoCanceller3
  // output.
  void RunCaptureTransportVerificationTest() {
    EchoCanceller3 aec3(
        EchoCanceller3Config(), sample_rate_hz_, 1, 1,
        std::unique_ptr<BlockProcessor>(
            new CaptureTransportVerificationProcessor(num_bands_)));

    for (size_t frame_index = 0; frame_index < kNumFramesToProcess;
         ++frame_index) {
      aec3.AnalyzeCapture(&capture_buffer_);
      OptionalBandSplit();
      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 0);
      PopulateInputFrame(frame_length_, frame_index,
                         &render_buffer_.channels()[0][0], 0);

      aec3.AnalyzeRender(&render_buffer_);
      aec3.ProcessCapture(&capture_buffer_, false);
      EXPECT_TRUE(VerifyOutputFrameBitexactness(
          frame_length_, num_bands_, frame_index,
          &capture_buffer_.split_bands(0)[0], -64));
    }
  }

  // Test method for testing that the render data is properly received by the
  // block processor.
  void RunRenderTransportVerificationTest() {
    EchoCanceller3 aec3(
        EchoCanceller3Config(), sample_rate_hz_, 1, 1,
        std::unique_ptr<BlockProcessor>(
            new RenderTransportVerificationProcessor(num_bands_)));

    std::vector<std::vector<float>> render_input(1);
    std::vector<float> capture_output;
    for (size_t frame_index = 0; frame_index < kNumFramesToProcess;
         ++frame_index) {
      aec3.AnalyzeCapture(&capture_buffer_);
      OptionalBandSplit();
      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 100);
      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &render_buffer_.split_bands(0)[0], 0);

      for (size_t k = 0; k < frame_length_; ++k) {
        render_input[0].push_back(render_buffer_.split_bands(0)[0][k]);
      }
      aec3.AnalyzeRender(&render_buffer_);
      aec3.ProcessCapture(&capture_buffer_, false);
      for (size_t k = 0; k < frame_length_; ++k) {
        capture_output.push_back(capture_buffer_.split_bands(0)[0][k]);
      }
    }
    HighPassFilter hp_filter(16000, 1);
    hp_filter.Process(&render_input);

    EXPECT_TRUE(
        VerifyOutputFrameBitexactness(render_input[0], capture_output, -64));
  }

  // Verifies that information about echo path changes are properly propagated
  // to the block processor.
  // The cases tested are:
  // -That no set echo path change flags are received when there is no echo path
  // change.
  // -That set echo path change flags are received and continues to be received
  // as long as echo path changes are flagged.
  // -That set echo path change flags are no longer received when echo path
  // change events stop being flagged.
  enum class EchoPathChangeTestVariant { kNone, kOneSticky, kOneNonSticky };

  void RunEchoPathChangeVerificationTest(
      EchoPathChangeTestVariant echo_path_change_test_variant) {
    constexpr size_t kNumFullBlocksPerFrame = 160 / kBlockSize;
    constexpr size_t kExpectedNumBlocksToProcess =
        (kNumFramesToProcess * 160) / kBlockSize;
    std::unique_ptr<testing::StrictMock<webrtc::test::MockBlockProcessor>>
        block_processor_mock(
            new StrictMock<webrtc::test::MockBlockProcessor>());
    EXPECT_CALL(*block_processor_mock, BufferRender(_))
        .Times(kExpectedNumBlocksToProcess);
    EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(_)).Times(0);

    switch (echo_path_change_test_variant) {
      case EchoPathChangeTestVariant::kNone:
        EXPECT_CALL(*block_processor_mock, ProcessCapture(false, _, _, _))
            .Times(kExpectedNumBlocksToProcess);
        break;
      case EchoPathChangeTestVariant::kOneSticky:
        EXPECT_CALL(*block_processor_mock, ProcessCapture(true, _, _, _))
            .Times(kExpectedNumBlocksToProcess);
        break;
      case EchoPathChangeTestVariant::kOneNonSticky:
        EXPECT_CALL(*block_processor_mock, ProcessCapture(true, _, _, _))
            .Times(kNumFullBlocksPerFrame);
        EXPECT_CALL(*block_processor_mock, ProcessCapture(false, _, _, _))
            .Times(kExpectedNumBlocksToProcess - kNumFullBlocksPerFrame);
        break;
    }

    EchoCanceller3 aec3(EchoCanceller3Config(), sample_rate_hz_, 1, 1,
                        std::move(block_processor_mock));

    for (size_t frame_index = 0; frame_index < kNumFramesToProcess;
         ++frame_index) {
      bool echo_path_change = false;
      switch (echo_path_change_test_variant) {
        case EchoPathChangeTestVariant::kNone:
          break;
        case EchoPathChangeTestVariant::kOneSticky:
          echo_path_change = true;
          break;
        case EchoPathChangeTestVariant::kOneNonSticky:
          if (frame_index == 0) {
            echo_path_change = true;
          }
          break;
      }

      aec3.AnalyzeCapture(&capture_buffer_);
      OptionalBandSplit();

      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 0);
      PopulateInputFrame(frame_length_, frame_index,
                         &render_buffer_.channels()[0][0], 0);

      aec3.AnalyzeRender(&render_buffer_);
      aec3.ProcessCapture(&capture_buffer_, echo_path_change);
    }
  }

  // Test for verifying that echo leakage information is being properly passed
  // to the processor.
  // The cases tested are:
  // -That no method calls are received when they should not.
  // -That false values are received each time they are flagged.
  // -That true values are received each time they are flagged.
  // -That a false value is received when flagged after a true value has been
  // flagged.
  enum class EchoLeakageTestVariant {
    kNone,
    kFalseSticky,
    kTrueSticky,
    kTrueNonSticky
  };

  void RunEchoLeakageVerificationTest(
      EchoLeakageTestVariant leakage_report_variant) {
    constexpr size_t kExpectedNumBlocksToProcess =
        (kNumFramesToProcess * 160) / kBlockSize;
    std::unique_ptr<testing::StrictMock<webrtc::test::MockBlockProcessor>>
        block_processor_mock(
            new StrictMock<webrtc::test::MockBlockProcessor>());
    EXPECT_CALL(*block_processor_mock, BufferRender(_))
        .Times(kExpectedNumBlocksToProcess);
    EXPECT_CALL(*block_processor_mock, ProcessCapture(_, _, _, _))
        .Times(kExpectedNumBlocksToProcess);

    switch (leakage_report_variant) {
      case EchoLeakageTestVariant::kNone:
        EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(_)).Times(0);
        break;
      case EchoLeakageTestVariant::kFalseSticky:
        EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(false))
            .Times(1);
        break;
      case EchoLeakageTestVariant::kTrueSticky:
        EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(true))
            .Times(1);
        break;
      case EchoLeakageTestVariant::kTrueNonSticky: {
        ::testing::InSequence s;
        EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(true))
            .Times(1);
        EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(false))
            .Times(kNumFramesToProcess - 1);
      } break;
    }

    EchoCanceller3 aec3(EchoCanceller3Config(), sample_rate_hz_, 1, 1,
                        std::move(block_processor_mock));

    for (size_t frame_index = 0; frame_index < kNumFramesToProcess;
         ++frame_index) {
      switch (leakage_report_variant) {
        case EchoLeakageTestVariant::kNone:
          break;
        case EchoLeakageTestVariant::kFalseSticky:
          if (frame_index == 0) {
            aec3.UpdateEchoLeakageStatus(false);
          }
          break;
        case EchoLeakageTestVariant::kTrueSticky:
          if (frame_index == 0) {
            aec3.UpdateEchoLeakageStatus(true);
          }
          break;
        case EchoLeakageTestVariant::kTrueNonSticky:
          if (frame_index == 0) {
            aec3.UpdateEchoLeakageStatus(true);
          } else {
            aec3.UpdateEchoLeakageStatus(false);
          }
          break;
      }

      aec3.AnalyzeCapture(&capture_buffer_);
      OptionalBandSplit();

      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 0);
      PopulateInputFrame(frame_length_, frame_index,
                         &render_buffer_.channels()[0][0], 0);

      aec3.AnalyzeRender(&render_buffer_);
      aec3.ProcessCapture(&capture_buffer_, false);
    }
  }

  // This verifies that saturation information is properly passed to the
  // BlockProcessor.
  // The cases tested are:
  // -That no saturation event is passed to the processor if there is no
  // saturation.
  // -That one frame with one negative saturated sample value is reported to be
  // saturated and that following non-saturated frames are properly reported as
  // not being saturated.
  // -That one frame with one positive saturated sample value is reported to be
  // saturated and that following non-saturated frames are properly reported as
  // not being saturated.
  enum class SaturationTestVariant { kNone, kOneNegative, kOnePositive };

  void RunCaptureSaturationVerificationTest(
      SaturationTestVariant saturation_variant) {
    const size_t kNumFullBlocksPerFrame = 160 / kBlockSize;
    const size_t kExpectedNumBlocksToProcess =
        (kNumFramesToProcess * 160) / kBlockSize;
    std::unique_ptr<testing::StrictMock<webrtc::test::MockBlockProcessor>>
        block_processor_mock(
            new StrictMock<webrtc::test::MockBlockProcessor>());
    EXPECT_CALL(*block_processor_mock, BufferRender(_))
        .Times(kExpectedNumBlocksToProcess);
    EXPECT_CALL(*block_processor_mock, UpdateEchoLeakageStatus(_)).Times(0);

    switch (saturation_variant) {
      case SaturationTestVariant::kNone:
        EXPECT_CALL(*block_processor_mock, ProcessCapture(_, false, _, _))
            .Times(kExpectedNumBlocksToProcess);
        break;
      case SaturationTestVariant::kOneNegative: {
        ::testing::InSequence s;
        EXPECT_CALL(*block_processor_mock, ProcessCapture(_, true, _, _))
            .Times(kNumFullBlocksPerFrame);
        EXPECT_CALL(*block_processor_mock, ProcessCapture(_, false, _, _))
            .Times(kExpectedNumBlocksToProcess - kNumFullBlocksPerFrame);
      } break;
      case SaturationTestVariant::kOnePositive: {
        ::testing::InSequence s;
        EXPECT_CALL(*block_processor_mock, ProcessCapture(_, true, _, _))
            .Times(kNumFullBlocksPerFrame);
        EXPECT_CALL(*block_processor_mock, ProcessCapture(_, false, _, _))
            .Times(kExpectedNumBlocksToProcess - kNumFullBlocksPerFrame);
      } break;
    }

    EchoCanceller3 aec3(EchoCanceller3Config(), sample_rate_hz_, 1, 1,
                        std::move(block_processor_mock));
    for (size_t frame_index = 0; frame_index < kNumFramesToProcess;
         ++frame_index) {
      for (int k = 0; k < fullband_frame_length_; ++k) {
        capture_buffer_.channels()[0][k] = 0.f;
      }
      switch (saturation_variant) {
        case SaturationTestVariant::kNone:
          break;
        case SaturationTestVariant::kOneNegative:
          if (frame_index == 0) {
            capture_buffer_.channels()[0][10] = -32768.f;
          }
          break;
        case SaturationTestVariant::kOnePositive:
          if (frame_index == 0) {
            capture_buffer_.channels()[0][10] = 32767.f;
          }
          break;
      }

      aec3.AnalyzeCapture(&capture_buffer_);
      OptionalBandSplit();

      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 0);
      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &render_buffer_.split_bands(0)[0], 0);

      aec3.AnalyzeRender(&render_buffer_);
      aec3.ProcessCapture(&capture_buffer_, false);
    }
  }

  // This test verifies that the swapqueue is able to handle jitter in the
  // capture and render API calls.
  void RunRenderSwapQueueVerificationTest() {
    const EchoCanceller3Config config;
    EchoCanceller3 aec3(
        config, sample_rate_hz_, 1, 1,
        std::unique_ptr<BlockProcessor>(
            new RenderTransportVerificationProcessor(num_bands_)));

    std::vector<std::vector<float>> render_input(1);
    std::vector<float> capture_output;

    for (size_t frame_index = 0; frame_index < kRenderTransferQueueSizeFrames;
         ++frame_index) {
      if (sample_rate_hz_ > 16000) {
        render_buffer_.SplitIntoFrequencyBands();
      }
      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &render_buffer_.split_bands(0)[0], 0);

      if (sample_rate_hz_ > 16000) {
        render_buffer_.SplitIntoFrequencyBands();
      }

      for (size_t k = 0; k < frame_length_; ++k) {
        render_input[0].push_back(render_buffer_.split_bands(0)[0][k]);
      }
      aec3.AnalyzeRender(&render_buffer_);
    }

    for (size_t frame_index = 0; frame_index < kRenderTransferQueueSizeFrames;
         ++frame_index) {
      aec3.AnalyzeCapture(&capture_buffer_);
      if (sample_rate_hz_ > 16000) {
        capture_buffer_.SplitIntoFrequencyBands();
      }

      PopulateInputFrame(frame_length_, num_bands_, frame_index,
                         &capture_buffer_.split_bands(0)[0], 0);

      aec3.ProcessCapture(&capture_buffer_, false);
      for (size_t k = 0; k < frame_length_; ++k) {
        capture_output.push_back(capture_buffer_.split_bands(0)[0][k]);
      }
    }
    HighPassFilter hp_filter(16000, 1);
    hp_filter.Process(&render_input);

    EXPECT_TRUE(
        VerifyOutputFrameBitexactness(render_input[0], capture_output, -64));
  }

  // This test verifies that a buffer overrun in the render swapqueue is
  // properly reported.
  void RunRenderPipelineSwapQueueOverrunReturnValueTest() {
    EchoCanceller3 aec3(EchoCanceller3Config(), sample_rate_hz_, 1, 1);

    constexpr size_t kRenderTransferQueueSize = 30;
    for (size_t k = 0; k < 2; ++k) {
      for (size_t frame_index = 0; frame_index < kRenderTransferQueueSize;
           ++frame_index) {
        if (sample_rate_hz_ > 16000) {
          render_buffer_.SplitIntoFrequencyBands();
        }
        PopulateInputFrame(frame_length_, frame_index,
                           &render_buffer_.channels()[0][0], 0);

        aec3.AnalyzeRender(&render_buffer_);
      }
    }
  }

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
  // Verifies the that the check for the number of bands in the AnalyzeRender
  // input is correct by adjusting the sample rates of EchoCanceller3 and the
  // input AudioBuffer to have a different number of bands.
  void RunAnalyzeRenderNumBandsCheckVerification() {
    // Set aec3_sample_rate_hz to be different from sample_rate_hz_ in such a
    // way that the number of bands for the rates are different.
    const int aec3_sample_rate_hz = sample_rate_hz_ == 48000 ? 32000 : 48000;
    EchoCanceller3 aec3(EchoCanceller3Config(), aec3_sample_rate_hz, 1, 1);
    PopulateInputFrame(frame_length_, 0, &render_buffer_.channels_f()[0][0], 0);

    EXPECT_DEATH(aec3.AnalyzeRender(&render_buffer_), "");
  }

  // Verifies the that the check for the number of bands in the ProcessCapture
  // input is correct by adjusting the sample rates of EchoCanceller3 and the
  // input AudioBuffer to have a different number of bands.
  void RunProcessCaptureNumBandsCheckVerification() {
    // Set aec3_sample_rate_hz to be different from sample_rate_hz_ in such a
    // way that the number of bands for the rates are different.
    const int aec3_sample_rate_hz = sample_rate_hz_ == 48000 ? 32000 : 48000;
    EchoCanceller3 aec3(EchoCanceller3Config(), aec3_sample_rate_hz, 1, 1);
    PopulateInputFrame(frame_length_, num_bands_, 0,
                       &capture_buffer_.split_bands_f(0)[0], 100);
    EXPECT_DEATH(aec3.ProcessCapture(&capture_buffer_, false), "");
  }

#endif

 private:
  void OptionalBandSplit() {
    if (sample_rate_hz_ > 16000) {
      capture_buffer_.SplitIntoFrequencyBands();
      render_buffer_.SplitIntoFrequencyBands();
    }
  }

  static constexpr size_t kNumFramesToProcess = 20;
  const int sample_rate_hz_;
  const size_t num_bands_;
  const size_t frame_length_;
  const int fullband_frame_length_;
  AudioBuffer capture_buffer_;
  AudioBuffer render_buffer_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoCanceller3Tester);
};

std::string ProduceDebugText(int sample_rate_hz) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.Release();
}

std::string ProduceDebugText(int sample_rate_hz, int variant) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz << ", variant: " << variant;
  return ss.Release();
}

}  // namespace

TEST(EchoCanceller3Buffering, CaptureBitexactness) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Tester(rate).RunCaptureTransportVerificationTest();
  }
}

TEST(EchoCanceller3Buffering, RenderBitexactness) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Tester(rate).RunRenderTransportVerificationTest();
  }
}

TEST(EchoCanceller3Buffering, RenderSwapQueue) {
  EchoCanceller3Tester(16000).RunRenderSwapQueueVerificationTest();
}

TEST(EchoCanceller3Buffering, RenderSwapQueueOverrunReturnValue) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Tester(rate)
        .RunRenderPipelineSwapQueueOverrunReturnValueTest();
  }
}

TEST(EchoCanceller3Messaging, CaptureSaturation) {
  auto variants = {EchoCanceller3Tester::SaturationTestVariant::kNone,
                   EchoCanceller3Tester::SaturationTestVariant::kOneNegative,
                   EchoCanceller3Tester::SaturationTestVariant::kOnePositive};
  for (auto rate : {16000, 32000, 48000}) {
    for (auto variant : variants) {
      SCOPED_TRACE(ProduceDebugText(rate, static_cast<int>(variant)));
      EchoCanceller3Tester(rate).RunCaptureSaturationVerificationTest(variant);
    }
  }
}

TEST(EchoCanceller3Messaging, EchoPathChange) {
  auto variants = {
      EchoCanceller3Tester::EchoPathChangeTestVariant::kNone,
      EchoCanceller3Tester::EchoPathChangeTestVariant::kOneSticky,
      EchoCanceller3Tester::EchoPathChangeTestVariant::kOneNonSticky};
  for (auto rate : {16000, 32000, 48000}) {
    for (auto variant : variants) {
      SCOPED_TRACE(ProduceDebugText(rate, static_cast<int>(variant)));
      EchoCanceller3Tester(rate).RunEchoPathChangeVerificationTest(variant);
    }
  }
}

TEST(EchoCanceller3Messaging, EchoLeakage) {
  auto variants = {
      EchoCanceller3Tester::EchoLeakageTestVariant::kNone,
      EchoCanceller3Tester::EchoLeakageTestVariant::kFalseSticky,
      EchoCanceller3Tester::EchoLeakageTestVariant::kTrueSticky,
      EchoCanceller3Tester::EchoLeakageTestVariant::kTrueNonSticky};
  for (auto rate : {16000, 32000, 48000}) {
    for (auto variant : variants) {
      SCOPED_TRACE(ProduceDebugText(rate, static_cast<int>(variant)));
      EchoCanceller3Tester(rate).RunEchoLeakageVerificationTest(variant);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(EchoCanceller3InputCheck, WrongCaptureNumBandsCheckVerification) {
  for (auto rate : {16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    EchoCanceller3Tester(rate).RunProcessCaptureNumBandsCheckVerification();
  }
}

// Verifiers that the verification for null input to the capture processing api
// call works.
TEST(EchoCanceller3InputCheck, NullCaptureProcessingParameter) {
  EXPECT_DEATH(EchoCanceller3(EchoCanceller3Config(), 16000, 1, 1)
                   .ProcessCapture(nullptr, false),
               "");
}

// Verifies the check for correct sample rate.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(EchoCanceller3InputCheck, DISABLED_WrongSampleRate) {
  ApmDataDumper data_dumper(0);
  EXPECT_DEATH(EchoCanceller3(EchoCanceller3Config(), 8001, 1, 1), "");
}

#endif

}  // namespace webrtc
