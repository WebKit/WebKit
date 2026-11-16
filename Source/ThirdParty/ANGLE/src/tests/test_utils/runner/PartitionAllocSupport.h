//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PartitionAllocSupport.h:
//   Support functions for PartitionAlloc integration in ANGLE tests.
//

#ifndef TESTS_TEST_UTILS_RUNNER_PARTITION_ALLOC_SUPPORT_H_
#define TESTS_TEST_UTILS_RUNNER_PARTITION_ALLOC_SUPPORT_H_

namespace angle
{

// Configures PartitionAlloc's allocator for testing. This is a no-op unless
// PartitionAlloc-as-malloc is available and enabled (ANGLE_USE_PARTITION_ALLOC
// and USE_PARTITION_ALLOC_AS_MALLOC).
void InitializePartitionAllocForTesting();

// Installs handlers causing a crash when a raw_ptr<T> becomes dangling.
// This is a no-op unless PartitionAlloc dangling pointer checks are available
// and enabled (ENABLE_DANGLING_RAW_PTR_CHECKS).
void InitializeDanglingPointerDetectorForTesting();

}  // namespace angle

#endif  // TESTS_TEST_UTILS_RUNNER_PARTITION_ALLOC_SUPPORT_H_
