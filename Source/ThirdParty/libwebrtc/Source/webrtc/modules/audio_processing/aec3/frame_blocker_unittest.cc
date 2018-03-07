/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/frame_blocker.h"

#include <sstream>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_framer.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float ComputeSampleValue(size_t chunk_counter,
                         size_t chunk_size,
                         size_t band,
                         size_t sample_index,
                         int offset) {
  float value =
      static_cast<int>(chunk_counter * chunk_size + sample_index) + offset;
  return value > 0 ? 5000 * band + value : 0;
}

void FillSubFrame(size_t sub_frame_counter,
                  int offset,
                  std::vector<std::vector<float>>* sub_frame) {
  for (size_t k = 0; k < sub_frame->size(); ++k) {
    for (size_t i = 0; i < (*sub_frame)[0].size(); ++i) {
      (*sub_frame)[k][i] =
          ComputeSampleValue(sub_frame_counter, kSubFrameLength, k, i, offset);
    }
  }
}

void FillSubFrameView(size_t sub_frame_counter,
                      int offset,
                      std::vector<std::vector<float>>* sub_frame,
                      std::vector<rtc::ArrayView<float>>* sub_frame_view) {
  FillSubFrame(sub_frame_counter, offset, sub_frame);
  for (size_t k = 0; k < sub_frame_view->size(); ++k) {
    (*sub_frame_view)[k] =
        rtc::ArrayView<float>(&(*sub_frame)[k][0], (*sub_frame)[k].size());
  }
}

bool VerifySubFrame(size_t sub_frame_counter,
                    int offset,
                    const std::vector<rtc::ArrayView<float>>& sub_frame_view) {
  std::vector<std::vector<float>> reference_sub_frame(
      sub_frame_view.size(), std::vector<float>(sub_frame_view[0].size(), 0.f));
  FillSubFrame(sub_frame_counter, offset, &reference_sub_frame);
  for (size_t k = 0; k < sub_frame_view.size(); ++k) {
    for (size_t i = 0; i < sub_frame_view[k].size(); ++i) {
      if (reference_sub_frame[k][i] != sub_frame_view[k][i]) {
        return false;
      }
    }
  }
  return true;
}

bool VerifyBlock(size_t block_counter,
                 int offset,
                 const std::vector<std::vector<float>>& block) {
  for (size_t k = 0; k < block.size(); ++k) {
    for (size_t i = 0; i < block[k].size(); ++i) {
      const float reference_value =
          ComputeSampleValue(block_counter, kBlockSize, k, i, offset);
      if (reference_value != block[k][i]) {
        return false;
      }
    }
  }
  return true;
}

// Verifies that the FrameBlocker properly forms blocks out of the frames.
void RunBlockerTest(int sample_rate_hz) {
  constexpr size_t kNumSubFramesToProcess = 20;
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(num_bands,
                                        std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> input_sub_frame(
      num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> input_sub_frame_view(num_bands);
  FrameBlocker blocker(num_bands);

  size_t block_counter = 0;
  for (size_t sub_frame_index = 0; sub_frame_index < kNumSubFramesToProcess;
       ++sub_frame_index) {
    FillSubFrameView(sub_frame_index, 0, &input_sub_frame,
                     &input_sub_frame_view);

    blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block);
    VerifyBlock(block_counter++, 0, block);

    if ((sub_frame_index + 1) % 4 == 0) {
      EXPECT_TRUE(blocker.IsBlockAvailable());
    } else {
      EXPECT_FALSE(blocker.IsBlockAvailable());
    }
    if (blocker.IsBlockAvailable()) {
      blocker.ExtractBlock(&block);
      VerifyBlock(block_counter++, 0, block);
    }
  }
}

// Verifies that the FrameBlocker and BlockFramer work well together and produce
// the expected output.
void RunBlockerAndFramerTest(int sample_rate_hz) {
  const size_t kNumSubFramesToProcess = 20;
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(num_bands,
                                        std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> input_sub_frame(
      num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<std::vector<float>> output_sub_frame(
      num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> output_sub_frame_view(num_bands);
  std::vector<rtc::ArrayView<float>> input_sub_frame_view(num_bands);
  FrameBlocker blocker(num_bands);
  BlockFramer framer(num_bands);

  for (size_t sub_frame_index = 0; sub_frame_index < kNumSubFramesToProcess;
       ++sub_frame_index) {
    FillSubFrameView(sub_frame_index, 0, &input_sub_frame,
                     &input_sub_frame_view);
    FillSubFrameView(sub_frame_index, 0, &output_sub_frame,
                     &output_sub_frame_view);

    blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block);
    framer.InsertBlockAndExtractSubFrame(block, &output_sub_frame_view);

    if ((sub_frame_index + 1) % 4 == 0) {
      EXPECT_TRUE(blocker.IsBlockAvailable());
    } else {
      EXPECT_FALSE(blocker.IsBlockAvailable());
    }
    if (blocker.IsBlockAvailable()) {
      blocker.ExtractBlock(&block);
      framer.InsertBlock(block);
    }
    EXPECT_TRUE(VerifySubFrame(sub_frame_index, -64, output_sub_frame_view));
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the FrameBlocker crashes if the InsertSubFrameAndExtractBlock
// method is called for inputs with the wrong number of bands or band lengths.
void RunWronglySizedInsertAndExtractParametersTest(int sample_rate_hz,
                                                   size_t num_block_bands,
                                                   size_t block_length,
                                                   size_t num_sub_frame_bands,
                                                   size_t sub_frame_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(num_block_bands,
                                        std::vector<float>(block_length, 0.f));
  std::vector<std::vector<float>> input_sub_frame(
      num_sub_frame_bands, std::vector<float>(sub_frame_length, 0.f));
  std::vector<rtc::ArrayView<float>> input_sub_frame_view(
      input_sub_frame.size());
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(correct_num_bands);
  EXPECT_DEATH(
      blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block), "");
}

// Verifies that the FrameBlocker crashes if the ExtractBlock method is called
// for inputs with the wrong number of bands or band lengths.
void RunWronglySizedExtractParameterTest(int sample_rate_hz,
                                         size_t num_block_bands,
                                         size_t block_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> correct_block(
      correct_num_bands, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> wrong_block(
      num_block_bands, std::vector<float>(block_length, 0.f));
  std::vector<std::vector<float>> input_sub_frame(
      correct_num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> input_sub_frame_view(
      input_sub_frame.size());
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(correct_num_bands);
  blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &correct_block);
  blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &correct_block);
  blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &correct_block);
  blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &correct_block);

  EXPECT_DEATH(blocker.ExtractBlock(&wrong_block), "");
}

// Verifies that the FrameBlocker crashes if the ExtractBlock method is called
// after a wrong number of previous InsertSubFrameAndExtractBlock method calls
// have been made.
void RunWrongExtractOrderTest(int sample_rate_hz,
                              size_t num_preceeding_api_calls) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<float>> block(correct_num_bands,
                                        std::vector<float>(kBlockSize, 0.f));
  std::vector<std::vector<float>> input_sub_frame(
      correct_num_bands, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> input_sub_frame_view(
      input_sub_frame.size());
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(correct_num_bands);
  for (size_t k = 0; k < num_preceeding_api_calls; ++k) {
    blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block);
  }

  EXPECT_DEATH(blocker.ExtractBlock(&block), "");
}
#endif

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FrameBlocker, WrongNumberOfBandsInBlockForInsertSubFrameAndExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedInsertAndExtractParametersTest(
        rate, wrong_num_bands, kBlockSize, correct_num_bands, kSubFrameLength);
  }
}

TEST(FrameBlocker,
     WrongNumberOfBandsInSubFrameForInsertSubFrameAndExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedInsertAndExtractParametersTest(
        rate, correct_num_bands, kBlockSize, wrong_num_bands, kSubFrameLength);
  }
}

TEST(FrameBlocker,
     WrongNumberOfSamplesInBlockForInsertSubFrameAndExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedInsertAndExtractParametersTest(
        rate, correct_num_bands, kBlockSize - 1, correct_num_bands,
        kSubFrameLength);
  }
}

TEST(FrameBlocker,
     WrongNumberOfSamplesInSubFrameForInsertSubFrameAndExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedInsertAndExtractParametersTest(rate, correct_num_bands,
                                                  kBlockSize, correct_num_bands,
                                                  kSubFrameLength - 1);
  }
}

TEST(FrameBlocker, WrongNumberOfBandsInBlockForExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
    RunWronglySizedExtractParameterTest(rate, wrong_num_bands, kBlockSize);
  }
}

TEST(FrameBlocker, WrongNumberOfSamplesInBlockForExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    const size_t correct_num_bands = NumBandsForRate(rate);
    RunWronglySizedExtractParameterTest(rate, correct_num_bands,
                                        kBlockSize - 1);
  }
}

TEST(FrameBlocker, WrongNumberOfPreceedingApiCallsForExtractBlock) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    for (size_t num_calls = 0; num_calls < 4; ++num_calls) {
      std::ostringstream ss;
      ss << "Sample rate: " << rate;
      ss << ", Num preceeding InsertSubFrameAndExtractBlock calls: "
         << num_calls;

      SCOPED_TRACE(ss.str());
      RunWrongExtractOrderTest(rate, num_calls);
    }
  }
}

// Verifiers that the verification for null sub_frame pointer works.
TEST(FrameBlocker, NullBlockParameter) {
  std::vector<std::vector<float>> sub_frame(
      1, std::vector<float>(kSubFrameLength, 0.f));
  std::vector<rtc::ArrayView<float>> sub_frame_view(sub_frame.size());
  FillSubFrameView(0, 0, &sub_frame, &sub_frame_view);
  EXPECT_DEATH(
      FrameBlocker(1).InsertSubFrameAndExtractBlock(sub_frame_view, nullptr),
      "");
}

#endif

TEST(FrameBlocker, BlockBitexactness) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunBlockerTest(rate);
  }
}

TEST(FrameBlocker, BlockerAndFramer) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunBlockerAndFramerTest(rate);
  }
}

}  // namespace webrtc
