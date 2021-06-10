/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/symmetric_matrix_buffer.h"

#include "modules/audio_processing/agc2/rnn_vad/ring_buffer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

template <typename T, int S>
void CheckSymmetry(const SymmetricMatrixBuffer<T, S>* sym_matrix_buf) {
  for (int row = 0; row < S - 1; ++row)
    for (int col = row + 1; col < S; ++col)
      EXPECT_EQ(sym_matrix_buf->GetValue(row, col),
                sym_matrix_buf->GetValue(col, row));
}

using PairType = std::pair<int, int>;

// Checks that the symmetric matrix buffer contains any pair with a value equal
// to the given one.
template <int S>
bool CheckPairsWithValueExist(
    const SymmetricMatrixBuffer<PairType, S>* sym_matrix_buf,
    const int value) {
  for (int row = 0; row < S - 1; ++row) {
    for (int col = row + 1; col < S; ++col) {
      auto p = sym_matrix_buf->GetValue(row, col);
      if (p.first == value || p.second == value)
        return true;
    }
  }
  return false;
}

// Test that shows how to combine RingBuffer and SymmetricMatrixBuffer to
// efficiently compute pair-wise scores. This test verifies that the evolution
// of a SymmetricMatrixBuffer instance follows that of RingBuffer.
TEST(RnnVadTest, SymmetricMatrixBufferUseCase) {
  // Instance a ring buffer which will be fed with a series of integer values.
  constexpr int kRingBufSize = 10;
  RingBuffer<int, 1, kRingBufSize> ring_buf;
  // Instance a symmetric matrix buffer for the ring buffer above. It stores
  // pairs of integers with which this test can easily check that the evolution
  // of RingBuffer and SymmetricMatrixBuffer match.
  SymmetricMatrixBuffer<PairType, kRingBufSize> sym_matrix_buf;
  for (int t = 1; t <= 100; ++t) {  // Evolution steps.
    SCOPED_TRACE(t);
    const int t_removed = ring_buf.GetArrayView(kRingBufSize - 1)[0];
    ring_buf.Push({&t, 1});
    // The head of the ring buffer is |t|.
    ASSERT_EQ(t, ring_buf.GetArrayView(0)[0]);
    // Create the comparisons between |t| and the older elements in the ring
    // buffer.
    std::array<PairType, kRingBufSize - 1> new_comparions;
    for (int i = 0; i < kRingBufSize - 1; ++i) {
      // Start comparing |t| to the second newest element in the ring buffer.
      const int delay = i + 1;
      const auto t_prev = ring_buf.GetArrayView(delay)[0];
      ASSERT_EQ(std::max(0, t - delay), t_prev);
      // Compare the last element |t| with |t_prev|.
      new_comparions[i].first = t_prev;
      new_comparions[i].second = t;
    }
    // Push the new comparisons in the symmetric matrix buffer.
    sym_matrix_buf.Push({new_comparions.data(), new_comparions.size()});
    // Tests.
    CheckSymmetry(&sym_matrix_buf);
    // Check that the pairs resulting from the content in the ring buffer are
    // in the right position.
    for (int delay1 = 0; delay1 < kRingBufSize - 1; ++delay1) {
      for (int delay2 = delay1 + 1; delay2 < kRingBufSize; ++delay2) {
        const auto t1 = ring_buf.GetArrayView(delay1)[0];
        const auto t2 = ring_buf.GetArrayView(delay2)[0];
        ASSERT_LE(t2, t1);
        const auto p = sym_matrix_buf.GetValue(delay1, delay2);
        EXPECT_EQ(p.first, t2);
        EXPECT_EQ(p.second, t1);
      }
    }
    // Check that every older element in the ring buffer still has a
    // corresponding pair in the symmetric matrix buffer.
    for (int delay = 1; delay < kRingBufSize; ++delay) {
      const auto t_prev = ring_buf.GetArrayView(delay)[0];
      EXPECT_TRUE(CheckPairsWithValueExist(&sym_matrix_buf, t_prev));
    }
    // Check that the element removed from the ring buffer has no corresponding
    // pairs in the symmetric matrix buffer.
    if (t > kRingBufSize - 1) {
      EXPECT_FALSE(CheckPairsWithValueExist(&sym_matrix_buf, t_removed));
    }
  }
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
