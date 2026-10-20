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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseRegion.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <algorithm>
#include <dlfcn.h>
#if OS(DARWIN)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#include <stdlib.h>
#endif
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wtf/Scope.h>

namespace JSCToolsTest {

using JSC::Corpse::Address;
using JSC::Corpse::Region;

static void testAllRegions()
{
    size_t pageSize = static_cast<size_t>(getpagesize());
    void* mapping = mmap(nullptr, 3 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) {
        TEST_ASSERT(false, "the required test VA should be mappable");
        return;
    }
    // The kernel keeps a page with other protections as a region of its own.
    Address first { mapping };
    Address middle = first + pageSize;
    Address last = middle + pageSize;
    mprotect(reinterpret_cast<void*>(middle.toTargetVMAddress()), pageSize, PROT_READ);
    auto unmap = makeScopeExit([&] {
        munmap(mapping, 3 * pageSize);
    });

    {
        SelfSnapshot self;
        if (!self.isValid())
            return;
        const Vector<Region>& regions = self.snapshot().regions();
        TEST_ASSERT(&self.snapshot().regions() == &regions, "the region list is read once");
        bool isOrdered = std::ranges::adjacent_find(regions, [](const Region& region, const Region& next) {
            return region.end() > next.base();
        }) == regions.end();
        TEST_ASSERT(isOrdered, "the regions are by address, and do not overlap");
        auto containing = [&](Address address) {
            return std::ranges::find_if(regions, [&](const Region& region) {
                return region.contains(address);
            });
        };
        auto firstRegion = containing(first);
        auto middleRegion = containing(middle);
        auto lastRegion = containing(last);
        TEST_ASSERT(firstRegion != regions.end() && firstRegion->end() == middle, "a region ends where the protection changes");
        TEST_ASSERT(middleRegion != regions.end() && middleRegion->base() == middle && middleRegion->end() == last, "and the next starts there");
        TEST_ASSERT(lastRegion != regions.end() && lastRegion->base() == last, "and so on to the last page of a mapping");
        auto found = Region::findContaining(self.snapshot().corpsePort(), middle);
        TEST_ASSERT(middleRegion != regions.end() && found && found->base() == middleRegion->base() && found->size() == middleRegion->size()
            && found->residentPageCount() == middleRegion->residentPageCount(), "a listed region is the one found by its address");
#if !OS(DARWIN)
        // This executable's ELF header is at offset 0 of a mapping of its file.
        Dl_info info;
        std::unique_ptr<char, decltype(&free)> executable { realpath("/proc/self/exe", nullptr), &free };
        auto header = dladdr(reinterpret_cast<const void*>(&testRegion), &info) ? containing(Address { info.dli_fbase }) : regions.end();
        TEST_ASSERT(executable && header != regions.end() && header->base() == Address { info.dli_fbase } && !header->fileOffset()
            && header->name() == String::fromUTF8(executable.get()), "a region names the file it maps, and the offset it maps it from");
#endif
    }

    ExpectedErrors expectedErrors(2);
    JSC::Corpse::Snapshot invalid(JSC::Corpse::Process::create(getpid()));
    TEST_ASSERT(invalid.regions().isEmpty(), "an invalid snapshot lists no regions");
}

void testRegion()
{
    SuiteTracer tracer("Region");
    if (!tracer.shouldRun())
        return;
    testAllRegions();

    size_t pageSize = static_cast<size_t>(getpagesize());
    static constexpr size_t mappedPages = 5;
    static constexpr size_t writtenPages = 2;
    static constexpr size_t readPages = 1; // Exclusive of writtenPages.

    // The test scratch area is seven pages, with the first and last given back (holes), so
    // that the five in the middle are a region of known size with a hole on either side.
    // In this test, we'll check on those hole pages being holes. Hence, we need for them to
    // stay unmapped.
    //
    // Unfortunately, bmalloc or other heaps, when expanding, may consume the lower unmapped
    // page, and put it to use. This breaks our reliance on it being unmapped. So, we'll
    // precede the test scratch area with a runway of 10 unmapped pages. This gives any heap
    // some room to grow into without picking off our hole pages.
    static constexpr size_t runwayPaddingPages = 10;
    static constexpr size_t lowerSentinelPage = runwayPaddingPages;
    static constexpr size_t holeBelowRegion = lowerSentinelPage + 1;
    static constexpr size_t holeAboveRegion = holeBelowRegion + mappedPages + 1;
    static constexpr size_t upperSentinelPage = holeAboveRegion + 1;
    static constexpr size_t reservationPages = upperSentinelPage + 1;

    size_t reservationSize = reservationPages * pageSize;
    void* reservation = mmap(nullptr, reservationSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (reservation == MAP_FAILED) {
        TEST_ASSERT(false, "the required test VA should be mappable");
        return;
    }
    auto addressAt = [&](size_t pageIndex) {
        return reinterpret_cast<uintptr_t>(reservation) + pageIndex * pageSize;
    };
    auto unmapPages = [&](size_t pageIndex, size_t pageCount) {
        munmap(reinterpret_cast<void*>(addressAt(pageIndex)), pageCount * pageSize);
    };
    unmapPages(0, runwayPaddingPages);
    unmapPages(holeBelowRegion, 1);
    unmapPages(holeAboveRegion, 1);

    uintptr_t base = addressAt(holeBelowRegion + 1);
    for (size_t page = 0; page < writtenPages; ++page)
        *reinterpret_cast<volatile uint8_t*>(base + page * pageSize) = 1; // Touch with write.
    for (size_t page = writtenPages; page < writtenPages + readPages; ++page)
        (void)*reinterpret_cast<volatile uint8_t*>(base + page * pageSize); // Touch with read.

    // Gives back everything this test still holds: the region and the two sentinels.
    // The runwayPaddingPages are already unmapped.
    auto unmapPagesStillHeld = [&]() {
        unmapPages(lowerSentinelPage, 1);
        unmapPages(holeBelowRegion + 1, mappedPages);
        unmapPages(upperSentinelPage, 1);
    };

    SelfSnapshot self;
    if (!self.isValid()) {
        unmapPagesStillHeld();
        return;
    }
    JSC::Corpse::TaskHandle corpsePort = self.snapshot().corpsePort();
#if OS(DARWIN)
    size_t kernelPageSize = vm_kernel_page_size;
#else
    size_t kernelPageSize = pageSize;
#endif

    {
        auto region = Region::findContaining(corpsePort, Address(static_cast<JSC::Corpse::target_address_t>(base)));
        TEST_ASSERT(region, "the region holding a known mapping is found");
        if (region) {
            TEST_ASSERT_HEX_EQ(region->base().toTargetVMAddress(), base, "the region starts where the mapping does");
            TEST_ASSERT_EQ(region->size(), mappedPages * pageSize, "the region is as large as the mapping");
            TEST_ASSERT_HEX_EQ(region->end().toTargetVMAddress(), base + mappedPages * pageSize,
                "the region ends where the mapping does");
            TEST_ASSERT_EQ(region->pageCount(),
                static_cast<uint64_t>(mappedPages * pageSize / kernelPageSize),
                "the region holds as many pages as were mapped");
            TEST_ASSERT(region->contains(region->base()), "a region contains its first byte");
            TEST_ASSERT(region->contains(region->end() - 1), "a region contains its last byte");
            TEST_ASSERT(!region->contains(region->end()), "a region does not contain the byte past its end");
            TEST_ASSERT(!region->contains(region->base() - 1), "a region does not contain the byte before it");

#if OS(DARWIN)
            uint64_t accessedKernelPages =
                static_cast<uint64_t>((writtenPages + readPages) * pageSize / kernelPageSize);
            TEST_ASSERT_EQ(region->residentPageCount(), accessedKernelPages,
                "the pages that were accessed are the resident ones");

            // Dirty does not mean written: an anonymous page has no pager to be re-read
            // from, so it counts as dirty from the moment a fault creates it, whether
            // that fault was a write or a read. The page that was only read is dirty in
            // most runs but not all, so the written pages are what can be counted on.
            uint64_t writtenKernelPages = static_cast<uint64_t>(writtenPages * pageSize / kernelPageSize);
            TEST_ASSERT(region->dirtyPageCount() >= writtenKernelPages
                && region->dirtyPageCount() <= accessedKernelPages,
                "every page that was written is dirty and no page that was never accessed is");
#else
            skipSuite("Region page residency", "/proc/<pid>/maps gives no resident or dirty page counts");
#endif
        }
    }
    {
        // An address in the middle of the mapping still finds the whole region.
        auto region = Region::findContaining(corpsePort,
            Address(static_cast<JSC::Corpse::target_address_t>(base + pageSize + 16)));
        TEST_ASSERT(region, "an address inside the mapping finds the region");
        if (region)
            TEST_ASSERT_HEX_EQ(region->base().toTargetVMAddress(), base, "any address in a region finds its base");
    }
    {
        // The kernel reports the region at or above the address it is asked about,
        // so a hole must be reported as a hole rather than as the region above it.
        auto region = Region::findContaining(corpsePort,
            Address(static_cast<JSC::Corpse::target_address_t>(addressAt(holeBelowRegion))));
        TEST_ASSERT(!region, "an address in an unmapped hole finds no region");
    }
    {
        // The same question asked from the other side. An address in this hole sits past
        // the end of the region below it, which must not be reported as containing it.
        auto region = Region::findContaining(corpsePort,
            Address(static_cast<JSC::Corpse::target_address_t>(addressAt(holeAboveRegion))));
        TEST_ASSERT(!region, "an address in the hole above a region finds no region");
    }
    {
        // On Darwin, the shared cache is mapped as a submap, which the search has to descend
        // into before it can describe what is actually there. A function's address
        // arrives signed on arm64e, and is an address only once stripped.
        Address inSystemLibrary = Address(reinterpret_cast<void*>(&memcpy)).stripped();
        auto region = Region::findContaining(corpsePort, inSystemLibrary);
        TEST_ASSERT(region, "an address in a system library (the shared cache on Darwin) finds a region");
        if (region)
            TEST_ASSERT(region->size(), "a system library region has a size");
    }

    unmapPagesStillHeld();
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
