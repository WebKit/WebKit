/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/block_framer.h"

#include <sstream>
#include <string>
#include <vector>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

void SetupSubFrameView(std::vector<std::vector<float>>* sub_frame,
                       std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  for (size_t k = 0; k < sub_frame_view->size(); ++k) {
    (*sub_frame_view)[k] =
        rtc::ArrayView<float>((*sub_frame)[k].data(), (*sub_frame)[k].size());
  }
}

float ComputeSampleValue(size_t chunk_counter,
                         size_t chunk_size,
                         size_t band,
                         size_t sample_index,
                         int offset) {
  float value =
      static_cast<int>(chunk_counter * chunk_size + sample_index) + offset;
  return value > 0 ? 5000 * band + value : 0;
}

bool VerifySubFrame(size_t sub_frame_counter,
                    int offset,
                    const std::vector<rtc::ArrayView<float>>& sub_frame_view) {
  for (size_t k = 0; k < sub_frame_view.size(); ++k) {
    for (size_t i = 0; i < sub_frame_view[k].size(); ++i) {
      const float reference_value =
          ComputeSampleValue(sub_frame_counter, kSubFrameLength, k, i, offset);
      if (reference_value != sub_frame_view[k][i]) {
        return false;
      }
    }
  }
  return true;
}

void FillBlock(size_t block_counter, std::vector<std::vector<float>>* block) {
  for (size_t k = 0; k < block->size(); ++k) {
    for (size_t i = 0; i < (*block)[0].size(); ++i) {
      (*block)[k][i] = ComputeSampleValue(block_counter, kBlockSize, k, i, 0);
    }
  }
}

// Verifies that the BlockFramer is able to produce the expected frame content.
void RunFramerTest(int sample_rate_hz) {
  constexpr size_t kNumSubFramesToProcess = 2;
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(num_bands,
                                        std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> output_sub_frame(
      num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> output_sub_frame_view(num_bands);
  SetupSubFrameView(&output_sub_frame, &output_sub_frame_view);
  BlockFramer framer(num_bands);

  size_t block_index = 0;
  for (size_t sub_frame_index = 0; sub_frame_index < kNumSubFramesToProcess;
       ++sub_frame_index) {
    FillBlock(block_index++, &block);
    framer.InsertBlockAndExtractSubFrame(block, &output_sub_frame_view);
    EXPECT_TRUE(VerifySubFrame(sub_frame_index, -64, output_sub_frame_view));

    if ((sub_frame_index + 1) % 4 == 0) {
      FillBlock(block_index++, &block);
      framer.InsertBlock(block);
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the BlockFramer crashes if the InsertBlockAndExtractSubFrame
// method is called for inputs with the wrong number of bands or band lengths.
void RunWronglySizedInsertAndExtractParametersTest(int sample_rate_hz,
                                                   size_t num_block_bands,
                                                   size_t block_length,
                                                   size_t num_sub_frame_bands,
                                                   size_t sub_frame_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(num_block_bands,
                                        std::vector<float>(block_length, 0.f));
  std::vector<std::vector<float>> output_sub_frame(
      num_sub_frame_bands, std::vector<float>(sub_frame_length, 0.f));
  std::vector<rtc::ArrayView<float>> output_sub_frame_view(
      output_sub_frame.size());
  SetupSubFrameView(&output_sub_frame, &output_sub_frame_view);
  BlockFramer framer(correct_num_bands);
  EXPECT_DEATH(
      framer.InsertBlockAndExtractSubFrame(block, &output_sub_frame_view), "");
}

// Verifies that the BlockFramer crashes if the InsertBlock method is called for
// inputs with the wrong number of bands or band lengths.
void RunWronglySizedInsertParameterTest(int sample_rate_hz,
                                        size_t num_block_bands,
                                        size_t block_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> correct_block(
      correct_num_bands, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> wrong_block(
      num_block_bands, std::vector<float>(block_length, 0.f));
  std::vector<std::vector<float>> output_sub_frame(
      correct_num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> output_sub_frame_view(
      output_sub_frame.size());
  SetupSubFrameView(&output_sub_frame, &output_sub_frame_view);
  BlockFramer framer(correct_num_bands);
  framer.InsertBlockAndExtractSubFrame(correct_block, &output_sub_frame_view);
  framer.InsertBlockAndExtractSubFrame(correct_block, &output_sub_frame_view);
  framer.InsertBlockAndExtractSubFrame(correct_block, &output_sub_frame_view);
  framer.InsertBlockAndExtractSubFrame(correct_block, &output_sub_frame_view);

  EXPECT_DEATH(framer.InsertBlock(wrong_block), "");
}

// Verifies that the BlockFramer crashes if the InsertBlock method is called
// after a wrong number of previous InsertBlockAndExtractSubFrame method calls
// have been made.
void RunWronglyInsertOrderTest(int sample_rate_hz,
                               size_t num_preceeding_api_calls) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(correct_num_bands,
                                        std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> output_sub_frame(
      correct_num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> output_sub_frame_view(
      output_sub_frame.size());
  SetupSubFrameView(&output_sub_frame, &output_sub_frame_view);
  BlockFramer framer(correct_num_bands);
  for (size_t k = 0; k < num_preceeding_api_calls; ++k) {
    framer.InsertBlockAndExtractSubFrame(block, &output_sub_frame_view);
  }

  EXPECT_DEATH(framer.InsertBlock(block), "");
}
#endif

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(BlockFramer, WrongNumberOfBandsInBlockForInsertBlockAndExtractSubFrame) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedInsertAndExtractParametersTest(
        rate, wrong_num_bands, kBlockSize, correct_num_bands, kSubFrameLength);
  }
}

TEST(BlockFramer,
     WrongNumberOfBandsInSubFrameForInsertBlockAndExtractSubFrame) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedInsertAndExtractParametersTest(
        rate, correct_num_bands, kBlockSize, wrong_num_bands, kSubFrameLength);
  }
}

TEST(BlockFramer, WrongNumberOfSamplesInBlockForInsertBlockAndExtractSubFrame) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedInsertAndExtractParametersTest(
        rate, correct_num_bands, kBlockSize - 1, correct_num_bands,
        kSubFrameLength);
  }
}

TEST(BlockFramer,
     WrongNumberOfSamplesInSubFrameForInsertBlockAndExtractSubFrame) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedInsertAndExtractParametersTest(rate, correct_num_bands,
                                                  kBlockSize, correct_num_bands,
                                                  kSubFrameLength - 1);
  }
}

TEST(BlockFramer, WrongNumberOfBandsInBlockForInsertBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedInsertParameterTest(rate, wrong_num_bands, kBlockSize);
  }
}

TEST(BlockFramer, WrongNumberOfSamplesInBlockForInsertBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedInsertParameterTest(rate, correct_num_bands, kBlockSize - 1);
  }
}

TEST(BlockFramer, WrongNumberOfPreceedingApiCallsForInsertBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t num_calls = 0; num_calls < 4; ++num_calls) {
      std::ostringstream ss;
      ss << "Sample rate: " << rate;
      ss << ", Num preceeding InsertBlockAndExtractSubFrame calls: "
         << num_calls;

      SCOPED_TRACE(ss.str());
      RunWronglyInsertOrderTest(rate, num_calls);
    }
  }
}

// Verifiers that the verification for null sub_frame pointer works.
TEST(BlockFramer, NullSubFrameParameter) {
  EXPECT_DEATH(BlockFramer(1).InsertBlockAndExtractSubFrame(
                   std::vector<std::vector<float>>(
                       1, std::vector<float>(kBlockSize, 0.f)),
                   nullptr),
               "");
}

#endif

TEST(BlockFramer, FrameBitexactness) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunFramerTest(rate);
  }
}

}  // namespace webrtc
