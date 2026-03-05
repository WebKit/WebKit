/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <deque>
#include "absl/algorithm/container.h"
#include "rtc_base/numerics/windowed_min_filter.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  class ReferenceFilter {
   public:
    explicit ReferenceFilter(int window_length) : max_size_(window_length) {}
    void Insert(int value) {
      buffer_.push_back(value);
      if (buffer_.size() > max_size_) {
        buffer_.pop_front();
      }
    }
    int GetMin() const { return *absl::c_min_element(buffer_); }

   private:
    const size_t max_size_;
    std::deque<int> buffer_;
  };

  ReferenceFilter reference_filter(/*window_length=*/10);
  WindowedMinFilter<int> filter(/*window_length=*/10);
  test::FuzzDataHelper fuzz_data(MakeArrayView(data, size));

  while (fuzz_data.CanReadBytes(sizeof(int))) {
    int value = fuzz_data.Read<int>();
    reference_filter.Insert(value);
    filter.Insert(value);
    RTC_CHECK_EQ(filter.GetMin(), reference_filter.GetMin());
  }
}
}  // namespace webrtc
