/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/acm_random.h"

using libaom_test::ACMRandom;

extern "C" {
int av1_neg_interleave(int x, int ref, int max);
int av1_neg_deinterleave(int diff, int ref, int max);
}

namespace {

struct Segment {
  int id;
  int pred;
  int last_id;
};

Segment GenerateSegment(int seed) {
  static const int MAX_SEGMENTS = 8;

  ACMRandom rnd_(seed);

  Segment segment;
  const int last_segid = rnd_.PseudoUniform(MAX_SEGMENTS);
  segment.last_id = last_segid;
  segment.pred = rnd_.PseudoUniform(MAX_SEGMENTS);
  segment.id = rnd_.PseudoUniform(last_segid + 1);

  return segment;
}

// Try to reveal a mismatch between segment binarization and debinarization
TEST(SegmentBinarizationSync, SearchForBinarizationMismatch) {
  const int count_tests = 1000;
  const int seed_init = 4321;

  for (int i = 0; i < count_tests; ++i) {
    const Segment seg = GenerateSegment(seed_init + i);

    const int max_segid = seg.last_id + 1;
    const int seg_diff = av1_neg_interleave(seg.id, seg.pred, max_segid);
    const int decoded_segid =
        av1_neg_deinterleave(seg_diff, seg.pred, max_segid);

    ASSERT_EQ(decoded_segid, seg.id);
  }
}

}  // namespace
