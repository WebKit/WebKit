/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/allocation_counter.h"

#if defined(WEBRTC_ALLOCATION_COUNTER_AVAILABLE)

#include <cstddef>
#include <cstdlib>
#include <vector>

#include "absl/base/attributes.h"

// FIXME: Comment header to avoid building gtest.cc, which is not included as part of libwebrtc sources.
// #include "test/gtest.h"

namespace {
#if defined(ABSL_HAVE_THREAD_LOCAL)
ABSL_CONST_INIT thread_local size_t g_new_count = 0u;
ABSL_CONST_INIT thread_local size_t g_delete_count = 0u;
#elif defined(WEBRTC_POSIX)
#error Handle WEBRTC_POSIX
#else
#error Unsupported platform
#endif
}  // namespace

void* operator new(size_t s) {
  ++g_new_count;
  return malloc(s);
}

void* operator new[](size_t s) {
  ++g_new_count;
  return malloc(s);
}

void operator delete(void* p) throw() {
  ++g_delete_count;
  return free(p);
}

void operator delete[](void* p) throw() {
  ++g_delete_count;
  return free(p);
}

namespace webrtc {

AllocationCounter::AllocationCounter()
    : initial_new_count_(g_new_count), initial_delete_count_(g_delete_count) {}

size_t AllocationCounter::new_count() const {
  return g_new_count - initial_new_count_;
}

size_t AllocationCounter::delete_count() const {
  return g_delete_count - initial_delete_count_;
}

// FIXME: See comment related to gtest.cc.
/*
TEST(AllocationCounterTest, CountsHeapAllocations) {
  std::vector<int> v;
  AllocationCounter counter;
  EXPECT_EQ(counter.new_count(), 0u);
  EXPECT_EQ(counter.delete_count(), 0u);
  v.resize(1000);
  EXPECT_EQ(counter.new_count(), 1u);
  EXPECT_EQ(counter.delete_count(), 0u);
  v.clear();
  v.shrink_to_fit();
  EXPECT_EQ(counter.new_count(), 1u);
  EXPECT_EQ(counter.delete_count(), 1u);
}
*/

}  // namespace webrtc

#endif  // defined(WEBRTC_ALLOCATION_COUNTER_AVAILABLE)
