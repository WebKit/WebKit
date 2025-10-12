/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_ALLOCATION_COUNTER_H_
#define COMMON_AUDIO_ALLOCATION_COUNTER_H_

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// The allocator override mechanism is not available since the
// sanitizers override the allocators themselves.
#else
#define WEBRTC_ALLOCATION_COUNTER_AVAILABLE 1

#include <cstddef>

namespace webrtc {

// Use to count the number of heap allocations that have been performed on the
// current thread within the scope of the AllocationCounter.
//
// * Note1: This class is a test-only utility.  In order to be able to count
//   allocations, AllocationCounter overrides the global new and delete
//   operators for the test binary.
//
// * Note2: An AllocationCounter instance must always be used from the same
//   thread.
class AllocationCounter {
 public:
  AllocationCounter();
  ~AllocationCounter() = default;

  // Returns the number of heap allocations that have been made since
  // construction.
  size_t new_count() const;
  size_t delete_count() const;

 private:
  const size_t initial_new_count_;
  const size_t initial_delete_count_;
};
}  // namespace webrtc

#endif  // all the sanitizers
#endif  // COMMON_AUDIO_ALLOCATION_COUNTER_H_
