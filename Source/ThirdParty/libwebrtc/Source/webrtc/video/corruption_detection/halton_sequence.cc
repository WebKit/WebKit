/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/halton_sequence.h"

#include <algorithm>
#include <vector>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

static constexpr int kMaxDimensions = 5;
const int kBases[kMaxDimensions] = {2, 3, 5, 7, 11};

double GetVanDerCorputSequenceElement(int sequence_idx, int base) {
  if (sequence_idx < 0 || base < 2) {
    sequence_idx = 0;
    base = 2;
  }
  double element = 0.0;
  double positional_value = 1.0;
  int left = sequence_idx;
  while (left > 0) {
    positional_value /= base;
    element += positional_value * (left % base);
    left /= base;
  }
  return element;
}

}  // namespace

HaltonSequence::HaltonSequence(int num_dimensions)
    : num_dimensions_(num_dimensions), current_idx_(0) {
  RTC_CHECK_GE(num_dimensions_, 1)
      << "num_dimensions must be >= 1. Will be set to 1.";
  RTC_CHECK_LE(num_dimensions_, kMaxDimensions)
      << "num_dimensions must be <= " << kMaxDimensions << ". Will be set to "
      << kMaxDimensions << ".";
  num_dimensions_ = std::clamp(num_dimensions_, 1, kMaxDimensions);
}

std::vector<double> HaltonSequence::GetNext() {
  std::vector<double> point = {};
  point.reserve(num_dimensions_);
  for (int i = 0; i < num_dimensions_; ++i) {
    point.push_back(GetVanDerCorputSequenceElement(current_idx_, kBases[i]));
  }
  ++current_idx_;
  return point;
}

void HaltonSequence::SetCurrentIndex(int idx) {
  if (idx >= 0) {
    current_idx_ = idx;
  }
  RTC_DCHECK_GE(idx, 0) << "Index must be non-negative";
}

void HaltonSequence::Reset() {
  HaltonSequence::current_idx_ = 0;
}

}  // namespace webrtc
