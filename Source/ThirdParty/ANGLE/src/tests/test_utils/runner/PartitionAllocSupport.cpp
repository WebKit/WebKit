//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PartitionAllocSupport.cpp:
//   Support functions for PartitionAlloc integration in ANGLE tests.
//

#include "PartitionAllocSupport.h"

#include "common/debug.h"

#if defined(ANGLE_USE_PARTITION_ALLOC)
#    include <partition_alloc/buildflags.h>
#    if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#        include <partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h>
#        if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
#            include <partition_alloc/dangling_raw_ptr_checks.h>
#        endif
#    endif
#endif

namespace angle
{

void InitializePartitionAllocForTesting()
{
#if defined(ANGLE_USE_PARTITION_ALLOC)
#    if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    allocator_shim::ConfigurePartitionsForTesting();
#    endif
#endif
}

// Installs dangling pointer callbacks. In ANGLE standalone, this aborts the test
// runner with a fatal crash when a dangling pointer is released.
//
// TODO(b/503180635): To help developers analyze dangling pointer issues, record
// stack traces in both SetDanglingRawPtrDetectedFn (when memory is freed) and
// SetDanglingRawPtrReleasedFn (when the raw_ptr is released), and join them by
// the dangling pointer ID to display both call stacks together on failure.
void InitializeDanglingPointerDetectorForTesting()
{
#if defined(ANGLE_USE_PARTITION_ALLOC)
#    if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#        if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
    // Invoked when memory is freed while a raw_ptr still references it.
    // Developers can place a breakpoint here in a debugger to display the call
    // stack where the pointed-to object was freed.
    partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t id) {
        // Kept empty by default. Place a breakpoint here to debug free sites.
    });

    // Invoked when a dangling raw_ptr is released (reassigned, cleared, or destructed).
    partition_alloc::SetDanglingRawPtrReleasedFn([](uintptr_t id) {
        FATAL() << "DanglingPointerDetector: A pointer was dangling!\n"
                << "See doc/DanglingPointerDetector.md for details:\n"
                << "https://chromium.googlesource.com/angle/angle/+/main/doc/"
                   "DanglingPointerDetector.md";
    });
#        endif
#    endif
#endif
}

}  // namespace angle
