//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MemoryTagging.h:
//    Memory Tagging Extension support. Memory that is mapped as taggable can be tagged, so
//    that the hardware traps out-of-bounds and use-after-free accesses to it. The tag is
//    part of the pointer, so a pointer to tagged memory must be untagged before it is
//    compared to a pointer obtained some other way.
//    The helpers are no-ops when memory tagging is not available.
//

#ifndef COMMON_MEMORYTAGGING_H_
#define COMMON_MEMORYTAGGING_H_

#include <stdint.h>

#include "common/log_utils.h"
#include "common/platform.h"
#include "common/span.h"

#ifdef ANGLE_PLATFORM_APPLE
#    if defined(__arm64e__)
#        if __has_include(<CoreFoundation/CFPriv.h>) // Matches USE_APPLE_INTERNAL_SDK in WTF.
#            include <AppleFeatures/AppleFeatures.h>
#            if defined(APPLE_FEATURE_MTE) && APPLE_FEATURE_MTE
#                define ANGLE_ENABLE_MEMORY_TAGGING
#            endif
#        endif
#    endif
#endif

#if defined(ANGLE_ENABLE_MEMORY_TAGGING)
#    include <libproc.h>
#    include <mach/mach_init.h>
#    include <mach/mach_vm.h>
#    include <mach/vm_statistics.h>
#    include <sys/errno.h>
#    include <sys/mman.h>
#    include <unistd.h>
#endif

namespace angle
{

#if defined(ANGLE_ENABLE_MEMORY_TAGGING)

constexpr size_t kMemoryTagGranuleSize = 16;

struct MemoryTaggingState
{
    bool isEnabled;
    size_t pageMask;
};

inline const MemoryTaggingState &GetMemoryTaggingState()
{
    static const MemoryTaggingState state = []() {
        struct proc_bsdinfo info;
        int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
        constexpr uint32_t kProcFlagMTESecEnabled = 0x4000000;
        return MemoryTaggingState{
            rc == static_cast<int>(sizeof(info)) && (info.pbi_flags & kProcFlagMTESecEnabled) != 0,
            static_cast<size_t>(sysconf(_SC_PAGESIZE) - 1)
        };
    }();
    return state;
}

inline void *UntagPointer(void *ptr)
{
    constexpr uintptr_t kTagMask = 0x0f00000000000000ull;
    return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) & ~kTagMask);
}

inline Span<uint8_t> TagMemory(Span<uint8_t> memory)
{
    ASSERT(memory.size() % kMemoryTagGranuleSize == 0);
    ASSERT(reinterpret_cast<uintptr_t>(memory.data()) % kMemoryTagGranuleSize == 0);
    if (!GetMemoryTaggingState().isEnabled)
    {
        return memory;
    }
    // Generate a random tag for the pointer, excluding tag 0, and store it for the first
    // granule.
    uintptr_t excludedTags = 1;
    uint8_t *tagged        = memory.data();
    asm volatile(".arch_extension memtag\n\t"
                 "irg %0, %0, %1\n\t"
                 "stg %0, [%0]"
                 : "+r"(tagged)
                 : "r"(excludedTags));
    // Store the tag for the rest of the granules, two granules at a time. Start at a
    // multiple of two granules from the end, which retags the first granule when needed.
    constexpr size_t kTagStoreSize = 2 * kMemoryTagGranuleSize;
    uint8_t *it                    = tagged + (memory.size() % kTagStoreSize);
    uint8_t *const end             = tagged + memory.size();
    while (it < end)
    {
        asm volatile(".arch_extension memtag\n\t"
                     "st2g %0, [%0], #32"
                     : "+r"(it));
    }
#    if defined(ANGLE_ENABLE_ASSERTS)
    void *storedTag = tagged;
    asm volatile(".arch_extension memtag\n\t"
                 "ldg %0, [%0]"
                 : "+r"(storedTag));
    ASSERT(storedTag != UntagPointer(storedTag));
#    endif
    return {tagged, memory.size()};
}

inline Span<uint8_t> AllocateTaggableMemory(size_t size)
{
    constexpr vm_inherit_t kChildProcessInheritance = VM_INHERIT_DEFAULT;
    constexpr boolean_t kCopy                       = false;
    constexpr vm_prot_t kProtections                = VM_PROT_WRITE | VM_PROT_READ;
    constexpr int kVMFlagsMTE                       = 0x2000;
    const MemoryTaggingState &state                 = GetMemoryTaggingState();
    int flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_TCMALLOC);
    if (state.isEnabled)
    {
        flags |= kVMFlagsMTE;
    }
    mach_vm_address_t address = 0;
    kern_return_t result = mach_vm_map(mach_task_self(), &address, size, state.pageMask, flags,
                                       MEMORY_OBJECT_NULL, 0, kCopy, kProtections, kProtections,
                                       kChildProcessInheritance);
    if (ANGLE_UNLIKELY(result != KERN_SUCCESS))
    {
        errno = 0;
        return {};
    }
    return {reinterpret_cast<uint8_t *>(address), size};
}

inline void FreeTaggableMemory(void *memory, size_t size)
{
    munmap(UntagPointer(memory), size);
}

#else

inline void *UntagPointer(void *ptr)
{
    return ptr;
}

inline Span<uint8_t> TagMemory(Span<uint8_t> memory)
{
    return memory;
}

#endif

}  // namespace angle

#endif  // COMMON_MEMORYTAGGING_H_
