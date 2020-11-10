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

#include <string>
#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block_framer.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float ComputeSampleValue(size_t chunk_counter,
                         size_t chunk_size,
                         size_t band,
                         size_t channel,
                         size_t sample_index,
                         int offset) {
  float value =
      static_cast<int>(chunk_counter * chunk_size + sample_index + channel) +
      offset;
  return value > 0 ? 5000 * band + value : 0;
}

void FillSubFrame(size_t sub_frame_counter,
                  int offset,
                  std::vector<std::vector<std::vector<float>>>* sub_frame) {
  for (size_t band = 0; band < sub_frame->size(); ++band) {
    for (size_t channel = 0; channel < (*sub_frame)[band].size(); ++channel) {
      for (size_t sample = 0; sample < (*sub_frame)[band][channel].size();
           ++sample) {
        (*sub_frame)[band][channel][sample] = ComputeSampleValue(
            sub_frame_counter, kSubFrameLength, band, channel, sample, offset);
      }
    }
  }
}

void FillSubFrameView(
    size_t sub_frame_counter,
    int offset,
    std::vector<std::vector<std::vector<float>>>* sub_frame,
    std::vector<std::vector<rtc::ArrayView<float>>>* sub_frame_view) {
  FillSubFrame(sub_frame_counter, offset, sub_frame);
  for (size_t band = 0; band < sub_frame_view->size(); ++band) {
    for (size_t channel = 0; channel < (*sub_frame_view)[band].size();
         ++channel) {
      (*sub_frame_view)[band][channel] = rtc::ArrayView<float>(
          &(*sub_frame)[band][channel][0], (*sub_frame)[band][channel].size());
    }
  }
}

bool VerifySubFrame(
    size_t sub_frame_counter,
    int offset,
    const std::vector<std::vector<rtc::ArrayView<float>>>& sub_frame_view) {
  std::vector<std::vector<std::vector<float>>> reference_sub_frame(
      sub_frame_view.size(),
      std::vector<std::vector<float>>(
          sub_frame_view[0].size(),
          std::vector<float>(sub_frame_view[0][0].size(), 0.f)));
  FillSubFrame(sub_frame_counter, offset, &reference_sub_frame);
  for (size_t band = 0; band < sub_frame_view.size(); ++band) {
    for (size_t channel = 0; channel < sub_frame_view[band].size(); ++channel) {
      for (size_t sample = 0; sample < sub_frame_view[band][channel].size();
           ++sample) {
        if (reference_sub_frame[band][channel][sample] !=
            sub_frame_view[band][channel][sample]) {
          return false;
        }
      }
    }
  }
  return true;
}

bool VerifyBlock(size_t block_counter,
                 int offset,
                 const std::vector<std::vector<std::vector<float>>>& block) {
  for (size_t band = 0; band < block.size(); ++band) {
    for (size_t channel = 0; channel < block[band].size(); ++channel) {
      for (size_t sample = 0; sample < block[band][channel].size(); ++sample) {
        const float reference_value = ComputeSampleValue(
            block_counter, kBlockSize, band, channel, sample, offset);
        if (reference_value != block[band][channel][sample]) {
          return false;
        }
      }
    }
  }
  return true;
}

// Verifies that the FrameBlocker properly forms blocks out of the frames.
void RunBlockerTest(int sample_rate_hz, size_t num_channels) {
  constexpr size_t kNumSubFramesToProcess = 20;
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<std::vector<float>>> block(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::vector<float>>> input_sub_frame(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> input_sub_frame_view(
      num_bands, std::vector<rtc::ArrayView<float>>(num_channels));
  FrameBlocker blocker(num_bands, num_channels);

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
void RunBlockerAndFramerTest(int sample_rate_hz, size_t num_channels) {
  const size_t kNumSubFramesToProcess = 20;
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<std::vector<float>>> block(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::vector<float>>> input_sub_frame(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<std::vector<float>>> output_sub_frame(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> output_sub_frame_view(
      num_bands, std::vector<rtc::ArrayView<float>>(num_channels));
  std::vector<std::vector<rtc::ArrayView<float>>> input_sub_frame_view(
      num_bands, std::vector<rtc::ArrayView<float>>(num_channels));
  FrameBlocker blocker(num_bands, num_channels);
  BlockFramer framer(num_bands, num_channels);

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
    if (sub_frame_index > 1) {
      EXPECT_TRUE(VerifySubFrame(sub_frame_index, -64, output_sub_frame_view));
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the FrameBlocker crashes if the InsertSubFrameAndExtractBlock
// method is called for inputs with the wrong number of bands or band lengths.
void RunWronglySizedInsertAndExtractParametersTest(
    int sample_rate_hz,
    size_t correct_num_channels,
    size_t num_block_bands,
    size_t num_block_channels,
    size_t block_length,
    size_t num_sub_frame_bands,
    size_t num_sub_frame_channels,
    size_t sub_frame_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<std::vector<float>>> block(
      num_block_bands,
      std::vector<std::vector<float>>(num_block_channels,
                                      std::vector<float>(block_length, 0.f)));
  std::vector<std::vector<std::vector<float>>> input_sub_frame(
      num_sub_frame_bands,
      std::vector<std::vector<float>>(
          num_sub_frame_channels, std::vector<float>(sub_frame_length, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> input_sub_frame_view(
      input_sub_frame.size(),
      std::vector<rtc::ArrayView<float>>(num_sub_frame_channels));
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(correct_num_bands, correct_num_channels);
  EXPECT_DEATH(
      blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block), "");
}

// Verifies that the FrameBlocker crashes if the ExtractBlock method is called
// for inputs with the wrong number of bands or band lengths.
void RunWronglySizedExtractParameterTest(int sample_rate_hz,
                                         size_t correct_num_channels,
                                         size_t num_block_bands,
                                         size_t num_block_channels,
                                         size_t block_length) {
  const size_t correct_num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<std::vector<float>>> correct_block(
      correct_num_bands,
      std::vector<std::vector<float>>(correct_num_channels,
                                      std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::vector<float>>> wrong_block(
      num_block_bands,
      std::vector<std::vector<float>>(num_block_channels,
                                      std::vector<float>(block_length, 0.f)));
  std::vector<std::vector<std::vector<float>>> input_sub_frame(
      correct_num_bands,
      std::vector<std::vector<float>>(
          correct_num_channels, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> input_sub_frame_view(
      input_sub_frame.size(),
      std::vector<rtc::ArrayView<float>>(correct_num_channels));
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(correct_num_bands, correct_num_channels);
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
                              size_t num_channels,
                              size_t num_preceeding_api_calls) {
  const size_t num_bands = NumBandsForRate(sample_rate_hz);

  std::vector<std::vector<std::vector<float>>> block(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::vector<float>>> input_sub_frame(
      num_bands, std::vector<std::vector<float>>(
                     num_channels, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> input_sub_frame_view(
      input_sub_frame.size(), std::vector<rtc::ArrayView<float>>(num_channels));
  FillSubFrameView(0, 0, &input_sub_frame, &input_sub_frame_view);
  FrameBlocker blocker(num_bands, num_channels);
  for (size_t k = 0; k < num_preceeding_api_calls; ++k) {
    blocker.InsertSubFrameAndExtractBlock(input_sub_frame_view, &block);
  }

  EXPECT_DEATH(blocker.ExtractBlock(&block), "");
}
#endif

std::string ProduceDebugText(int sample_rate_hz, size_t num_channels) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz;
  ss << ", number of channels: " << num_channels;
  return ss.Release();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FrameBlockerDeathTest,
     WrongNumberOfBandsInBlockForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, wrong_num_bands, correct_num_channels,
          kBlockSize, correct_num_bands, correct_num_channels, kSubFrameLength);
    }
  }
}

TEST(FrameBlockerDeathTest,
     WrongNumberOfChannelsInBlockForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_channels = correct_num_channels + 1;
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, correct_num_bands, wrong_num_channels,
          kBlockSize, correct_num_bands, correct_num_channels, kSubFrameLength);
    }
  }
}

TEST(FrameBlockerDeathTest,
     WrongNumberOfBandsInSubFrameForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, correct_num_bands, correct_num_channels,
          kBlockSize, wrong_num_bands, correct_num_channels, kSubFrameLength);
    }
  }
}

TEST(FrameBlockerDeathTest,
     WrongNumberOfChannelsInSubFrameForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_channels = correct_num_channels + 1;
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, correct_num_bands, wrong_num_channels,
          kBlockSize, correct_num_bands, wrong_num_channels, kSubFrameLength);
    }
  }
}

TEST(FrameBlockerDeathTest,
     WrongNumberOfSamplesInBlockForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, correct_num_bands, correct_num_channels,
          kBlockSize - 1, correct_num_bands, correct_num_channels,
          kSubFrameLength);
    }
  }
}

TEST(FrameBlockerDeathTest,
     WrongNumberOfSamplesInSubFrameForInsertSubFrameAndExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      RunWronglySizedInsertAndExtractParametersTest(
          rate, correct_num_channels, correct_num_bands, correct_num_channels,
          kBlockSize, correct_num_bands, correct_num_channels,
          kSubFrameLength - 1);
    }
  }
}

TEST(FrameBlockerDeathTest, WrongNumberOfBandsInBlockForExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_bands = (correct_num_bands % 3) + 1;
      RunWronglySizedExtractParameterTest(rate, correct_num_channels,
                                          wrong_num_bands, correct_num_channels,
                                          kBlockSize);
    }
  }
}

TEST(FrameBlockerDeathTest, WrongNumberOfChannelsInBlockForExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      const size_t wrong_num_channels = correct_num_channels + 1;
      RunWronglySizedExtractParameterTest(rate, correct_num_channels,
                                          correct_num_bands, wrong_num_channels,
                                          kBlockSize);
    }
  }
}

TEST(FrameBlockerDeathTest, WrongNumberOfSamplesInBlockForExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t correct_num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, correct_num_channels));
      const size_t correct_num_bands = NumBandsForRate(rate);
      RunWronglySizedExtractParameterTest(rate, correct_num_channels,
                                          correct_num_bands,
                                          correct_num_channels, kBlockSize - 1);
    }
  }
}

TEST(FrameBlockerDeathTest, WrongNumberOfPreceedingApiCallsForExtractBlock) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t num_channels : {1, 2, 4, 8}) {
      for (size_t num_calls = 0; num_calls < 4; ++num_calls) {
        rtc::StringBuilder ss;
        ss << "Sample rate: " << rate;
        ss << "Num channels: " << num_channels;
        ss << ", Num preceeding InsertSubFrameAndExtractBlock calls: "
           << num_calls;

        SCOPED_TRACE(ss.str());
        RunWrongExtractOrderTest(rate, num_channels, num_calls);
      }
    }
  }
}

// Verifies that the verification for 0 number of channels works.
TEST(FrameBlockerDeathTest, ZeroNumberOfChannelsParameter) {
  EXPECT_DEATH(FrameBlocker(16000, 0), "");
}

// Verifies that the verification for 0 number of bands works.
TEST(FrameBlockerDeathTest, ZeroNumberOfBandsParameter) {
  EXPECT_DEATH(FrameBlocker(0, 1), "");
}

// Verifiers that the verification for null sub_frame pointer works.
TEST(FrameBlockerDeathTest, NullBlockParameter) {
  std::vector<std::vector<std::vector<float>>> sub_frame(
      1, std::vector<std::vector<float>>(
             1, std::vector<float>(kSubFrameLength, 0.f)));
  std::vector<std::vector<rtc::ArrayView<float>>> sub_frame_view(
      sub_frame.size());
  FillSubFrameView(0, 0, &sub_frame, &sub_frame_view);
  EXPECT_DEATH(
      FrameBlocker(1, 1).InsertSubFrameAndExtractBlock(sub_frame_view, nullptr),
      "");
}

#endif

TEST(FrameBlocker, BlockBitexactness) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, num_channels));
      RunBlockerTest(rate, num_channels);
    }
  }
}

TEST(FrameBlocker, BlockerAndFramer) {
  for (auto rate : {16000, 32000, 48000}) {
    for (size_t num_channels : {1, 2, 4, 8}) {
      SCOPED_TRACE(ProduceDebugText(rate, num_channels));
      RunBlockerAndFramerTest(rate, num_channels);
    }
  }
}

}  // namespace webrtc
