/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CorpseRegion.h"

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <mach/mach_vm.h>

namespace JSC {
namespace Corpse {

uint64_t Region::pageCount() const
{
    return vm_kernel_page_size ? m_size / vm_kernel_page_size : 0;
}

std::optional<Region> Region::findContaining(mach_port_t task, Address address)
{
    // mach_vm_region_recurse reports the region at or above the address it is given,
    // so the result only describes `address` if it turns out to contain it.
    mach_vm_address_t regionAddress = 0;
    mach_vm_size_t regionSize = 0;
    vm_region_submap_info_data_64_t info;
    for (natural_t depth = 0; ; ++depth) {
        regionAddress = address.toMachVMAddress();
        regionSize = 0;
        natural_t depthLimit = depth; // We tell the kernel how deep we want to go. Kernel tells us how deep it can go.
        mach_msg_type_number_t infoCount = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = mach_vm_region_recurse(task, &regionAddress, &regionSize,
            &depthLimit, reinterpret_cast<vm_region_recurse_info_t>(&info), &infoCount);
        if (kr != KERN_SUCCESS)
            return std::nullopt;
        if (!info.is_submap)
            break;
    }

    Region region;
    region.m_base = Address(regionAddress);
    region.m_size = static_cast<size_t>(regionSize);
    if (!region.contains(address))
        return std::nullopt;

    region.m_residentPageCount = info.pages_resident;
    region.m_dirtyPageCount = info.pages_dirtied;
    return region;
}

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
