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

#if ENABLE(MYA)

#if OS(DARWIN)
#include "CorpseMachVMSPI.h"
#include <mach/mach.h>
#else
#include <errno.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <limits>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Memory::Region);

namespace {

void deallocateLocalPages(const uint8_t* localBase, size_t bytes)
{
#if OS(DARWIN)
    mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(localBase), bytes);
#else
    munmap(const_cast<uint8_t*>(localBase), bytes);
#endif
}

} // anonymous namespace

Memory::Region::Region(Memory& memory, Address remoteBase, size_t pageCount, const uint8_t* localBase)
    : m_memory(memory)
    , m_remoteBase(remoteBase)
    , m_pageCount(pageCount)
    , m_localBase(localBase)
{
}

void Memory::Region::release()
{
    RELEASE_ASSERT(m_refCount);
    if (--m_refCount)
        return;
    m_memory.unmap(this); // Destroys this Region.
}

size_t Memory::pageSize()
{
#if OS(DARWIN)
    return vm_kernel_page_size;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}

Memory::Memory(TaskHandle corpsePort)
    : m_corpsePort(corpsePort)
{
    // Placed here so we can assert this invariant only once on construction.
    size_t page = pageSize();
    RELEASE_ASSERT(page && !(page & (page - 1))); // The masking below needs a power of two.
}

Memory::~Memory()
{
    RELEASE_ASSERT(m_regions.isEmpty());

    for (auto& entry : m_regions) {
        for (auto& region : entry.value)
            deallocateLocalPages(region->localBase(), region->pageCount() * pageSize());
    }
}

Memory::MapResult Memory::map(Address objAddress, size_t objSizeInBytes)
{
    if (!objSizeInBytes)
        return MapResult { .error = Error::InvalidRequest };

    target_address_t objBase = objAddress.toTargetVMAddress();
    target_address_t objEnd = objBase + objSizeInBytes;
    if (objEnd < objBase)
        return MapResult { .error = Error::InvalidRequest }; // objEnd overflows.

    size_t page = pageSize();
    target_address_t pageMask = static_cast<target_address_t>(page) - 1;

    if (objEnd > std::numeric_limits<target_address_t>::max() - pageMask)
        return MapResult { .error = Error::InvalidRequest };

    // Compute Region that object resides in.
    target_address_t regionBase = objBase & ~pageMask;
    target_address_t regionEnd = (objEnd + pageMask) & ~pageMask;

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
            if (region->pageCount() >= pageCount) {
                region->retain();
                return MapResult { region.get(), region->localBase() + offset };
            }
        }
    }

    // No existing Region fits. Map a new one.
    size_t regionBytes = pageCount * page;
#if OS(DARWIN)
    mach_vm_address_t target = 0;

    // Map the remote page(s) into our process as Read-Only and without copying.
    vm_prot_t currentProtection = VM_PROT_READ;
    vm_prot_t maximumProtection = VM_PROT_READ;
    boolean_t makeCopyOrNot = false;
    kern_return_t kr = mach_vm_remap_new(mach_task_self(), &target, regionBytes, 0, VM_FLAGS_ANYWHERE,
        m_corpsePort, regionBaseAddress.toTargetVMAddress(), makeCopyOrNot, &currentProtection, &maximumProtection, VM_INHERIT_NONE);

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
#else
    void* target = mmap(nullptr, regionBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (target == MAP_FAILED)
        return MapResult { .error = Error::Unknown, .kernResult = errno };

    struct iovec local { target, regionBytes };
    struct iovec remote { reinterpret_cast<void*>(static_cast<uintptr_t>(regionBase)), regionBytes };
    ssize_t got = process_vm_readv(m_corpsePort, &local, 1, &remote, 1, 0);
    if (got < 0 || static_cast<size_t>(got) != regionBytes) {
        int result = got < 0 ? errno : EFAULT;
        munmap(target, regionBytes);
        return MapResult { .error = Error::Unknown, .kernResult = result };
    }
    auto* localBase = reinterpret_cast<const uint8_t*>(target);
#endif

    // Success.
    auto region = makeUnique<Region>(*this, regionBaseAddress, pageCount, localBase);
    region->retain();
    Region* mapped = region.get();

    auto& regions = m_regions.add(regionBaseAddress, Vector<std::unique_ptr<Region>> { }).iterator->value;
    size_t position = 0;
    while (position < regions.size() && regions[position]->pageCount() < pageCount)
        ++position;
    regions.insert(position, WTF::move(region));

    return MapResult { mapped, localBase + offset };
}

void Memory::unmap(Region* region)
{
    ASSERT(!region->refCount());

    Address remoteBase = region->remoteBase();
    deallocateLocalPages(region->localBase(), region->pageCount() * pageSize());

    auto iterator = m_regions.find(remoteBase);
    RELEASE_ASSERT(iterator != m_regions.end());

    // When found, removeFirstMatching will clear the unique_ptr, which deletes the Region.
    // So make sure we don't use the region after this remove.
    bool removed = iterator->value.removeFirstMatching([&] (auto& candidate) {
        return candidate.get() == region;
    });
    RELEASE_ASSERT(removed);

    bool baseHasNoRegionsLeft = iterator->value.isEmpty();
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
        dataLog(indent, "  corpse ", RawHex(entry.key.toTargetVMAddress()), " ->");
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

#endif // ENABLE(MYA)
