/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator_impl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block.h"
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#else
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#endif

namespace webrtc {
namespace {
using testing::FloatEq;
using testing::Not;

using ModelInputEnum = FeatureExtractor::ModelInputEnum;
using ModelOutputEnum = FeatureExtractor::ModelOutputEnum;

struct ModelConstants {
  explicit ModelConstants(int frame_size)
      : frame_size(frame_size),
        step_size(frame_size / 2),
        frame_size_by_2_plus_1(frame_size / 2 + 1) {}

  int frame_size;
  int step_size;
  int frame_size_by_2_plus_1;
};

// Mocks the TF Lite interaction to simplify testing the behavior of
// preprocessing, postprocessing, and AEC3-related rebuffering.
class MockModelRunner : public NeuralResidualEchoEstimatorImpl::ModelRunner {
 public:
  explicit MockModelRunner(const ModelConstants& model_constants)
      : constants_(model_constants),
        metadata_([]() {
          audioproc::ReeModelMetadata metadata;
          metadata.set_version(1);
          return metadata;
        }()),
        input_mic_(constants_.frame_size),
        input_linear_aec_output_(constants_.frame_size),
        input_aec_ref_(constants_.frame_size),
        output_echo_mask_(constants_.frame_size_by_2_plus_1) {}

  ~MockModelRunner() override {}

  int StepSize() const override { return constants_.step_size; }

  ArrayView<float> GetInput(ModelInputEnum input_enum) override {
    switch (input_enum) {
      case ModelInputEnum::kMic:
        return webrtc::ArrayView<float>(input_mic_.data(),
                                        constants_.frame_size);
      case ModelInputEnum::kLinearAecOutput:
        return webrtc::ArrayView<float>(input_linear_aec_output_.data(),
                                        constants_.frame_size);
      case ModelInputEnum::kAecRef:
        return webrtc::ArrayView<float>(input_aec_ref_.data(),
                                        constants_.frame_size);
      case ModelInputEnum::kModelState:
      case ModelInputEnum::kNumInputs:
        RTC_CHECK(false);
        return webrtc::ArrayView<float>();
    }
  }

  ArrayView<const float> GetOutput(ModelOutputEnum output_enum) override {
    switch (output_enum) {
      case ModelOutputEnum::kEchoMask:
      case ModelOutputEnum::kUnboundedEchoMask:
        return ArrayView<const float>(output_echo_mask_.data(),
                                      constants_.frame_size_by_2_plus_1);
      case ModelOutputEnum::kModelState:
        // Mock model state output if needed, for now return empty
        return ArrayView<const float>();
      default:
        RTC_CHECK(false);
        return ArrayView<float>();
    }
  }

  const audioproc::ReeModelMetadata& GetMetadata() const override {
    return metadata_;
  }

  MOCK_METHOD(bool, Invoke, (), (override));

  MOCK_METHOD(void, Reset, (), (override));

  const ModelConstants constants_;
  const audioproc::ReeModelMetadata metadata_;
  std::vector<float> input_mic_;
  std::vector<float> input_linear_aec_output_;
  std::vector<float> input_aec_ref_;
  std::vector<float> output_echo_mask_;
};

class NeuralResidualEchoEstimatorImplTest
    : public ::testing::TestWithParam<ModelConstants> {};
INSTANTIATE_TEST_SUITE_P(
    VariableModelFrameLength,
    NeuralResidualEchoEstimatorImplTest,
    ::testing::Values(ModelConstants(/*frame_size=*/2 * kBlockSize),
                      ModelConstants(/*frame_size=*/4 * kBlockSize),
                      ModelConstants(/*frame_size=*/8 * kBlockSize)));

TEST_P(NeuralResidualEchoEstimatorImplTest,
       InputBlocksAreComposedIntoOverlappingFrames) {
  const ModelConstants model_constants = GetParam();
  SCOPED_TRACE(testing::Message()
               << "model_constants.frame_size=" << model_constants.frame_size);

  constexpr int kNumCaptureChannels = 1;
  std::array<float, kBlockSize> x;
  std::vector<std::array<float, kBlockSize>> y(kNumCaptureChannels);
  std::vector<std::array<float, kBlockSize>> e(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded(
      kNumCaptureChannels);

  auto mock_model_runner = std::make_unique<MockModelRunner>(model_constants);
  for (int i = 0; i < model_constants.frame_size; ++i) {
    // The odd numbers are different primes, to uniquely identify each buffer.
    mock_model_runner->input_mic_[i] = i + 2311;
    mock_model_runner->input_linear_aec_output_[i] = i + 2333;
    mock_model_runner->input_aec_ref_[i] = i + 2339;
  }
  auto* mock_model_runner_ptr = mock_model_runner.get();
  NeuralResidualEchoEstimatorImpl estimator(std::move(mock_model_runner));

  EXPECT_CALL(*mock_model_runner_ptr, Invoke())
      .Times(1)
      .WillOnce(testing::Return(true));

  const int num_blocks_to_process = model_constants.step_size / kBlockSize;
  for (int block_counter = 0; block_counter < num_blocks_to_process;
       ++block_counter) {
    // The odd numbers are different primes, to uniquely identify each buffer.
    for (size_t j = 0; j < kBlockSize; ++j) {
      x[j] = block_counter * kBlockSize + j + 11;
      y[0][j] = block_counter * kBlockSize + j + 13;
      e[0][j] = block_counter * kBlockSize + j + 17;
    }
    for (size_t j = 0; j < kFftLengthBy2Plus1; ++j) {
      E2[0][j] = block_counter * kFftLengthBy2Plus1 + j + 23;
      S2[0][j] = block_counter * kFftLengthBy2Plus1 + j + 29;
      Y2[0][j] = block_counter * kFftLengthBy2Plus1 + j + 31;
    }
    Block render_block(1, 1);
    for (size_t j = 0; j < kBlockSize; ++j) {
      render_block.View(/*band=*/0, /*ch=*/0)[j] = x[j];
    }
    estimator.Estimate(render_block, y, e, S2, Y2, E2,
                       /*dominant_nearend=*/false, R2, R2_unbounded);
  }

  // Check that old buffer content is shifted down properly.
  for (int i = 0; i < model_constants.frame_size - model_constants.step_size;
       ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    EXPECT_FLOAT_EQ(mock_model_runner_ptr->input_linear_aec_output_[i],
                    model_constants.step_size + i + 2333);
    EXPECT_FLOAT_EQ(mock_model_runner_ptr->input_aec_ref_[i],
                    model_constants.step_size + i + 2339);
  }
  // Check that new buffer content matches the input data. This time with
  // scaling as the scaling is applied when new data is buffered.
  for (int i = model_constants.frame_size - model_constants.step_size;
       i < model_constants.frame_size; ++i) {
    constexpr float kScaling = 1.0f / 32768;
    SCOPED_TRACE(testing::Message() << "i=" << i);
    int input_index =
        i - (model_constants.frame_size - model_constants.step_size);
    EXPECT_FLOAT_EQ(mock_model_runner_ptr->input_linear_aec_output_[i],
                    kScaling * (input_index + 17));
    EXPECT_FLOAT_EQ(mock_model_runner_ptr->input_aec_ref_[i],
                    kScaling * (input_index + 11));
  }
}

TEST_P(NeuralResidualEchoEstimatorImplTest, OutputMaskIsApplied) {
  const ModelConstants model_constants = GetParam();
  SCOPED_TRACE(testing::Message()
               << "model_constants.frame_size=" << model_constants.frame_size);

  constexpr int kNumCaptureChannels = 1;
  std::array<float, kBlockSize> x;
  std::vector<std::array<float, kBlockSize>> y(kNumCaptureChannels);
  std::vector<std::array<float, kBlockSize>> e(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2{kNumCaptureChannels};
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2{kNumCaptureChannels};
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2{kNumCaptureChannels};
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded(
      kNumCaptureChannels);
  std::fill(x.begin(), x.end(), 10000);
  std::fill(y[0].begin(), y[0].end(), 10000);
  std::fill(e[0].begin(), e[0].end(), 10000);
  std::fill(E2[0].begin(), E2[0].end(), 10000);
  std::fill(S2[0].begin(), S2[0].end(), 10000);
  std::fill(Y2[0].begin(), Y2[0].end(), 10000);

  auto mock_model_runner = std::make_unique<MockModelRunner>(model_constants);

  // Mock the output echo mask to be a ramp from 0.1 at DC to 1.0 at the highest
  // frequency bin.
  const int blocks_per_model_step = model_constants.step_size / kBlockSize;
  mock_model_runner->output_echo_mask_[0] = 0.1;
  for (size_t i = 1; i < kFftLengthBy2Plus1; ++i) {
    for (int j = 1; j <= blocks_per_model_step; ++j) {
      mock_model_runner
          ->output_echo_mask_[(i - 1) * blocks_per_model_step + j] =
          0.1 + 0.9 * i / model_constants.step_size;
    }
  }
  auto* mock_model_runner_ptr = mock_model_runner.get();
  NeuralResidualEchoEstimatorImpl estimator(std::move(mock_model_runner));

  EXPECT_CALL(*mock_model_runner_ptr, Invoke())
      .Times(1)
      .WillOnce(testing::Return(true));

  for (int b = 0; b < blocks_per_model_step; ++b) {
    Block render_block(1, 1);
    for (size_t j = 0; j < kBlockSize; ++j) {
      render_block.View(/*band=*/0, /*ch=*/0)[j] = x[j];
    }
    estimator.Estimate(render_block, y, e, S2, Y2, E2,
                       /*dominant_nearend=*/false, R2, R2_unbounded);
  }

  // Check that the mocked output mask is applied.
  for (size_t i = 0; i < kFftLengthBy2Plus1; ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    const float mask = (0.1 + 0.9 * i / model_constants.step_size);
    const float power_adjusted_mask = 1 - (1 - mask) * (1 - mask);
    EXPECT_FLOAT_EQ(R2[0][i], 10000 * power_adjusted_mask);
    EXPECT_FLOAT_EQ(R2_unbounded[0][i], R2[0][i]);
  }
}

TEST_P(NeuralResidualEchoEstimatorImplTest, ResetsState) {
  const ModelConstants model_constants = GetParam();
  if (model_constants.step_size == kBlockSize) {
    // This reset test needs to have a step size larger than the block size.
    return;
  }
  SCOPED_TRACE(testing::Message()
               << "model_constants.frame_size=" << model_constants.frame_size);

  constexpr int kNumCaptureChannels = 1;
  std::vector<std::array<float, kBlockSize>> y(kNumCaptureChannels);
  std::vector<std::array<float, kBlockSize>> e(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded(
      kNumCaptureChannels);

  auto mock_model_runner = std::make_unique<MockModelRunner>(model_constants);
  auto* mock_model_runner_ptr = mock_model_runner.get();
  NeuralResidualEchoEstimatorImpl estimator(std::move(mock_model_runner));

  EXPECT_CALL(*mock_model_runner_ptr, Invoke()).Times(1);
  EXPECT_CALL(*mock_model_runner_ptr, Reset()).Times(1);

  const int num_blocks_to_process = model_constants.step_size / kBlockSize;
  for (int frame = 0; frame < 2; ++frame) {
    for (int block = 0; block < num_blocks_to_process; ++block) {
      Block render_block(1, 1);
      estimator.Estimate(render_block, y, e, S2, Y2, E2,
                         /*dominant_nearend=*/false, R2, R2_unbounded);
      if (frame == 1 && block == 0) {
        // Resetting after the first block clears the estimator internal
        // buffers. This prevents the model from receiving a full input frame,
        // so Invoke() will not be called again within this second frame.
        estimator.Reset();
      }
    }
  }
}

TEST(NeuralResidualEchoEstimatorWithRealModelTest,
     RunEstimationWithRealTfLiteModel) {
  std::string model_path = test::ResourcePath(
      "audio_processing/aec3/noop_ml_aec_model_for_testing", "tflite");
  tflite::ops::builtin::BuiltinOpResolver op_resolver;
  std::unique_ptr<tflite::FlatBufferModel> model =
      tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
  std::unique_ptr<NeuralResidualEchoEstimatorImpl::ModelRunner>
      tflite_model_runner = NeuralResidualEchoEstimatorImpl::LoadTfLiteModel(
          model.get(), op_resolver);
  ASSERT_TRUE(tflite_model_runner != nullptr);

  const audioproc::ReeModelMetadata metadata =
      tflite_model_runner->GetMetadata();
  ASSERT_EQ(metadata.version(), 2);

  NeuralResidualEchoEstimatorImpl estimator(std::move(tflite_model_runner));

  constexpr int kNumCaptureChannels = 2;
  std::array<float, kBlockSize> x;
  std::vector<std::array<float, kBlockSize>> y{kNumCaptureChannels};
  std::vector<std::array<float, kBlockSize>> e{kNumCaptureChannels};
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded{
      kNumCaptureChannels};
  Random random_generator(4635U);
  constexpr int kNumBlocksToProcess = 1000;
  for (int b = 0; b < kNumBlocksToProcess; ++b) {
    constexpr float kAmplitude = 0.1f;
    RandomizeSampleVector(&random_generator, x, kAmplitude);
    for (int ch = 0; ch < kNumCaptureChannels; ++ch) {
      RandomizeSampleVector(&random_generator, y[ch], kAmplitude);
      RandomizeSampleVector(&random_generator, e[ch], kAmplitude);
      RandomizeSampleVector(&random_generator, E2[ch], kAmplitude);
      RandomizeSampleVector(&random_generator, S2[ch], kAmplitude);
      RandomizeSampleVector(&random_generator, Y2[ch], kAmplitude);
      std::fill(R2[ch].begin(), R2[ch].end(), 1234.0f);
      std::fill(R2_unbounded[ch].begin(), R2_unbounded[ch].end(), 1234.0f);
    }
    Block render_block(1, 1);
    for (size_t j = 0; j < kBlockSize; ++j) {
      render_block.View(/*band=*/0, /*ch=*/0)[j] = x[j];
    }
    estimator.Estimate(render_block, y, e, S2, Y2, E2,
                       /*dominant_nearend=*/false, R2, R2_unbounded);

    // Check that the output is populated.
    for (int ch = 0; ch < kNumCaptureChannels; ++ch) {
      for (size_t i = 0; i < kFftLengthBy2Plus1; ++i) {
        SCOPED_TRACE(testing::Message() << "block b=" << b << ", channel ch="
                                        << ch << ", index i=" << i);
        EXPECT_THAT(R2[ch][i], Not(FloatEq(1234.0)));
        EXPECT_THAT(R2_unbounded[ch][i], Not(FloatEq(1234.0)));
      }
    }
  }
}

// Verifies that LoadTfLiteModel returns nullptr if the model's metadata version
// is unsupported. This is done by loading a test model with a valid version,
// modifying the version in the metadata to an unsupported value, and then
// checking that the model fails to load.
TEST(NeuralResidualEchoEstimatorWithRealModelTest, WrongModelVersion) {
  std::string model_path = test::ResourcePath(
      "audio_processing/aec3/noop_ml_aec_model_for_testing", "tflite");

  // 1. Load the model from file.
  auto original_model =
      tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
  ASSERT_TRUE(original_model != nullptr);

  // 2. Get the raw buffer and size from the loaded model.
  const tflite::Allocation* allocation = original_model->allocation();
  const char* original_buffer_data =
      static_cast<const char*>(allocation->base());
  size_t original_buffer_size = allocation->bytes();

  // 3. Read the metadata
  const tflite::Model* model_obj = original_model->GetModel();
  ASSERT_TRUE(model_obj != nullptr);
  int32_t metadata_buffer_index = -1;
  if (model_obj->metadata()) {
    for (const auto* meta : *model_obj->metadata()) {
      if (meta->name() && meta->name()->str() == "REE_METADATA") {
        metadata_buffer_index = meta->buffer();
        break;
      }
    }
  }
  ASSERT_NE(metadata_buffer_index, -1) << "Metadata not found";

  // 4. Get the metadata buffer details from the model structure.
  const tflite::Buffer* ree_metadata_buffer =
      model_obj->buffers()->Get(metadata_buffer_index);
  ASSERT_TRUE(ree_metadata_buffer != nullptr);
  ASSERT_TRUE(ree_metadata_buffer->data() != nullptr);
  const char* metadata_data_ptr =
      reinterpret_cast<const char*>(ree_metadata_buffer->data()->data());
  size_t metadata_data_size = ree_metadata_buffer->data()->size();
  ASSERT_LT(metadata_data_ptr, original_buffer_data + original_buffer_size);

  // 5. Deserialize the metadata from the buffer copy, check original version.
  audioproc::ReeModelMetadata metadata_proto;
  ASSERT_TRUE(metadata_proto.ParseFromString(
      absl::string_view(metadata_data_ptr, metadata_data_size)));
  ASSERT_EQ(metadata_proto.version(), 2);

  // 6. Modify the version.
  metadata_proto.set_version(3);

  // 7. Serialize the modified metadata.
  std::string modified_metadata_str;
  ASSERT_TRUE(metadata_proto.SerializeToString(&modified_metadata_str));

  // 8. Ensure the size hasn't changed, then overwrite the bytes in the buffer
  // copy.
  ASSERT_EQ(modified_metadata_str.size(), metadata_data_size)
      << "Serialized metadata size changed, direct overwrite not possible.";
  std::vector<char> modified_buffer(
      original_buffer_data, original_buffer_data + original_buffer_size);

  std::memcpy(
      modified_buffer.data() + (metadata_data_ptr - original_buffer_data),
      modified_metadata_str.data(), modified_metadata_str.size());

  // 9. Build the modified model from the updated buffer.
  auto modified_model = tflite::FlatBufferModel::BuildFromBuffer(
      modified_buffer.data(), modified_buffer.size());
  ASSERT_TRUE(modified_model != nullptr);

  // 13. Attempt to load the model and expect failure due to version mismatch.
  tflite::ops::builtin::BuiltinOpResolver op_resolver;
  std::unique_ptr<NeuralResidualEchoEstimatorImpl::ModelRunner>
      tflite_model_runner = NeuralResidualEchoEstimatorImpl::LoadTfLiteModel(
          modified_model.get(), op_resolver);
  EXPECT_TRUE(tflite_model_runner == nullptr);
}

}  // namespace
}  // namespace webrtc
