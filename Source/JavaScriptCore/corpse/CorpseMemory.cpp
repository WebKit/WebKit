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
#include "CorpseMemory.h"

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include "CorpseMachVMSPI.h"

#include <limits>
#include <mach/mach.h>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Memory::Region);

Memory::MapResult Memory::Region::create(Memory& memory, Address remoteBase, size_t pageCount, size_t offset)
{
    mach_vm_address_t target = 0;

    vm_prot_t currentProtection = VM_PROT_READ;
    vm_prot_t maximumProtection = VM_PROT_READ;
    boolean_t copyTheMemory = false; // i.e. share the corpse's pages instead of copying into new pages for this process.
    kern_return_t kr = mach_vm_remap_new(mach_task_self(), &target, pageCount * pageSize(), 0, VM_FLAGS_ANYWHERE,
        memory.m_corpsePort, remoteBase.toMachVMAddress(), copyTheMemory, &currentProtection, &maximumProtection, VM_INHERIT_NONE);

    if (kr != KERN_SUCCESS) {
        Error error;
        switch (kr) {
        case KERN_INVALID_ADDRESS:
            error = Error::UnmappedHole; // Region includes holes or unmapped pages.
            break;
        case KERN_PROTECTION_FAILURE:
            error = Error::NotReadable;
            break;
        case KERN_NO_SPACE:
            error = Error::OutOfAddressSpace;
            break;
        default:
            error = Error::Unknown;
            break;
        }
        return MapResult { .error = error, .kernResult = kr };
    }

    auto* localBase = reinterpret_cast<const uint8_t*>(target);
    RefPtr<Region> region = adoptRef(new Region(memory, remoteBase, pageCount, localBase));
    return MapResult { WTF::move(region), localBase + offset };
}

Memory::Region::Region(Memory& memory, Address remoteBase, size_t pageCount, const uint8_t* localBase)
    : m_memory(memory)
    , m_remoteBase(remoteBase)
    , m_pageCount(pageCount)
    , m_localBase(localBase)
{
}

Memory::Region::~Region()
{
    auto localBase = reinterpret_cast<mach_vm_address_t>(m_localBase);
    mach_vm_deallocate(mach_task_self(), localBase, m_pageCount * pageSize());
}

void Memory::Region::deref()
{
    RELEASE_ASSERT(m_refCount);
    if (--m_refCount > 1)
        return;

    // Currently, the policy is to immediately release this Region when there are no more
    // clients ref'ing it. We reserve the option to change this policy in the future.
    if (m_refCount == 1) {
        // removeRegion() will deref() this. We don't want it to get down to this path and
        // recurses into removeRegion() again. Bumping the refCount up by at least 2 blocks
        // that, and allows us to handle the deletion ourselves below when ready.
        m_refCount += 2;
        m_memory.removeRegion(this);
        m_refCount = 0; // Now ready to delete.
    }

    delete this;
}

size_t Memory::pageSize()
{
    return vm_kernel_page_size;
}

Memory::Memory(mach_port_t corpsePort)
    : m_corpsePort(corpsePort)
{
    // Placed here so we can assert this invariant only once on construction.
    size_t page = pageSize();
    RELEASE_ASSERT(page && !(page & (page - 1))); // The masking below needs a power of two.
}

Memory::~Memory()
{
    for (auto& entry : m_regions) {
        for (auto& region : entry.value)
            RELEASE_ASSERT(region->refCount() == 1);
    }
}

Memory::MapResult Memory::map(Address objAddress, size_t objSizeInBytes)
{
    if (!objSizeInBytes)
        return MapResult { .error = Error::InvalidRequest };

    mach_vm_address_t objBase = objAddress.toMachVMAddress();
    mach_vm_address_t objEnd = objBase + objSizeInBytes;
    if (objEnd < objBase)
        return MapResult { .error = Error::InvalidRequest }; // objEnd overflows.

    size_t page = pageSize();
    mach_vm_address_t pageMask = static_cast<mach_vm_address_t>(page) - 1;

    if (objEnd > std::numeric_limits<mach_vm_address_t>::max() - pageMask)
        return MapResult { .error = Error::InvalidRequest };

    // Compute Region that object resides in.
    mach_vm_address_t regionBase = objBase & ~pageMask;
    mach_vm_address_t regionEnd = (objEnd + pageMask) & ~pageMask;

    // Reject "Page 0". A null Address is reserved as the empty value in the m_regions map.
    if (!regionBase)
        return MapResult { .error = Error::InvalidRequest };

    // Find an existing page aligned Region that fits this request if possible.
    Address regionBaseAddress { regionBase };
    size_t pageCount = static_cast<size_t>((regionEnd - regionBase) / page);
    size_t offset = static_cast<size_t>(objBase - regionBase);

    auto iterator = m_regions.find(regionBaseAddress);
    if (iterator != m_regions.end()) {
        // Ordered by ascending pageCount. Find the smallest (i.e. first) one that fits.
        for (auto& region : iterator->value) {
            if (region->pageCount() >= pageCount)
                return MapResult { region, region->localBase() + offset };
        }
    }

    // No existing Region fits, so map a new one.
    auto result = Region::create(*this, regionBaseAddress, pageCount, offset);
    if (!result.region)
        return result;

    addRegion(result.region);
    return result;
}

void Memory::addRegion(RefPtr<Region> region)
{
    auto& regions = m_regions.add(region->remoteBase(), Vector<RefPtr<Region>> { }).iterator->value;

    // The regions vector is sorted in ascending pageCount to enable find smallest fit first.
    size_t position = 0;
    while (position < regions.size() && regions[position]->pageCount() < region->pageCount())
        ++position;
    regions.insert(position, WTF::move(region));
}

void Memory::removeRegion(Region* region)
{
    auto iterator = m_regions.find(region->remoteBase());
    RELEASE_ASSERT(iterator != m_regions.end());

    auto& regions = iterator->value;
    bool removed = regions.removeFirstMatching([&] (auto& candidate) {
        return candidate.get() == region;
    });
    RELEASE_ASSERT(removed);

    bool baseHasNoRegionsLeft = regions.isEmpty();
    if (baseHasNoRegionsLeft)
        m_regions.remove(iterator);
}

size_t Memory::regionCount() const
{
    size_t count = 0;
    for (auto& entry : m_regions)
        count += entry.value.size();
    return count;
}

size_t Memory::mappedPageCount() const
{
    size_t pages = 0;
    for (auto& entry : m_regions) {
        for (auto& region : entry.value)
            pages += region->pageCount();
    }
    return pages;
}

void Memory::dump(const char* indent) const
{
    dataLogLn(indent, "m_regions: ", regionCount(), " region(s) at ", m_regions.size(), " base(s), ",
        mappedPageCount(), " page(s) mapped");
    for (auto& entry : m_regions) {
        dataLog(indent, "  corpse ", RawHex(entry.key.toMachVMAddress()), " ->");
        for (auto& region : entry.value) {
            const char* baseNote = region->remoteBase() == entry.key ? "" : " <base disagrees with its key>";
            dataLog(" [", region->pageCount(), " page(s), local ",
                RawHex(reinterpret_cast<uintptr_t>(region->localBase())), ", refCount ", region->refCount(),
                baseNote, "]");
        }
        dataLogLn("");
    }
}

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
