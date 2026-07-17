/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestHarness.h"

#include "bmalloc_heap.h"
#include "bmalloc_heap_config.h"
#include "pas_internal_config.h"
#include "pas_page_malloc.h"
#include "tagged_bmalloc_heap.h"
#include <algorithm>
#include <cstdint>
#include <vector>

#if PAS_ENABLE_BMALLOC

namespace {

// The bmalloc heap's minimum alignment for small objects, and hence its
// smallest object size (16 bytes).
constexpr size_t bmallocMinAlign = static_cast<size_t>(1) << BMALLOC_MINALIGN_SHIFT;

// The size at and above which zeroing switches from memset to the large-heap
// virtual-memory path.
constexpr size_t vaZeroThreshold = static_cast<size_t>(1) << PAS_VA_BASED_ZERO_MEMORY_SHIFT;

// Interior sample sizes are taken as a page divided by a small factor, so they
// fall a fraction of the way into a page-based regime rather than on a boundary,
// and track the page constants if a config changes. A larger factor gives a
// smaller object (more per page); pageFractionDivisor is the usual choice.
constexpr size_t pageFractionDivisor = 16;

// Half of pageFractionDivisor, i.e. twice the sample size, used where the usual
// fraction would coincide with another size already covered in the same group.
constexpr size_t coarsePageFractionDivisor = pageFractionDivisor / 2;

// A small, deliberately non-aligned amount (odd and below bmallocMinAlign) added
// to interior page-fraction samples so the requested size is not a multiple of
// the allocation quantum. The zeroed path zeroes exactly the requested size, so
// a non-aligned size lets the whole-buffer check catch a regression that rounded
// the zeroing length down to an alignment or page boundary; a round size would
// mask it. Boundary and regime-max samples stay exact.
constexpr size_t oddSizeDelta = bmallocMinAlign - 1;

// Fill the buffer with a non-zero, offset-varying pattern so that any byte left
// behind by a later allocation is detectable.
void dirtyBuffer(void* ptr, size_t size)
{
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i)
        bytes[i] = static_cast<uint8_t>(0x80 | (i & 0x7f));
}

// Return the offset of the first non-zero byte, or size if every byte is zero.
// Scans 8 bytes at a time (a word is non-zero iff some byte is), byte-scanning an
// unaligned head and the sub-word tail.
size_t firstNonZeroByte(const void* ptr, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
    size_t i = 0;
    while (i < size && (reinterpret_cast<uintptr_t>(bytes + i) & (sizeof(uint64_t) - 1))) {
        if (bytes[i])
            return i;
        ++i;
    }
    for (; i + sizeof(uint64_t) <= size; i += sizeof(uint64_t)) {
        if (*reinterpret_cast<const uint64_t*>(bytes + i)) {
            for (size_t b = i; b < i + sizeof(uint64_t); ++b) {
                if (bytes[b])
                    return b;
            }
        }
    }
    for (; i < size; ++i) {
        if (bytes[i])
            return i;
    }
    return size;
}

// The two axes varied below, in addition to size and alignment. Untagged vs
// tagged selects the two families of zeroed entry points. Plain vs
// with-alignment selects entry points that take different internal paths even
// when the requested alignment is the natural minimum.
enum class HeapVariant { Untagged, Tagged };
enum class AlignmentApi { Plain, WithAlignment };

// Whether reuse goes straight through (WithoutScavenge) or forces a synchronous
// scavenge between free and reuse, so the region is decommitted and must be
// recommitted to serve the second allocation (WithScavenge). Unlike the axes
// above, this is chosen once per test rather than varied within a run.
enum class Reuse { WithoutScavenge, WithScavenge };

const char* variantName(HeapVariant variant)
{
    return variant == HeapVariant::Tagged ? "tagged" : "untagged";
}

const char* alignmentApiName(AlignmentApi api)
{
    return api == AlignmentApi::WithAlignment ? "with-alignment" : "plain";
}

void* allocateZeroed(HeapVariant variant, AlignmentApi api, size_t size, size_t alignment)
{
    switch (variant) {
    case HeapVariant::Untagged:
        if (api == AlignmentApi::WithAlignment)
            return bmalloc_try_allocate_zeroed_with_alignment(size, alignment, pas_non_compact_allocation_mode);
        return bmalloc_try_allocate_zeroed(size, pas_non_compact_allocation_mode);
    case HeapVariant::Tagged:
        if (api == AlignmentApi::WithAlignment)
            return tagged_bmalloc_try_allocate_zeroed_with_alignment(size, alignment);
        return tagged_bmalloc_try_allocate_zeroed(size);
    }
    CHECK(!"Invalid variant");
    return nullptr;
}

void deallocateVariant(HeapVariant variant, void* ptr)
{
    if (variant == HeapVariant::Tagged)
        tagged_bmalloc_deallocate(ptr);
    else
        bmalloc_deallocate(ptr);
}

// Print the parameters that identify a failing case, so a zero-check or
// alignment failure points at the exact size/alignment/variant/phase.
void reportParameters(HeapVariant variant, AlignmentApi api, size_t size, size_t alignment, const char* phase)
{
    std::cout << "  params: variant=" << variantName(variant) << " api=" << alignmentApiName(api)
              << " size=" << size << " alignment=" << alignment << " phase=" << phase << std::endl;
}

// The order in which a buffer's pages are first faulted during verification. The
// large VA-zeroing path (madvise(MADV_ZERO) or the mmap fallback) leaves pages
// zero-fill-on-demand, so a page is only observed zero when first touched; a
// hashtable reads its buckets in hash order -- effectively at random -- so a
// fault-order-dependent zeroing bug could surface under non-ascending access but
// not under an ascending scan. (For the eagerly memset-zeroed regimes the order
// is immaterial, so those callers use the default Ascending.)
enum class FaultOrder { Ascending, Descending, Shuffled };

const char* faultOrderName(FaultOrder order)
{
    switch (order) {
    case FaultOrder::Ascending:
        return "ascending";
    case FaultOrder::Descending:
        return "descending";
    case FaultOrder::Shuffled:
        return "shuffled";
    }
    CHECK(!"Invalid fault order");
    return "";
}

// In-place Fisher-Yates shuffle using the seeded test RNG: random order, but
// reproducible across runs.
template<typename T>
void seededShuffle(std::vector<T>& values)
{
    for (size_t i = values.size(); i-- > 1;) {
        size_t j = deterministicRandomNumber(static_cast<unsigned>(i + 1));
        T swapTemp = values[i];
        values[i] = values[j];
        values[j] = swapTemp;
    }
}

// The sequence of page indices to touch, in the requested first-fault order.
std::vector<size_t> pageVisitationOrder(size_t numPages, FaultOrder order)
{
    std::vector<size_t> pages;
    pages.reserve(numPages);
    for (size_t i = 0; i < numPages; ++i)
        pages.push_back(i);

    switch (order) {
    case FaultOrder::Ascending:
        break;
    case FaultOrder::Descending:
        std::reverse(pages.begin(), pages.end());
        break;
    case FaultOrder::Shuffled:
        seededShuffle(pages);
        break;
    }
    return pages;
}

// Require every byte to be zero, reporting the identifying parameters and the
// first offending offset/value on failure. `order` controls the order in which
// the buffer's pages are first faulted: Ascending walks them front to back;
// Descending/Shuffled visit pages out of order (the first byte touched in each
// page faults it), which matters only for the large VA-zeroed path.
void checkIsZeroed(const void* ptr, size_t size, HeapVariant variant, AlignmentApi api,
                   size_t alignment, const char* phase, FaultOrder order = FaultOrder::Ascending)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(ptr);

    // Visit the pages in the requested order so the first byte touched in each
    // page faults it in that order.
    size_t pageSize = pas_page_malloc_alignment();
    size_t numPages = (size + pageSize - 1) / pageSize;
    std::vector<size_t> pages = pageVisitationOrder(numPages, order);
    for (size_t pageIndex : pages) {
        size_t begin = pageIndex * pageSize;
        // The allocation might not be a whole number of pages; clamp the last one.
        size_t end = std::min(begin + pageSize, size);
        size_t pageLength = end - begin;
        size_t nonZero = firstNonZeroByte(bytes + begin, pageLength);
        if (nonZero < pageLength) {
            size_t offset = begin + nonZero;
            reportParameters(variant, api, size, alignment, phase);
            std::cout << "  first non-zero byte at offset " << offset << " (page " << pageIndex << " of "
                      << numPages << ") is 0x" << std::hex << static_cast<unsigned>(bytes[offset]) << std::dec
                      << " with " << faultOrderName(order) << " fault order." << std::endl;
            CHECK(!bytes[offset]);
        }
    }
}

void checkIsAligned(const void* ptr, size_t size, HeapVariant variant, AlignmentApi api,
                    size_t alignment, const char* phase)
{
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    if (address % alignment)
        reportParameters(variant, api, size, alignment, phase);
    CHECK_EQUAL(address % alignment, static_cast<uintptr_t>(0));
}

// One case of the run: zeroed-alloc, verify zero (and alignment), dirty, free,
// then zeroed-alloc the same size again and verify the reused region is zero.
// With Reuse::WithScavenge, a synchronous scavenge runs after the free so the
// region is decommitted and the reuse must go through recommit rather than a
// straight free-list handback. `order` sets the fault order of the reused
// buffer's check -- Ascending for the generic runs; the large VA-zeroed
// fault-order test (testLargeZeroingFaultOrder) drives Descending/Shuffled.
void zeroingReuse(HeapVariant variant, AlignmentApi api, size_t size, size_t alignment,
                  Reuse reuse, FaultOrder order = FaultOrder::Ascending)
{
    void* first = allocateZeroed(variant, api, size, alignment);
    if (!first)
        reportParameters(variant, api, size, alignment, "fresh");
    CHECK(first);
    checkIsZeroed(first, size, variant, api, alignment, "fresh");
    if (api == AlignmentApi::WithAlignment)
        checkIsAligned(first, size, variant, api, alignment, "fresh");

    dirtyBuffer(first, size);
    deallocateVariant(variant, first);

    // Force the just-freed region to be decommitted (its physical pages returned
    // to the OS) before reuse, so the second allocation must recommit and
    // re-fault -- and hence re-zero -- those pages instead of reusing them
    // straight off a free list. Decommit does not zero on its own, so a correct
    // result depends entirely on the allocator re-zeroing.
    if (reuse == Reuse::WithScavenge)
        pas_scavenger_run_synchronously_now();

    void* second = allocateZeroed(variant, api, size, alignment);
    if (!second)
        reportParameters(variant, api, size, alignment, "reused");
    CHECK(second);
    // Same-size reuse from the large heap returns the same region, so a
    // different region here means the re-zeroing did not act on the dirtied
    // backing. Smaller regimes legitimately hand back a different free-list entry,
    // so only the large-heap sizes make this worth noting.
    if (size >= vaZeroThreshold && second != first) {
        std::cout << "Note: reuse returned a different region for size " << size
                  << "; dirtied-backing reuse not exercised." << std::endl;
    }
    checkIsZeroed(second, size, variant, api, alignment, "reused", order);
    if (api == AlignmentApi::WithAlignment)
        checkIsAligned(second, size, variant, api, alignment, "reused");

    deallocateVariant(variant, second);
}

// Run zeroingReuse across both heap variants and both alignment-API forms,
// for each requested size. The plain form runs once per size at the natural
// minimum alignment; the with-alignment form runs across a range of alignments.
// The background scavenger is suspended so the reuse path is deterministic: a
// WithoutScavenge case reuses its still-committed region, and a WithScavenge case
// decommits only at the forced scavenge, not at some unpredictable
// background point.
void runZeroingReuse(const std::vector<size_t>& sizes, Reuse reuse = Reuse::WithoutScavenge)
{
    pas_scavenger_suspend();

    static constexpr HeapVariant variants[] = { HeapVariant::Untagged, HeapVariant::Tagged };
    static constexpr size_t alignments[] = { 16, 512, 4096, 16384 };

    for (HeapVariant variant : variants) {
        for (size_t size : sizes) {
            zeroingReuse(variant, AlignmentApi::Plain, size, bmallocMinAlign, reuse);
            for (size_t alignment : alignments)
                zeroingReuse(variant, AlignmentApi::WithAlignment, size, alignment, reuse);
        }
    }
}

// Companion run for the large VA-zeroed path: for each size, cross the fault
// order (how the reused buffer's pages are first touched) with the reuse mode
// (immediate free-list reuse vs forced decommit/recommit). Untagged/plain only,
// since the VA-zeroing path is variant- and alignment-independent. Suspends the
// scavenger for the same determinism reason as runZeroingReuse.
void runZeroingReuseFaultOrders(const std::vector<size_t>& sizes)
{
    pas_scavenger_suspend();

    static constexpr FaultOrder orders[] = {
        FaultOrder::Ascending, FaultOrder::Descending, FaultOrder::Shuffled,
    };
    static constexpr Reuse reuses[] = { Reuse::WithoutScavenge, Reuse::WithScavenge };

    for (size_t size : sizes) {
        for (FaultOrder order : orders) {
            for (Reuse reuse : reuses)
                zeroingReuse(HeapVariant::Untagged, AlignmentApi::Plain, size, bmallocMinAlign, reuse, order);
        }
    }
}

// The size groups below sample every regime of the zeroed-malloc path. Sizes
// are derived from the config's page-size and threshold constants rather than
// hard-coded, so the run keeps hitting the interesting boundaries if a config
// changes. The subsystem for a given size is chosen by the runtime config, but
// these ranges are representative: a size at a fraction of a page lands inside
// that page's subsystem, and one near page/PAS_MIN_OBJECTS_PER_PAGE sits near
// that subsystem's largest object. Segregated and bitfit pages both zero via
// memset, the large heap zeroes via memset below the virtual-memory threshold,
// and via the virtual-memory path at and above it.

std::vector<size_t> segregatedRegimeSizes()
{
    // Small- and medium-segregated pages (memset), starting at the smallest
    // object the heap serves and climbing toward each subsystem's largest object
    // at page/PAS_MIN_OBJECTS_PER_PAGE.
    return {
        bmallocMinAlign,
        PAS_SMALL_PAGE_DEFAULT_SIZE / pageFractionDivisor + oddSizeDelta,
        PAS_SMALL_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE,
        PAS_MEDIUM_PAGE_DEFAULT_SIZE / pageFractionDivisor + oddSizeDelta,
        PAS_MEDIUM_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE,
    };
}

std::vector<size_t> bitfitRegimeSizes()
{
    // Medium-bitfit and marge-bitfit pages (memset). The first size exceeds the
    // medium-segregated max object, so it rolls into bitfit; the tagged ceiling
    // is included because it is where the tagged variant stops using the
    // MTE-taggable path.
    return {
        PAS_SMALL_PAGE_DEFAULT_SIZE,
        PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
        // Coarser fraction here: the usual one would equal the tagged ceiling above.
        PAS_MEDIUM_BITFIT_PAGE_DEFAULT_SIZE / coarsePageFractionDivisor + oddSizeDelta,
        PAS_MARGE_PAGE_DEFAULT_SIZE / pageFractionDivisor + oddSizeDelta,
    };
}

std::vector<size_t> largeMemsetRegimeSizes()
{
    // Large heap below the virtual-memory threshold (memset): a size past the
    // marge-bitfit max object, and one just below the threshold.
    return {
        PAS_MEDIUM_BITFIT_PAGE_DEFAULT_SIZE,
        vaZeroThreshold - PAS_SMALL_PAGE_DEFAULT_SIZE,
    };
}

std::vector<size_t> largeVirtualMemoryRegimeSizes()
{
    // Large heap around the virtual-memory threshold: one size just below (still
    // memset) to bracket the boundary, the threshold itself and just above (both
    // virtual-memory zeroing), and a multi-megabyte object.
    return {
        vaZeroThreshold - bmallocMinAlign,
        vaZeroThreshold,
        vaZeroThreshold + bmallocMinAlign,
        4 * vaZeroThreshold,
    };
}

void testZeroingReuseSegregatedSizes()
{
    runZeroingReuse(segregatedRegimeSizes());
}

void testZeroingReuseBitfitSizes()
{
    runZeroingReuse(bitfitRegimeSizes());
}

void testZeroingReuseLargeMemsetSizes()
{
    runZeroingReuse(largeMemsetRegimeSizes());
}

void testZeroingReuseLargeVirtualMemorySizes()
{
    runZeroingReuse(largeVirtualMemoryRegimeSizes());
}

// Same reuse check, but forcing a synchronous scavenge between free and reuse so
// the region is decommitted and must be recommitted (and re-zeroed) to serve the
// second allocation. This exercises the decommit/recommit reuse path across the
// same regimes.
void testZeroingReuseWithScavengeSegregatedSizes()
{
    runZeroingReuse(segregatedRegimeSizes(), Reuse::WithScavenge);
}

void testZeroingReuseWithScavengeBitfitSizes()
{
    runZeroingReuse(bitfitRegimeSizes(), Reuse::WithScavenge);
}

void testZeroingReuseWithScavengeLargeMemsetSizes()
{
    runZeroingReuse(largeMemsetRegimeSizes(), Reuse::WithScavenge);
}

void testZeroingReuseWithScavengeLargeVirtualMemorySizes()
{
    runZeroingReuse(largeVirtualMemoryRegimeSizes(), Reuse::WithScavenge);
}

// Small objects are batched many-per-page: a segregated page holds at least
// PAS_MIN_OBJECTS_PER_PAGE objects, so one page's backing serves many
// allocations and those objects are zeroed per object, not per page. Fill
// several pages so that many objects are co-resident on shared pages, verifying
// each is independently zero -- both when first carved from a page and when a
// freed object is reused.
void smallPageBatching(HeapVariant variant, size_t size)
{
    // Enough objects to span several pages, computed per size so the packed and
    // sparse cases both cross page boundaries. objectsPerPage is approximate (the
    // object size is rounded up to a size class, and page headers take space),
    // which only over-counts, so the span is at least this many pages.
    constexpr size_t pagesToSpan = 4;
    size_t objectsPerPage = PAS_SMALL_PAGE_DEFAULT_SIZE / size;
    if (!objectsPerPage)
        objectsPerPage = 1;
    size_t count = objectsPerPage * pagesToSpan;

    std::vector<void*> objects;
    objects.reserve(count);

    // Fill: allocate many co-resident objects sharing pages; each must be zero.
    // Dirty each so that once they are freed the recycled objects hold non-zero
    // bytes, making the refill check meaningful.
    for (size_t i = 0; i < count; ++i) {
        void* object = allocateZeroed(variant, AlignmentApi::Plain, size, bmallocMinAlign);
        if (!object)
            reportParameters(variant, AlignmentApi::Plain, size, bmallocMinAlign, "fill");
        CHECK(object);
        checkIsZeroed(object, size, variant, AlignmentApi::Plain, bmallocMinAlign, "fill");
        dirtyBuffer(object, size);
        objects.push_back(object);
    }

    // Free all, then refill: the freed objects are recycled to serve the new
    // allocations, so each reallocated object must be re-zeroed individually rather
    // than relying on the initial page commit.
    for (void* object : objects)
        deallocateVariant(variant, object);
    for (size_t i = 0; i < count; ++i) {
        void* object = allocateZeroed(variant, AlignmentApi::Plain, size, bmallocMinAlign);
        if (!object)
            reportParameters(variant, AlignmentApi::Plain, size, bmallocMinAlign, "recycled");
        CHECK(object);
        checkIsZeroed(object, size, variant, AlignmentApi::Plain, bmallocMinAlign, "recycled");
        objects[i] = object;
    }
    for (void* object : objects)
        deallocateVariant(variant, object);
}

// Verify per-object zeroing when many small objects share batched pages, on both
// fresh page commits and object recycling.
void testSmallZeroingPageBatching()
{
    // Small sizes bracketing the batching density: the smallest object (the most
    // per page) and the largest small-segregated object (the fewest per page, but
    // still at least PAS_MIN_OBJECTS_PER_PAGE).
    static constexpr size_t sizes[] = {
        bmallocMinAlign,
        PAS_SMALL_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE,
    };
    static constexpr HeapVariant variants[] = { HeapVariant::Untagged, HeapVariant::Tagged };
    for (HeapVariant variant : variants) {
        for (size_t size : sizes)
            smallPageBatching(variant, size);
    }
}

// Verify large VA-zeroed allocations read zero regardless of the order in which
// their pages are first faulted, mimicking a hashtable's non-sequential bucket
// access on reused (previously-dirtied) memory. Runs each order under both reuse
// modes; ascending is a control against descending and shuffled.
void testLargeZeroingFaultOrder()
{
    // A fixed multi-megabyte size (spans many pages) alongside the threshold; kept
    // independent of vaZeroThreshold so lowering the threshold can't shrink it.
    constexpr size_t multiMegabyteSize = static_cast<size_t>(4) << 20;
    static_assert(multiMegabyteSize >= vaZeroThreshold,
                  "large fault-order sample should stay on the VA-zeroing path");
    runZeroingReuseFaultOrders({ vaZeroThreshold, multiMegabyteSize });
}

// A live buffer in the fragmentation pool, carrying the unique stamp written into
// it. stamp() fills the whole buffer with a single non-zero byte keyed to stampId:
// the high bit is always set (so it is never mistaken for zeroed memory) and the
// low 7 bits are the id. Two live buffers whose ids differ mod 128 therefore hold
// different bytes, so if one is handed the other's still-live memory the overwrite
// differs from the expected byte at every overlapping position -- at any offset, so
// a partial/shifted overlap is caught too. Stamp and verify run 8 bytes at a time.
struct LiveBuffer {
    void* ptr;
    size_t size;
    uint32_t stampId;

    uint8_t stampByte() const
    {
        return static_cast<uint8_t>(0x80 | (stampId & 0x7f));
    }

    // The stamp byte broadcast to all 8 lanes, for word-at-a-time stamp/verify.
    uint64_t stampWord() const
    {
        return static_cast<uint64_t>(stampByte()) * 0x0101010101010101ULL;
    }

    // Write the stamp across the whole buffer. The allocation base is at least
    // 8-byte aligned, so full words are written aligned; the sub-word tail is
    // written byte-wise.
    void stamp()
    {
        uint8_t* bytes = static_cast<uint8_t*>(ptr);
        uint64_t word = stampWord();
        size_t i = 0;
        for (; i + sizeof(uint64_t) <= size; i += sizeof(uint64_t))
            *reinterpret_cast<uint64_t*>(bytes + i) = word;
        for (; i < size; ++i)
            bytes[i] = stampByte();
    }

    // Verify the buffer still holds its stamp, reporting the first mismatching
    // offset on failure. A mismatch means another allocation was handed this
    // still-live buffer's memory (overwriting the stamp, or re-zeroing it).
    void verify(HeapVariant variant, const char* phase) const
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
        uint64_t word = stampWord();
        uint8_t expected = stampByte();
        size_t i = 0;
        for (; i + sizeof(uint64_t) <= size; i += sizeof(uint64_t)) {
            if (*reinterpret_cast<const uint64_t*>(bytes + i) != word)
                break;
        }
        for (; i < size; ++i) {
            if (bytes[i] != expected) {
                std::cout << "  stamp mismatch: variant=" << variantName(variant) << " phase=" << phase
                          << " stampId=" << stampId << " size=" << size << " offset=" << i << " expected=0x"
                          << std::hex << static_cast<unsigned>(expected) << " actual=0x"
                          << static_cast<unsigned>(bytes[i]) << std::dec << "." << std::endl;
                CHECK_EQUAL(bytes[i], expected);
                return;
            }
        }
    }
};

// One zeroing regime's size range. The pool takes one size per regime per round,
// so every regime stays covered while the exact size within it is randomized --
// varying fragmentation and catching any size-rounding bug. Ranges track the config
// constants; the large-VA range is capped so the many-live pool, whose stamps are
// verified byte-for-byte, stays bounded.
struct FragmentationRegime {
    size_t minSize;
    size_t maxSize;
};

std::vector<FragmentationRegime> fragmentationRegimes()
{
    size_t smallSegMax = PAS_SMALL_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE;
    size_t mediumSegMax = PAS_MEDIUM_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE;
    size_t bitfitMax = PAS_MARGE_PAGE_DEFAULT_SIZE / PAS_MIN_OBJECTS_PER_PAGE;
    return {
        { bmallocMinAlign, smallSegMax }, // small-segregated (memset)
        { smallSegMax + 1, mediumSegMax }, // medium-segregated (memset)
        { mediumSegMax + 1, bitfitMax }, // medium/marge bitfit (memset)
        { bitfitMax + 1, vaZeroThreshold - 1 }, // large heap, memset
        { vaZeroThreshold, 2 * vaZeroThreshold }, // large heap, virtual-memory zeroing
    };
}

// One pool allocation's randomized size and alignment entry point.
struct AllocationSpec {
    size_t size;
    size_t alignment;
    AlignmentApi api;
};

struct AlignmentChoice {
    AlignmentApi api;
    size_t alignment;
};

// A size in [minSize, maxSize], chosen with the seeded RNG.
size_t randomSizeInRange(size_t minSize, size_t maxSize)
{
    if (maxSize <= minSize)
        return minSize;
    return minSize + deterministicRandomNumber(static_cast<unsigned>(maxSize - minSize + 1));
}

// Randomly pick an alignment entry point (seeded): the plain API at the natural
// minimum, or the with-alignment API at one of the reuse sweep's quanta, so the
// pool exercises both paths across a range of alignments.
AlignmentChoice randomAlignmentChoice()
{
    static constexpr AlignmentChoice choices[] = {
        { AlignmentApi::Plain, bmallocMinAlign },
        { AlignmentApi::WithAlignment, 16 },
        { AlignmentApi::WithAlignment, 512 },
        { AlignmentApi::WithAlignment, 4096 },
        { AlignmentApi::WithAlignment, 16384 },
    };
    unsigned count = static_cast<unsigned>(sizeof(choices) / sizeof(choices[0]));
    return choices[deterministicRandomNumber(count)];
}

// A shuffled, stratified allocation plan: one spec per regime per round (so every
// regime is covered poolRounds times) with a randomized size and alignment, then
// shuffled so regimes and sizes interleave. A fresh call re-randomizes, so fill and
// refill differ.
std::vector<AllocationSpec> fragmentationPlan(size_t poolRounds)
{
    std::vector<FragmentationRegime> regimes = fragmentationRegimes();
    std::vector<AllocationSpec> plan;
    plan.reserve(regimes.size() * poolRounds);
    for (size_t round = 0; round < poolRounds; ++round) {
        for (const FragmentationRegime& regime : regimes) {
            AlignmentChoice choice = randomAlignmentChoice();
            plan.push_back({ randomSizeInRange(regime.minSize, regime.maxSize), choice.alignment, choice.api });
        }
    }
    seededShuffle(plan);
    return plan;
}

// Fragmentation + aliasing coverage over randomized, per-regime-stratified sizes
// and alignments (phases detailed inline). Every allocation is zero-checked,
// extending born-dirty coverage to fragmented and possibly-decommitted backing;
// each live buffer's unique stamp is re-verified after every phase, catching a
// double-handout -- an allocation returned an address still owned by a live buffer.
// Single-threaded; the chaos tests cover the concurrent case.
void fragmentationReuse(HeapVariant variant, Reuse reuse)
{
    constexpr size_t poolRounds = 12;

    std::vector<LiveBuffer> live;
    uint32_t nextStampId = 1;

    auto allocateStamped = [&](const AllocationSpec& spec, const char* phase) {
        void* ptr = allocateZeroed(variant, spec.api, spec.size, spec.alignment);
        if (!ptr)
            reportParameters(variant, spec.api, spec.size, spec.alignment, phase);
        CHECK(ptr);
        checkIsZeroed(ptr, spec.size, variant, spec.api, spec.alignment, phase);
        if (spec.api == AlignmentApi::WithAlignment)
            checkIsAligned(ptr, spec.size, variant, spec.api, spec.alignment, phase);
        LiveBuffer buffer { ptr, spec.size, nextStampId };
        buffer.stamp();
        live.push_back(buffer);
        ++nextStampId;
    };

    auto verifyAllStamps = [&](const char* phase) {
        for (const LiveBuffer& buffer : live)
            buffer.verify(variant, phase);
    };

    // Fill the pool from a shuffled, stratified plan.
    for (const AllocationSpec& spec : fragmentationPlan(poolRounds))
        allocateStamped(spec, "fill");
    verifyAllStamps("after-fill");

    // Free a seeded-random subset so live and freed regions interleave irregularly;
    // WithScavenge decommits them before the refill. Survivors' stamps must stay
    // untouched.
    std::vector<LiveBuffer> survivors;
    for (const LiveBuffer& buffer : live) {
        if (deterministicRandomNumber(2))
            survivors.push_back(buffer);
        else
            deallocateVariant(variant, buffer.ptr);
    }
    live = survivors;
    if (reuse == Reuse::WithScavenge)
        pas_scavenger_run_synchronously_now();
    verifyAllStamps("after-scattered-free");

    // Refill from a fresh plan so the irregularly fragmented free regions must be
    // split and coalesced to serve the new sizes, not just reused in the order freed.
    for (const AllocationSpec& spec : fragmentationPlan(poolRounds))
        allocateStamped(spec, "refill");
    verifyAllStamps("after-refill");

    // Teardown: a final stamp check as each buffer is freed.
    for (const LiveBuffer& buffer : live) {
        buffer.verify(variant, "teardown");
        deallocateVariant(variant, buffer.ptr);
    }
}

// Run across both heap variants, scavenger suspended so freed regions decommit
// only at the forced scavenge (WithScavenge), not at an unpredictable background
// point.
void runFragmentationReuse(Reuse reuse)
{
    pas_scavenger_suspend();

    static constexpr HeapVariant variants[] = { HeapVariant::Untagged, HeapVariant::Tagged };
    for (HeapVariant variant : variants)
        fragmentationReuse(variant, reuse);
}

// Fragmentation + aliasing with immediate free-list reuse (no scavenge between
// free and refill).
void testFragmentationReuse()
{
    runFragmentationReuse(Reuse::WithoutScavenge);
}

// Same, but forcing a synchronous scavenge after the scattered frees so the
// refills recommit and re-zero decommitted, fragmented backing.
void testFragmentationReuseWithScavenge()
{
    runFragmentationReuse(Reuse::WithScavenge);
}

} // anonymous namespace

#endif // PAS_ENABLE_BMALLOC

void addAllocationZeroingTests()
{
#if PAS_ENABLE_BMALLOC
    ADD_TEST(testZeroingReuseSegregatedSizes());
    ADD_TEST(testZeroingReuseBitfitSizes());
    ADD_TEST(testZeroingReuseLargeMemsetSizes());
    ADD_TEST(testZeroingReuseLargeVirtualMemorySizes());
    ADD_TEST(testZeroingReuseWithScavengeSegregatedSizes());
    ADD_TEST(testZeroingReuseWithScavengeBitfitSizes());
    ADD_TEST(testZeroingReuseWithScavengeLargeMemsetSizes());
    ADD_TEST(testZeroingReuseWithScavengeLargeVirtualMemorySizes());
    ADD_TEST(testSmallZeroingPageBatching());
    ADD_TEST(testLargeZeroingFaultOrder());
    ADD_TEST(testFragmentationReuse());
    ADD_TEST(testFragmentationReuseWithScavenge());
#endif // PAS_ENABLE_BMALLOC
}
