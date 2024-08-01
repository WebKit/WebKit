/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/audio_view.h"

#include <array>

#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr const float kFloatStepIncrease = 0.5f;
constexpr const int16_t kIntStepIncrease = 1;

template <typename T>
void Increment(float& t) {
  t += kFloatStepIncrease;
}

template <typename T>
void Increment(int16_t& t) {
  t += kIntStepIncrease;
}

// Fills a given buffer with monotonically increasing values.
template <typename T>
void FillBuffer(rtc::ArrayView<T> buffer) {
  T value = {};
  for (T& t : buffer) {
    Increment<T>(value);
    t = value;
  }
}

}  // namespace

TEST(AudioViewTest, MonoView) {
  const size_t kArraySize = 100u;
  int16_t arr[kArraySize];
  FillBuffer(rtc::ArrayView<int16_t>(arr));

  MonoView<int16_t> mono(arr);
  MonoView<const int16_t> const_mono(arr);
  EXPECT_EQ(mono.size(), kArraySize);
  EXPECT_EQ(const_mono.size(), kArraySize);
  EXPECT_EQ(&mono[0], &const_mono[0]);
  EXPECT_EQ(mono[0], arr[0]);

  EXPECT_EQ(1u, NumChannels(mono));
  EXPECT_EQ(1u, NumChannels(const_mono));
  EXPECT_EQ(100u, SamplesPerChannel(mono));
  EXPECT_TRUE(IsMono(mono));
  EXPECT_TRUE(IsMono(const_mono));
}

TEST(AudioViewTest, InterleavedView) {
  const size_t kArraySize = 100u;
  int16_t arr[kArraySize];
  FillBuffer(rtc::ArrayView<int16_t>(arr));

  InterleavedView<int16_t> interleaved(arr, kArraySize, 1);
  EXPECT_EQ(NumChannels(interleaved), 1u);
  EXPECT_TRUE(IsMono(interleaved));
  EXPECT_EQ(SamplesPerChannel(interleaved), kArraySize);
  EXPECT_EQ(interleaved.AsMono().size(), kArraySize);
  EXPECT_EQ(&interleaved.AsMono()[0], &arr[0]);
  EXPECT_EQ(interleaved.AsMono(), interleaved.data());

  // Basic iterator test.
  int i = 0;
  for (auto s : interleaved) {
    EXPECT_EQ(s, arr[i++]);
  }

  interleaved = InterleavedView<int16_t>(arr, kArraySize / 2, 2);
  InterleavedView<const int16_t> const_interleaved(arr, 50, 2);
  EXPECT_EQ(NumChannels(interleaved), 2u);
  EXPECT_EQ(NumChannels(const_interleaved), 2u);
  EXPECT_EQ(&const_interleaved[0], &interleaved[0]);
  EXPECT_TRUE(!IsMono(interleaved));
  EXPECT_TRUE(!IsMono(const_interleaved));
  EXPECT_EQ(SamplesPerChannel(interleaved), 50u);
  EXPECT_EQ(SamplesPerChannel(const_interleaved), 50u);

  interleaved = InterleavedView<int16_t>(arr, 4);
  EXPECT_EQ(NumChannels(interleaved), 4u);
  InterleavedView<const int16_t> const_interleaved2(interleaved);
  EXPECT_EQ(NumChannels(const_interleaved2), 4u);
  EXPECT_EQ(SamplesPerChannel(interleaved), 25u);

  const_interleaved2 = interleaved;
  EXPECT_EQ(NumChannels(const_interleaved2), 4u);
  EXPECT_EQ(&const_interleaved2[0], &interleaved[0]);
}

TEST(AudioViewTest, DeinterleavedView) {
  const size_t kArraySize = 100u;
  int16_t arr[kArraySize] = {};
  DeinterleavedView<int16_t> di(arr, 10, 10);
  DeinterleavedView<const int16_t> const_di(arr, 10, 10);
  EXPECT_EQ(NumChannels(di), 10u);
  EXPECT_EQ(SamplesPerChannel(di), 10u);
  EXPECT_TRUE(!IsMono(di));
  EXPECT_EQ(const_di[5][1], di[5][1]);  // Spot check.
  // For deinterleaved views, although they may hold multiple channels,
  // the AsMono() method is still available and returns the first channel
  // in the view.
  auto mono_ch = di.AsMono();
  EXPECT_EQ(NumChannels(mono_ch), 1u);
  EXPECT_EQ(SamplesPerChannel(mono_ch), 10u);
  EXPECT_EQ(di[0], mono_ch);  // first channel should be same as mono.

  di = DeinterleavedView<int16_t>(arr, 50, 2);
  // Test assignment.
  const_di = di;
  EXPECT_EQ(&di.AsMono()[0], &const_di.AsMono()[0]);

  // Access the second channel in the deinterleaved view.
  // The start of the second channel should be directly after the first channel.
  // The memory width of each channel is held by the `stride()` member which
  // by default is the same value as samples per channel.
  mono_ch = di[1];
  EXPECT_EQ(SamplesPerChannel(mono_ch), 50u);
  EXPECT_EQ(&mono_ch[0], &arr[di.samples_per_channel()]);
}

TEST(AudioViewTest, CopySamples) {
  const size_t kArraySize = 100u;
  int16_t source_arr[kArraySize] = {};
  int16_t dest_arr[kArraySize] = {};
  FillBuffer(rtc::ArrayView<int16_t>(source_arr));

  InterleavedView<const int16_t> source(source_arr, 2);
  InterleavedView<int16_t> destination(dest_arr, 2);

  static_assert(IsInterleavedView(source) == IsInterleavedView(destination),
                "");

  // Values in `dest_arr` should all be 0, none of the values in `source_arr`
  // should be 0.
  for (size_t i = 0; i < kArraySize; ++i) {
    ASSERT_EQ(dest_arr[i], 0);
    ASSERT_NE(source_arr[i], 0);
  }

  CopySamples(destination, source);
  for (size_t i = 0; i < kArraySize; ++i) {
    ASSERT_EQ(dest_arr[i], source_arr[i]) << "i == " << i;
  }
}

TEST(AudioViewTest, ClearSamples) {
  std::array<int16_t, 100u> samples = {};
  FillBuffer(rtc::ArrayView<int16_t>(samples));
  ASSERT_NE(samples[0], 0);
  ClearSamples(samples);
  for (const auto s : samples) {
    ASSERT_EQ(s, 0);
  }

  std::array<float, 100u> samples_f = {};
  FillBuffer(rtc::ArrayView<float>(samples_f));
  ASSERT_NE(samples_f[0], 0.0);
  ClearSamples(samples_f);
  for (const auto s : samples_f) {
    ASSERT_EQ(s, 0.0);
  }

  // Clear only half of the buffer
  FillBuffer(rtc::ArrayView<int16_t>(samples));
  const auto half_way = samples.size() / 2;
  ClearSamples(samples, half_way);
  for (size_t i = 0u; i < samples.size(); ++i) {
    if (i < half_way) {
      ASSERT_EQ(samples[i], 0);
    } else {
      ASSERT_NE(samples[i], 0);
    }
  }
}
}  // namespace webrtc
