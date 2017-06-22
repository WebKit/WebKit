/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/basic_types.h"

#include "libyuv/compare_row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if ORIGINAL_OPT
uint32 HammingDistance_C(const uint8* src_a, const uint8* src_b, int count) {
  uint32 diff = 0u;

  int i;
  for (i = 0; i < count; ++i) {
    int x = src_a[i] ^ src_b[i];
    if (x & 1)
      ++diff;
    if (x & 2)
      ++diff;
    if (x & 4)
      ++diff;
    if (x & 8)
      ++diff;
    if (x & 16)
      ++diff;
    if (x & 32)
      ++diff;
    if (x & 64)
      ++diff;
    if (x & 128)
      ++diff;
  }
  return diff;
}
#endif

// Hakmem method for hamming distance.
uint32 HammingDistance_C(const uint8* src_a, const uint8* src_b, int count) {
  uint32 diff = 0u;

  int i;
  for (i = 0; i < count - 3; i += 4) {
    uint32 x = *((uint32*)src_a) ^ *((uint32*)src_b);
    uint32 u = x - ((x >> 1) & 0x55555555);
    u = ((u >> 2) & 0x33333333) + (u & 0x33333333);
    diff += ((((u + (u >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24);
    src_a += 4;
    src_b += 4;
  }
  return diff;
}

uint32 SumSquareError_C(const uint8* src_a, const uint8* src_b, int count) {
  uint32 sse = 0u;
  int i;
  for (i = 0; i < count; ++i) {
    int diff = src_a[i] - src_b[i];
    sse += (uint32)(diff * diff);
  }
  return sse;
}

// hash seed of 5381 recommended.
// Internal C version of HashDjb2 with int sized count for efficiency.
uint32 HashDjb2_C(const uint8* src, int count, uint32 seed) {
  uint32 hash = seed;
  int i;
  for (i = 0; i < count; ++i) {
    hash += (hash << 5) + src[i];
  }
  return hash;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
