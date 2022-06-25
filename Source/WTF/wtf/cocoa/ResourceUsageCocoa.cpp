/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/ResourceUsage.h>

#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <utility>
#include <wtf/spi/cocoa/MachVMSPI.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WTF {

size_t vmPageSize()
{
#if PLATFORM(IOS_FAMILY)
    return vm_kernel_page_size;
#else
    static size_t cached = sysconf(_SC_PAGESIZE);
    return cached;
#endif
}

void logFootprintComparison(const std::array<TagInfo, 256>& before, const std::array<TagInfo, 256>& after)
{
    const size_t pageSize = vmPageSize();

    WTFLogAlways("Per-tag breakdown of memory reclaimed by pressure handler:");
    WTFLogAlways("  ## %16s %10s %10s %10s", "VM Tag", "Before", "After", "Diff");
    for (unsigned i = 0; i < 256; ++i) {
        ssize_t dirtyBefore = before[i].dirty * pageSize;
        ssize_t dirtyAfter = after[i].dirty * pageSize;
        ssize_t dirtyDiff = dirtyAfter - dirtyBefore;
        if (!dirtyBefore && !dirtyAfter)
            continue;
        String tagName = displayNameForVMTag(i);
        if (!tagName)
            tagName = makeString("Tag ", i);
        WTFLogAlways("  %02X %16s %10ld %10ld %10ld",
            i,
            tagName.ascii().data(),
            dirtyBefore,
            dirtyAfter,
            dirtyDiff
        );
    }
}

ASCIILiteral displayNameForVMTag(unsigned tag)
{
    switch (tag) {
    case VM_MEMORY_CGIMAGE: return "CG image"_s;
    case VM_MEMORY_COREGRAPHICS_DATA: return "CG raster data"_s;
    case VM_MEMORY_CORESERVICES: return "CoreServices"_s;
    case VM_MEMORY_DYLIB: return "dylib"_s;
    case VM_MEMORY_FOUNDATION: return "Foundation"_s;
    case VM_MEMORY_IMAGEIO: return "ImageIO"_s;
    case VM_MEMORY_IOACCELERATOR: return "IOAccelerator"_s;
    case VM_MEMORY_IOSURFACE: return "IOSurface"_s;
    case VM_MEMORY_IOKIT: return "IOKit"_s;
    case VM_MEMORY_JAVASCRIPT_CORE: return "Gigacage"_s;
    case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR: return "JSC JIT"_s;
    case VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE: return "IsoHeap"_s;
    case VM_MEMORY_LAYERKIT: return "CoreAnimation"_s;
    case VM_MEMORY_LIBDISPATCH: return "libdispatch"_s;
    case VM_MEMORY_MALLOC: return "malloc"_s;
    case VM_MEMORY_MALLOC_HUGE: return "malloc (huge)"_s;
    case VM_MEMORY_MALLOC_LARGE: return "malloc (large)"_s;
    case VM_MEMORY_MALLOC_MEDIUM: return "malloc (medium)"_s;
    case VM_MEMORY_MALLOC_NANO: return "malloc (nano)"_s;
    case VM_MEMORY_MALLOC_SMALL: return "malloc (small)"_s;
    case VM_MEMORY_MALLOC_TINY: return "malloc (tiny)"_s;
    case VM_MEMORY_OS_ALLOC_ONCE: return "os_alloc_once"_s;
    case VM_MEMORY_SQLITE: return "SQLite"_s;
    case VM_MEMORY_STACK: return "Stack"_s;
    case VM_MEMORY_TCMALLOC: return "bmalloc"_s;
    case VM_MEMORY_UNSHARED_PMAP: return "pmap (unshared)"_s;
    default: return { };
    }
}

std::array<TagInfo, 256> pagesPerVMTag()
{
    std::array<TagInfo, 256> tags;
    task_t task = mach_task_self();
    mach_vm_size_t size;
    uint32_t depth = 0;
    struct vm_region_submap_info_64 info = { };
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    for (mach_vm_address_t addr = 0; ; addr += size) {
        int purgeableState;
        if (mach_vm_purgable_control(task, addr, VM_PURGABLE_GET_STATE, &purgeableState) != KERN_SUCCESS)
            purgeableState = VM_PURGABLE_DENY;

        kern_return_t kr = mach_vm_region_recurse(task, &addr, &size, &depth, (vm_region_info_t)&info, &count);
        if (kr != KERN_SUCCESS)
            break;

        tags[info.user_tag].regionCount++;

        tags[info.user_tag].reserved += size / vmPageSize();

        if (purgeableState == VM_PURGABLE_VOLATILE) {
            tags[info.user_tag].reclaimable += info.pages_resident;
            continue;
        }

        if (purgeableState == VM_PURGABLE_EMPTY) {
            tags[info.user_tag].reclaimable += size / vmPageSize();
            continue;
        }

        // This calculates the footprint of a VM region in a manner similar to what footprint(1) does in the legacy --vmObjectDirty mode.
        bool anonymous = !info.external_pager;
        if (anonymous) {
            tags[info.user_tag].dirty += info.pages_resident - info.pages_reusable + info.pages_swapped_out;
            tags[info.user_tag].reclaimable += info.pages_reusable;
        } else
            tags[info.user_tag].dirty += info.pages_dirtied + info.pages_swapped_out;
    }

    return tags;
}

}
