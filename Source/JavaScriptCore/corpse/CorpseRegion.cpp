/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#if ENABLE(MYA)

#include "CorpseError.h"
#include "CorpseLimits.h"

#if OS(DARWIN)
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#else
#include "CorpseProcess.h"
#include <charconv>
#include <unistd.h>
#endif

namespace JSC {
namespace Corpse {

#if OS(DARWIN)

uint64_t Region::pageCount() const
{
    return vm_kernel_page_size ? m_size / vm_kernel_page_size : 0;
}

// KERN_INVALID_ADDRESS means there is no region at or above `address`.
kern_return_t Region::atOrAbove(mach_port_t task, Address address, Region& region)
{
    mach_vm_address_t regionAddress = 0;
    mach_vm_size_t regionSize = 0;
    vm_region_submap_info_data_64_t info;
    for (natural_t depth = 0; ; ++depth) {
        regionAddress = address.toTargetVMAddress();
        regionSize = 0;
        natural_t depthLimit = depth; // We tell the kernel how deep we want to go. Kernel tells us how deep it can go.
        mach_msg_type_number_t infoCount = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = mach_vm_region_recurse(task, &regionAddress, &regionSize,
            &depthLimit, reinterpret_cast<vm_region_recurse_info_t>(&info), &infoCount);
        if (kr != KERN_SUCCESS)
            return kr;
        if (!info.is_submap)
            break;
    }

    region.m_base = Address(regionAddress);
    region.m_size = static_cast<size_t>(regionSize);
    region.m_residentPageCount = info.pages_resident;
    region.m_dirtyPageCount = info.pages_dirtied;
    return KERN_SUCCESS;
}

std::optional<Region> Region::findContaining(mach_port_t task, Address address)
{
    Region region;
    if (atOrAbove(task, address, region) != KERN_SUCCESS || !region.contains(address))
        return std::nullopt;
    return region;
}

Vector<Region> Region::all(mach_port_t task)
{
    Vector<Region> result;
    Region region;
    kern_return_t kr;
    while ((kr = atOrAbove(task, result.isEmpty() ? Address { } : result.last().end(), region)) == KERN_SUCCESS) {
        if (result.size() >= maxRegionCount) {
            CORPSE_REPORT("The task lists more than %zu regions", maxRegionCount);
            return { };
        }
        result.append(region);
    }
    if (kr != KERN_INVALID_ADDRESS) {
        CORPSE_REPORT("Could not list the regions of the task: %s (0x%x)", mach_error_string(kr), kr);
        return { };
    }
    return result;
}

#else

uint64_t Region::pageCount() const
{
    return m_size / static_cast<size_t>(getpagesize());
}

std::optional<Region> Region::findContaining(TaskHandle pid, Address address)
{
    for (Region& region : all(pid)) {
        if (region.contains(address))
            return WTF::move(region);
    }
    return std::nullopt;
}

// One line of /proc/<pid>/maps: "start-end perms offset dev inode   name". See proc_pid_maps(5).
std::optional<Region> Region::parseMapsLine(std::string_view line)
{
    auto field = [&] {
        size_t start = line.find_first_not_of(' ');
        if (start == std::string_view::npos)
            return std::string_view { };
        line.remove_prefix(start);
        size_t end = std::min(line.find(' '), line.size());
        std::string_view result = line.substr(0, end);
        line.remove_prefix(end);
        return result;
    };
    auto parse = [](std::string_view text, uint64_t& value, int base) {
        return !text.empty() && std::from_chars(text.data(), text.data() + text.size(), value, base).ec == std::errc { };
    };
    std::string_view range = field();
    std::string_view permissions = field();
    std::string_view offset = field();
    field(); // Device.
    std::string_view inode = field();
    size_t nameStart = line.find_first_not_of(' ');

    size_t dash = range.find('-');
    uint64_t start = 0;
    uint64_t end = 0;
    Region region;
    if (dash == std::string_view::npos || !parse(range.substr(0, dash), start, 16) || !parse(range.substr(dash + 1), end, 16)
        || end <= start || permissions.size() != 4 || !parse(offset, region.m_fileOffset, 16) || !parse(inode, region.m_inode, 10))
        return std::nullopt;
    region.m_base = Address { start };
    region.m_size = static_cast<size_t>(end - start);
    region.m_isExecutable = permissions[2] == 'x';
    if (nameStart != std::string_view::npos)
        region.m_name = String::fromUTF8(std::span { line.substr(nameStart) });
    return region;
}

Vector<Region> Region::all(TaskHandle pid)
{
    auto maps = readProcFile(pid, "maps");
    if (!maps) {
        CORPSE_REPORT("Could not read the memory map of pid %d", pid);
        return { };
    }
    Vector<Region> result;
    std::string_view rest { *maps };
    while (!rest.empty()) {
        size_t lineEnd = std::min(rest.find('\n'), rest.size());
        std::string_view line = rest.substr(0, lineEnd);
        auto region = parseMapsLine(line);
        if (!region) {
            CORPSE_REPORT("Could not parse the line '%.*s' of the memory map of pid %d", static_cast<int>(line.size()), line.data(), pid);
            return { };
        }
        if (result.size() >= maxRegionCount) {
            CORPSE_REPORT("The memory map of pid %d lists more than %zu regions", pid, maxRegionCount);
            return { };
        }
        result.append(WTF::move(*region));
        rest.remove_prefix(std::min(lineEnd + 1, rest.size()));
    }
    return result;
}

#endif // OS(DARWIN)

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
