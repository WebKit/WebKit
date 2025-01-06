/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/delta_encoding.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

void SetFixedLengthEncoderDeltaSignednessForTesting(bool signedness);
void UnsetFixedLengthEncoderDeltaSignednessForTesting();

namespace {

enum class DeltaSignedness { kNoOverride, kForceUnsigned, kForceSigned };

void MaybeSetSignedness(DeltaSignedness signedness) {
  switch (signedness) {
    case DeltaSignedness::kNoOverride:
      UnsetFixedLengthEncoderDeltaSignednessForTesting();
      return;
    case DeltaSignedness::kForceUnsigned:
      SetFixedLengthEncoderDeltaSignednessForTesting(false);
      return;
    case DeltaSignedness::kForceSigned:
      SetFixedLengthEncoderDeltaSignednessForTesting(true);
      return;
  }
  RTC_DCHECK_NOTREACHED();
}

uint64_t RandomWithMaxBitWidth(Random* prng, uint64_t max_width) {
  RTC_DCHECK_GE(max_width, 1u);
  RTC_DCHECK_LE(max_width, 64u);

  const uint64_t low = prng->Rand(std::numeric_limits<uint32_t>::max());
  const uint64_t high =
      max_width > 32u ? prng->Rand(std::numeric_limits<uint32_t>::max()) : 0u;

  const uint64_t random_before_mask = (high << 32) | low;

  if (max_width < 64) {
    return random_before_mask & ((static_cast<uint64_t>(1) << max_width) - 1);
  } else {
    return random_before_mask;
  }
}

// Encodes `values` based on `base`, then decodes the result and makes sure
// that it is equal to the original input.
// If `encoded_string` is non-null, the encoded result will also be written
// into it.
void TestEncodingAndDecoding(std::optional<uint64_t> base,
                             const std::vector<std::optional<uint64_t>>& values,
                             std::string* encoded_string = nullptr) {
  const std::string encoded = EncodeDeltas(base, values);
  if (encoded_string) {
    *encoded_string = encoded;
  }

  const std::vector<std::optional<uint64_t>> decoded =
      DecodeDeltas(encoded, base, values.size());

  EXPECT_EQ(decoded, values);
}

std::vector<std::optional<uint64_t>> CreateSequenceByFirstValue(
    uint64_t first,
    size_t sequence_length) {
  std::vector<std::optional<uint64_t>> sequence(sequence_length);
  std::iota(sequence.begin(), sequence.end(), first);
  return sequence;
}

std::vector<std::optional<uint64_t>> CreateSequenceByLastValue(
    uint64_t last,
    size_t num_values) {
  const uint64_t first = last - num_values + 1;
  std::vector<std::optional<uint64_t>> result(num_values);
  std::iota(result.begin(), result.end(), first);
  return result;
}

// If `sequence_length` is greater than the number of deltas, the sequence of
// deltas will wrap around.
std::vector<std::optional<uint64_t>> CreateSequenceByOptionalDeltas(
    uint64_t first,
    const std::vector<std::optional<uint64_t>>& deltas,
    size_t sequence_length) {
  RTC_DCHECK_GE(sequence_length, 1);

  std::vector<std::optional<uint64_t>> sequence(sequence_length);

  uint64_t previous = first;
  for (size_t i = 0, next_delta_index = 0; i < sequence.size(); ++i) {
    if (deltas[next_delta_index].has_value()) {
      sequence[i] =
          std::optional<uint64_t>(previous + deltas[next_delta_index].value());
      previous = sequence[i].value();
    }
    next_delta_index = (next_delta_index + 1) % deltas.size();
  }

  return sequence;
}

size_t EncodingLengthUpperBound(size_t delta_max_bit_width,
                                size_t num_of_deltas,
                                DeltaSignedness signedness_override) {
  std::optional<size_t> smallest_header_size_bytes;
  switch (signedness_override) {
    case DeltaSignedness::kNoOverride:
    case DeltaSignedness::kForceUnsigned:
      smallest_header_size_bytes = 1;
      break;
    case DeltaSignedness::kForceSigned:
      smallest_header_size_bytes = 2;
      break;
  }
  RTC_DCHECK(smallest_header_size_bytes);

  return delta_max_bit_width * num_of_deltas + *smallest_header_size_bytes;
}

// If `sequence_length` is greater than the number of deltas, the sequence of
// deltas will wrap around.
std::vector<std::optional<uint64_t>> CreateSequenceByDeltas(
    uint64_t first,
    const std::vector<uint64_t>& deltas,
    size_t sequence_length) {
  RTC_DCHECK(!deltas.empty());
  std::vector<std::optional<uint64_t>> optional_deltas(deltas.size());
  for (size_t i = 0; i < deltas.size(); ++i) {
    optional_deltas[i] = std::optional<uint64_t>(deltas[i]);
  }
  return CreateSequenceByOptionalDeltas(first, optional_deltas,
                                        sequence_length);
}

// Tests of the delta encoding, parameterized by the number of values
// in the sequence created by the test.
class DeltaEncodingTest
    : public ::testing::TestWithParam<
          std::tuple<DeltaSignedness, size_t, bool, uint64_t>> {
 public:
  DeltaEncodingTest()
      : signedness_(std::get<0>(GetParam())),
        num_of_values_(std::get<1>(GetParam())),
        optional_values_(std::get<2>(GetParam())),
        partial_random_seed_(std::get<3>(GetParam())) {
    MaybeSetSignedness(signedness_);
  }

  ~DeltaEncodingTest() override = default;

  // Running with the same seed for all variants would make all tests start
  // with the same sequence; avoid this by making the seed different.
  uint64_t Seed() const {
    // Multiply everything but by different primes to produce unique results.
    return 2 * static_cast<uint64_t>(signedness_) + 3 * num_of_values_ +
           5 * optional_values_ + 7 * partial_random_seed_;
  }

  const DeltaSignedness signedness_;
  const uint64_t num_of_values_;
  const bool optional_values_;
  const uint64_t partial_random_seed_;  // Explained where it's used.
};

TEST_P(DeltaEncodingTest, AllValuesEqualToExistentBaseValue) {
  const std::optional<uint64_t> base(3432);
  std::vector<std::optional<uint64_t>> values(num_of_values_);
  std::fill(values.begin(), values.end(), base);
  std::string encoded;
  TestEncodingAndDecoding(base, values, &encoded);

  // Additional requirement - the encoding should be efficient in this
  // case - the empty string will be used.
  EXPECT_TRUE(encoded.empty());
}

TEST_P(DeltaEncodingTest, AllValuesEqualToNonExistentBaseValue) {
  if (!optional_values_) {
    return;  // Test irrelevant for this case.
  }

  const std::optional<uint64_t> base;
  std::vector<std::optional<uint64_t>> values(num_of_values_);
  std::fill(values.begin(), values.end(), base);
  std::string encoded;
  TestEncodingAndDecoding(base, values, &encoded);

  // Additional requirement - the encoding should be efficient in this
  // case - the empty string will be used.
  EXPECT_TRUE(encoded.empty());
}

TEST_P(DeltaEncodingTest, BaseNonExistentButSomeOtherValuesExist) {
  if (!optional_values_) {
    return;  // Test irrelevant for this case.
  }

  const std::optional<uint64_t> base;
  std::vector<std::optional<uint64_t>> values(num_of_values_);

  Random prng(Seed());

  const uint64_t max_bit_width = 1 + prng.Rand(63);  // [1, 64]

  for (size_t i = 0; i < values.size();) {
    // Leave a random number of values as non-existent.
    const size_t non_existent_count = prng.Rand(values.size() - i - 1);
    i += non_existent_count;

    // Assign random values to a random number of values. (At least one, to
    // prevent this iteration of the outer loop from being a no-op.)
    const size_t existent_count =
        std::max<size_t>(prng.Rand(values.size() - i - 1), 1);
    for (size_t j = 0; j < existent_count; ++j) {
      values[i + j] = RandomWithMaxBitWidth(&prng, max_bit_width);
    }
    i += existent_count;
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MinDeltaNoWrapAround) {
  const std::optional<uint64_t> base(3432);

  auto values = CreateSequenceByFirstValue(base.value() + 1, num_of_values_);
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[0] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaNoWrapAround) {
  const uint64_t kBigDelta = 132828;
  const std::optional<uint64_t> base(3432);

  auto values =
      CreateSequenceByFirstValue(base.value() + kBigDelta, num_of_values_);
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[0] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaNoWrapAround) {
  const std::optional<uint64_t> base(3432);

  auto values = CreateSequenceByLastValue(std::numeric_limits<uint64_t>::max(),
                                          num_of_values_);
  ASSERT_GT(values[values.size() - 1], base) << "Sanity; must not wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[0] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, SmallDeltaWithWrapAroundComparedToBase) {
  if (optional_values_ && num_of_values_ == 1) {
    return;  // Inapplicable
  }

  const std::optional<uint64_t> base(std::numeric_limits<uint64_t>::max());

  auto values = CreateSequenceByDeltas(*base, {1, 10, 3}, num_of_values_);
  ASSERT_LT(values[0], base) << "Sanity; must wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, SmallDeltaWithWrapAroundInValueSequence) {
  if (num_of_values_ == 1 || (optional_values_ && num_of_values_ < 3)) {
    return;  // Inapplicable.
  }

  const std::optional<uint64_t> base(std::numeric_limits<uint64_t>::max() - 2);

  auto values = CreateSequenceByDeltas(*base, {1, 10, 3}, num_of_values_);
  ASSERT_LT(values[values.size() - 1], values[0]) << "Sanity; must wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    RTC_DCHECK_GT(values.size() - 1, 1u);  // Wrap around not cancelled.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

// Suppress "integral constant overflow" warning; this is the test's focus.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4307)
#endif
TEST_P(DeltaEncodingTest, BigDeltaWithWrapAroundComparedToBase) {
  if (optional_values_ && num_of_values_ == 1) {
    return;  // Inapplicable
  }

  const uint64_t kBigDelta = 132828;
  const std::optional<uint64_t> base(std::numeric_limits<uint64_t>::max() -
                                     kBigDelta + 3);

  auto values =
      CreateSequenceByFirstValue(base.value() + kBigDelta, num_of_values_);
  ASSERT_LT(values[0], base.value()) << "Sanity; must wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, BigDeltaWithWrapAroundInValueSequence) {
  if (num_of_values_ == 1 || (optional_values_ && num_of_values_ < 3)) {
    return;  // Inapplicable.
  }

  const uint64_t kBigDelta = 132828;
  const std::optional<uint64_t> base(std::numeric_limits<uint64_t>::max() -
                                     kBigDelta + 3);

  auto values = CreateSequenceByFirstValue(std::numeric_limits<uint64_t>::max(),
                                           num_of_values_);
  ASSERT_LT(values[values.size() - 1], values[0]) << "Sanity; must wrap around";

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    RTC_DCHECK_GT(values.size() - 1, 1u);  // Wrap around not cancelled.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

TEST_P(DeltaEncodingTest, MaxDeltaWithWrapAroundComparedToBase) {
  if (optional_values_ && num_of_values_ == 1) {
    return;  // Inapplicable
  }

  const std::optional<uint64_t> base(3432);
  auto values = CreateSequenceByFirstValue(*base - 1, num_of_values_);

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

TEST_P(DeltaEncodingTest, MaxDeltaWithWrapAroundInValueSequence) {
  if (num_of_values_ == 1 || (optional_values_ && num_of_values_ < 3)) {
    return;  // Inapplicable.
  }

  const std::optional<uint64_t> base(3432);

  auto values = CreateSequenceByDeltas(
      *base, {0, std::numeric_limits<uint64_t>::max(), 3}, num_of_values_);
  // Wraps around continuously by virtue of being max(); will not ASSERT.

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    RTC_DCHECK_GT(values.size() - 1, 1u);  // Wrap around not cancelled.
    values[1] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

// If num_of_values_ == 1, a zero delta will yield an empty string; that's
// already covered by AllValuesEqualToExistentBaseValue, but it doesn't hurt to
// test again. For all other cases, we have a new test.
TEST_P(DeltaEncodingTest, ZeroDelta) {
  const std::optional<uint64_t> base(3432);

  // Arbitrary sequence of deltas with intentional zero deltas, as well as
  // consecutive zeros.
  const std::vector<uint64_t> deltas = {0,      312, 11, 1,  1, 0, 0, 12,
                                        400321, 3,   3,  12, 5, 0, 6};
  auto values = CreateSequenceByDeltas(base.value(), deltas, num_of_values_);

  if (optional_values_) {
    // Arbitrarily make one of the values non-existent, to force
    // optional-supporting encoding.
    values[0] = std::optional<uint64_t>();
  }

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_SUITE_P(
    SignednessOverrideAndNumberOfValuesInSequence,
    DeltaEncodingTest,
    ::testing::Combine(::testing::Values(DeltaSignedness::kNoOverride,
                                         DeltaSignedness::kForceUnsigned,
                                         DeltaSignedness::kForceSigned),
                       ::testing::Values(1, 2, 100, 10000),
                       ::testing::Bool(),
                       ::testing::Values(10, 20, 30)));

// Tests over the quality of the compression (as opposed to its correctness).
// Not to be confused with tests of runtime efficiency.
class DeltaEncodingCompressionQualityTest
    : public ::testing::TestWithParam<
          std::tuple<DeltaSignedness, uint64_t, uint64_t, uint64_t>> {
 public:
  DeltaEncodingCompressionQualityTest()
      : signedness_(std::get<0>(GetParam())),
        delta_max_bit_width_(std::get<1>(GetParam())),
        num_of_values_(std::get<2>(GetParam())),
        partial_random_seed_(std::get<3>(GetParam())) {
    MaybeSetSignedness(signedness_);
  }

  ~DeltaEncodingCompressionQualityTest() override = default;

  // Running with the same seed for all variants would make all tests start
  // with the same sequence; avoid this by making the seed different.
  uint64_t Seed() const {
    // Multiply everything but by different primes to produce unique results.
    return 2 * static_cast<uint64_t>(signedness_) + 3 * delta_max_bit_width_ +
           5 * delta_max_bit_width_ + 7 * num_of_values_ +
           11 * partial_random_seed_;
  }

  const DeltaSignedness signedness_;
  const uint64_t delta_max_bit_width_;
  const uint64_t num_of_values_;
  const uint64_t partial_random_seed_;  // Explained where it's used.
};

// If no wrap-around occurs in the stream, the width of the values does not
// matter to compression performance; only the deltas matter.
TEST_P(DeltaEncodingCompressionQualityTest,
       BaseDoesNotAffectEfficiencyIfNoWrapAround) {
  // 1. Bases which will not produce a wrap-around.
  // 2. The last base - 0xffffffffffffffff - does cause a wrap-around, but
  //    that still works, because the width is 64 anyway, and does not
  //    need to be conveyed explicitly in the encoding header.
  const uint64_t bases[] = {0, 0x55, 0xffffffff,
                            std::numeric_limits<uint64_t>::max()};
  const size_t kIntendedWrapAroundBaseIndex = arraysize(bases);

  std::vector<uint64_t> deltas(num_of_values_);

  // Allows us to make sure that the deltas do not produce a wrap-around.
  uint64_t last_element[arraysize(bases)];
  memcpy(last_element, bases, sizeof(bases));

  // Avoid empty `deltas` due to first element causing wrap-around.
  deltas[0] = 1;
  for (size_t i = 0; i < arraysize(last_element); ++i) {
    last_element[i] += 1;
  }

  Random prng(Seed());

  for (size_t i = 1; i < deltas.size(); ++i) {
    const uint64_t delta = RandomWithMaxBitWidth(&prng, delta_max_bit_width_);

    bool wrap_around = false;
    for (size_t j = 0; j < arraysize(last_element); ++j) {
      if (j == kIntendedWrapAroundBaseIndex) {
        continue;
      }

      last_element[j] += delta;
      if (last_element[j] < bases[j]) {
        wrap_around = true;
        break;
      }
    }

    if (wrap_around) {
      deltas.resize(i);
      break;
    }

    deltas[i] = delta;
  }

  std::string encodings[arraysize(bases)];

  for (size_t i = 0; i < arraysize(bases); ++i) {
    const auto values =
        CreateSequenceByDeltas(bases[i], deltas, num_of_values_);
    // Produce the encoding and write it to encodings[i].
    // By using TestEncodingAndDecoding() to do this, we also sanity-test
    // the encoding/decoding, though that is not the test's focus.
    TestEncodingAndDecoding(bases[i], values, &encodings[i]);
    EXPECT_LE(encodings[i].length(),
              EncodingLengthUpperBound(delta_max_bit_width_, num_of_values_,
                                       signedness_));
  }

  // Test focus - all of the encodings should be the same, as they are based
  // on the same delta sequence, and do not contain a wrap-around.
  for (size_t i = 1; i < arraysize(encodings); ++i) {
    EXPECT_EQ(encodings[i], encodings[0]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    SignednessOverrideAndDeltaMaxBitWidthAndNumberOfValuesInSequence,
    DeltaEncodingCompressionQualityTest,
    ::testing::Combine(
        ::testing::Values(DeltaSignedness::kNoOverride,
                          DeltaSignedness::kForceUnsigned,
                          DeltaSignedness::kForceSigned),
        ::testing::Values(1, 4, 8, 15, 16, 17, 31, 32, 33, 63, 64),
        ::testing::Values(1, 2, 100, 10000),
        ::testing::Values(11, 12, 13)));

// Similar to DeltaEncodingTest, but instead of semi-surgically producing
// specific cases, produce large amount of semi-realistic inputs.
class DeltaEncodingFuzzerLikeTest
    : public ::testing::TestWithParam<
          std::tuple<DeltaSignedness, uint64_t, uint64_t, bool, uint64_t>> {
 public:
  DeltaEncodingFuzzerLikeTest()
      : signedness_(std::get<0>(GetParam())),
        delta_max_bit_width_(std::get<1>(GetParam())),
        num_of_values_(std::get<2>(GetParam())),
        optional_values_(std::get<3>(GetParam())),
        partial_random_seed_(std::get<4>(GetParam())) {
    MaybeSetSignedness(signedness_);
  }

  ~DeltaEncodingFuzzerLikeTest() override = default;

  // Running with the same seed for all variants would make all tests start
  // with the same sequence; avoid this by making the seed different.
  uint64_t Seed() const {
    // Multiply everything but by different primes to produce unique results.
    return 2 * static_cast<uint64_t>(signedness_) + 3 * delta_max_bit_width_ +
           5 * delta_max_bit_width_ + 7 * num_of_values_ +
           11 * static_cast<uint64_t>(optional_values_) +
           13 * partial_random_seed_;
  }

  const DeltaSignedness signedness_;
  const uint64_t delta_max_bit_width_;
  const uint64_t num_of_values_;
  const bool optional_values_;
  const uint64_t partial_random_seed_;  // Explained where it's used.
};

TEST_P(DeltaEncodingFuzzerLikeTest, Test) {
  const std::optional<uint64_t> base(3432);

  Random prng(Seed());
  std::vector<std::optional<uint64_t>> deltas(num_of_values_);
  for (size_t i = 0; i < deltas.size(); ++i) {
    if (!optional_values_ || prng.Rand<bool>()) {
      deltas[i] = RandomWithMaxBitWidth(&prng, delta_max_bit_width_);
    }
  }
  const auto values =
      CreateSequenceByOptionalDeltas(base.value(), deltas, num_of_values_);

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_SUITE_P(
    SignednessOverrideAndDeltaMaxBitWidthAndNumberOfValuesInSequence,
    DeltaEncodingFuzzerLikeTest,
    ::testing::Combine(
        ::testing::Values(DeltaSignedness::kNoOverride,
                          DeltaSignedness::kForceUnsigned,
                          DeltaSignedness::kForceSigned),
        ::testing::Values(1, 4, 8, 15, 16, 17, 31, 32, 33, 63, 64),
        ::testing::Values(1, 2, 100, 10000),
        ::testing::Bool(),
        ::testing::Values(21, 22, 23)));

class DeltaEncodingSpecificEdgeCasesTest
    : public ::testing::TestWithParam<
          std::tuple<DeltaSignedness, uint64_t, bool>> {
 public:
  DeltaEncodingSpecificEdgeCasesTest() {
    UnsetFixedLengthEncoderDeltaSignednessForTesting();
  }

  ~DeltaEncodingSpecificEdgeCasesTest() override = default;
};

// This case is special because it produces identical forward/backward deltas.
TEST_F(DeltaEncodingSpecificEdgeCasesTest, SignedDeltaWithOnlyTopBitOn) {
  MaybeSetSignedness(DeltaSignedness::kForceSigned);

  const std::optional<uint64_t> base(3432);

  const uint64_t delta = static_cast<uint64_t>(1) << 63;
  const std::vector<std::optional<uint64_t>> values = {base.value() + delta};

  TestEncodingAndDecoding(base, values);
}

TEST_F(DeltaEncodingSpecificEdgeCasesTest, MaximumUnsignedDelta) {
  MaybeSetSignedness(DeltaSignedness::kForceUnsigned);

  const std::optional<uint64_t> base((static_cast<uint64_t>(1) << 63) + 0x123);

  const std::vector<std::optional<uint64_t>> values = {base.value() - 1};

  TestEncodingAndDecoding(base, values);
}

// Check that, if all deltas are set to -1, things still work.
TEST_P(DeltaEncodingSpecificEdgeCasesTest, ReverseSequence) {
  MaybeSetSignedness(std::get<0>(GetParam()));
  const uint64_t width = std::get<1>(GetParam());
  const bool wrap_around = std::get<2>(GetParam());

  const uint64_t value_mask = (width == 64)
                                  ? std::numeric_limits<uint64_t>::max()
                                  : ((static_cast<uint64_t>(1) << width) - 1);

  const uint64_t base = wrap_around ? 1u : (0xf82d3 & value_mask);
  const std::vector<std::optional<uint64_t>> values = {
      (base - 1u) & value_mask, (base - 2u) & value_mask,
      (base - 3u) & value_mask};

  TestEncodingAndDecoding(base, values);
}

INSTANTIATE_TEST_SUITE_P(
    _,
    DeltaEncodingSpecificEdgeCasesTest,
    ::testing::Combine(
        ::testing::Values(DeltaSignedness::kNoOverride,
                          DeltaSignedness::kForceUnsigned,
                          DeltaSignedness::kForceSigned),
        ::testing::Values(1, 4, 8, 15, 16, 17, 31, 32, 33, 63, 64),
        ::testing::Bool()));

}  // namespace
}  // namespace webrtc
