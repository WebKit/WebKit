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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseMemory.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <limits>
#include <span>
#include <unistd.h>
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSCToolsTest {

using JSC::Corpse::Address;
using JSC::Corpse::Memory;
using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;

#if OS(DARWIN)

namespace {

// The arena's page layout, all of it set up before the corpse is taken. The pages the
// kernel will not hand back are what the error cases below are about.
enum ArenaPage : unsigned {
    Readable0 = 0,  // Three contiguous readable pages, so a range can span them.
    Readable1 = 1,
    Readable2 = 2,
    Hole = 3,       // Deallocated, so the arena has a gap in the middle.
    ReadOnly = 4,   // A region of its own, whose neighbour...
    NeighbourOfReadOnly = 5, // ...is a separate region with different protections.
    NoAccess = 6,   // Mapped but inaccessible, like a reservation never committed.
    Readable3 = 7,
};
constexpr unsigned arenaPageCount = 8;

constexpr uint64_t patternBase = 0xC03B5E0000000000ull;
uint64_t patternFor(unsigned page) { return patternBase | page; }

constexpr uint64_t tailPatternBase = 0x7A11E00000000000ull;
uint64_t tailPatternFor(unsigned page) { return tailPatternBase | page; }

// Each page holds one pattern at its first word and another at its last, so a read can
// be attributed to its page and contiguity across a boundary can be checked.
class Arena {
public:
    bool build()
    {
        m_pageSize = Memory::pageSize();
        kern_return_t kr = mach_vm_allocate(mach_task_self(), &m_base, arenaPageCount * m_pageSize, VM_FLAGS_ANYWHERE);
        if (kr != KERN_SUCCESS)
            return false;

        for (unsigned page = 0; page < arenaPageCount; ++page) {
            *headOf(page) = patternFor(page);
            *tailOf(page) = tailPatternFor(page);
        }

        if (mach_vm_deallocate(mach_task_self(), addressOf(Hole), m_pageSize) != KERN_SUCCESS)
            return false;
        if (mach_vm_protect(mach_task_self(), addressOf(ReadOnly), m_pageSize, FALSE, VM_PROT_READ) != KERN_SUCCESS)
            return false;
        if (mach_vm_protect(mach_task_self(), addressOf(NoAccess), m_pageSize, FALSE, VM_PROT_NONE) != KERN_SUCCESS)
            return false;
        return true;
    }

    size_t pageSize() const { return m_pageSize; }
    mach_vm_address_t addressOf(unsigned page) const { return m_base + page * m_pageSize; }
    Address corpseAddressOf(unsigned page) const { return Address(addressOf(page)); }

    // The last word of a page, which is where a range that crosses into the next page
    // starts in the tests below.
    Address corpseAddressOfTailOf(unsigned page) const
    {
        return Address(addressOf(page) + m_pageSize - sizeof(uint64_t));
    }

private:
    uint64_t* headOf(unsigned page) const { return reinterpret_cast<uint64_t*>(addressOf(page)); }
    uint64_t* tailOf(unsigned page) const
    {
        return reinterpret_cast<uint64_t*>(addressOf(page) + m_pageSize - sizeof(uint64_t));
    }

    mach_vm_address_t m_base { 0 };
    size_t m_pageSize { 0 };
};

// Takes a std::span, so that handing a Memory::Span straight to it is what checks a
// handle stands in for one.
uint64_t firstWordOf(std::span<const uint8_t> bytes)
{
    RELEASE_ASSERT(bytes.size() >= sizeof(uint64_t));
    uint64_t word = 0;
    memcpySpan(asMutableByteSpan(word), bytes.first(sizeof(uint64_t)));
    return word;
}

} // anonymous namespace

void testMemory()
{
    SuiteTracer tracer("Memory");
    if (!tracer.shouldRun())
        return;

    Arena arena;
    if (!arena.build()) {
        TEST_ASSERT(false, "the test arena could be built");
        return;
    }
    size_t pageSize = arena.pageSize();

    // The corpse is taken after the arena is laid out, so it holds the same layout at
    // the same addresses.
    RefPtr<Process> process = Process::create(getpid());
    if (!process->attach()) {
        TEST_ASSERT(false, "attaching to this process succeeds");
        return;
    }
    Snapshot snapshot(process);
    if (!snapshot.isValid()) {
        TEST_ASSERT(false, "a snapshot of this process is valid");
        return;
    }
    Memory& memory = snapshot.memory();
    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0), "a new Memory has nothing mapped");

    {
        auto word = memory.ptr<uint64_t>(arena.corpseAddressOf(Readable0));
        TEST_ASSERT(word.isValid(), "a readable page maps");
        TEST_ASSERT(static_cast<bool>(word), "a valid Ptr is true");
        TEST_ASSERT_EQ(word.error(), Memory::Error::None, "a mapped range reports no error");
        TEST_ASSERT_HEX_EQ(*word, patternFor(Readable0), "it reads back the page's pattern");
        TEST_ASSERT_EQ(word.sizeInBytes(), sizeof(uint64_t), "it is as large as the type it was read as");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "one region is live");
        TEST_ASSERT_EQ(memory.mappedPageCount(), static_cast<size_t>(1), "covering one page");
    }
    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0),
        "the region is released as soon as the last handle on it dies");

    {
        // A second request for the same page rides on the first region, whichever kind of
        // handle asks for it.
        auto first = memory.ptr<uint64_t>(arena.corpseAddressOf(Readable0));
        auto second = memory.span<uint64_t>(arena.corpseAddressOf(Readable0), 1);
        TEST_ASSERT(first.isValid() && second.isValid(), "both requests for one page are valid");
        TEST_ASSERT(first.get() == second.data(), "they land at the same local address");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "and share one region");
    }

    {
        // An offset into a page maps the page, and the handle points at the offset.
        auto atOffset = memory.span<uint8_t>(arena.corpseAddressOf(Readable1) + 64, sizeof(uint64_t));
        TEST_ASSERT(atOffset.isValid(), "an address partway into a page maps");
        auto wholePage = memory.span<uint8_t>(arena.corpseAddressOf(Readable1), sizeof(uint64_t));
        TEST_ASSERT(wholePage.isValid(), "so does the page it is in");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1),
            "both come from the same region, keyed on the page they share");
        TEST_ASSERT(atOffset.data() == wholePage.data() + 64, "the offset within the page is preserved");
    }

    {
        // The case a single-page region cannot serve: a range that crosses into the next
        // page has to be one contiguous mapping of both.
        auto words = memory.span<uint64_t>(arena.corpseAddressOfTailOf(Readable1), 2);
        TEST_ASSERT(words.isValid(), "a range crossing a page boundary maps");
        TEST_ASSERT_EQ(memory.mappedPageCount(), static_cast<size_t>(2), "as two pages");
        TEST_ASSERT_EQ(words.size(), static_cast<size_t>(2), "it is sized in elements");
        TEST_ASSERT_EQ(words.sizeInBytes(), 2 * sizeof(uint64_t), "and in bytes");
        TEST_ASSERT_HEX_EQ(words[0], tailPatternFor(Readable1), "its first word is the last word of the first page");
        TEST_ASSERT_HEX_EQ(words[1], patternFor(Readable2),
            "and the word after it is the next page's first, so the two pages are contiguous");
    }

    {
        // Nothing shorter is mapped, so a one-page request rides on the longer region
        // rather than making one of its own.
        auto threePages = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), 3 * pageSize);
        TEST_ASSERT(threePages.isValid(), "a three-page range maps");
        auto onePage = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), sizeof(uint64_t));
        TEST_ASSERT(onePage.isValid(), "a one-page range at the same base maps");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "out of the region already there");
        TEST_ASSERT(onePage.data() == threePages.data(), "at the same local address");
    }

    {
        // Smallest fit: with regions of two lengths at one base, a request takes the
        // shortest that can hold it, so a short read does not pin a longer region.
        auto onePage = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), sizeof(uint64_t));
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "a one-page region to start with");

        // Three pages cannot come out of a one-page region.
        auto threePages = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), 3 * pageSize);
        TEST_ASSERT(threePages.isValid(), "a three-page range at the same base maps");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(2), "as a second region");
        TEST_ASSERT(threePages.data() != onePage.data(), "at a local address of its own");

        auto onePageAgain = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), sizeof(uint64_t));
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(2), "another one-page request adds no region");
        TEST_ASSERT(onePageAgain.data() == onePage.data(), "and takes the one-page region, not the longer one");

        auto twoPages = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), pageSize + sizeof(uint64_t));
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(2), "nor does a two-page request");
        TEST_ASSERT(twoPages.data() == threePages.data(),
            "which the one-page region cannot hold, so it takes the three-page one");
    }
    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0), "all of those are released");

    {
        // Moving a handle moves its reference with it rather than duplicating it.
        Memory::Span<uint64_t> moved;
        TEST_ASSERT(!moved.isValid(), "a default Span is invalid");
        TEST_ASSERT(!moved.data(), "and reads as a null pointer");
        TEST_ASSERT(moved.empty(), "and holds nothing");
        {
            auto span = memory.span<uint64_t>(arena.corpseAddressOf(Readable3), 1);
            moved = WTF::move(span);
            TEST_ASSERT(!span.isValid(), "a moved-from Span is invalid");
            TEST_ASSERT(span.empty(), "and holds nothing");
        }
        TEST_ASSERT(moved.isValid(), "the moved-to Span still maps");
        TEST_ASSERT_HEX_EQ(moved[0], patternFor(Readable3), "and still reads");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "holding the one region");
    }
    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0), "which the move did not release twice");

    {
        // A Ptr carries its reference the same way.
        Memory::Ptr<uint64_t> moved;
        TEST_ASSERT(!moved, "a default Ptr is invalid");
        TEST_ASSERT(!moved.get(), "and reads as a null pointer");
        {
            auto word = memory.ptr<uint64_t>(arena.corpseAddressOf(Readable3));
            moved = WTF::move(word);
            TEST_ASSERT(!word, "a moved-from Ptr is invalid");
        }
        TEST_ASSERT_HEX_EQ(*moved, patternFor(Readable3), "the moved-to one still reads");
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(1), "holding the one region");
    }
    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0), "which that move did not release twice");

    {
        // One call covers two regions with different protections, and a Span stands in
        // for a std::span, which is what lets anything that parses bytes read it without
        // knowing about corpses.
        auto bytes = memory.span<uint8_t>(arena.corpseAddressOf(ReadOnly), pageSize + sizeof(uint64_t));
        TEST_ASSERT(bytes.isValid(), "a range spanning two regions maps");
        TEST_ASSERT_EQ(bytes.sizeInBytes(), pageSize + sizeof(uint64_t), "as large as was asked for");
        TEST_ASSERT_HEX_EQ(firstWordOf(bytes), patternFor(ReadOnly),
            "a Span passes for a std::span, and the read-only page reads back");
        TEST_ASSERT_HEX_EQ(firstWordOf(bytes.subspan(pageSize)), patternFor(NeighbourOfReadOnly),
            "so does its neighbour, through a subspan");
        TEST_ASSERT_HEX_EQ(bytes.front(), patternFor(ReadOnly) & 0xff, "front() is the first byte of the range");
        TEST_ASSERT_EQ(bytes.first(4).size(), static_cast<size_t>(4), "first(n) is n long");
        TEST_ASSERT_EQ(bytes.last(4).size(), static_cast<size_t>(4), "and so is last(n)");
    }

    {
        // Iteration, so that a handle works with anything that walks a range.
        auto words = memory.span<uint64_t>(arena.corpseAddressOf(Readable0), 2);
        TEST_ASSERT(words.isValid(), "a two-element range maps");
        size_t visited = 0;
        uint64_t firstSeen = 0;
        for (uint64_t word : words) {
            if (!visited)
                firstSeen = word;
            ++visited;
        }
        TEST_ASSERT_EQ(visited, static_cast<size_t>(2), "a range-for visits every element");
        TEST_ASSERT_HEX_EQ(firstSeen, patternFor(Readable0), "starting at the first");
        TEST_ASSERT_HEX_EQ(words.back(), *(words.data() + 1), "back() is the last element");
    }

    {
        auto span = memory.span<uint8_t>(arena.corpseAddressOf(Hole), sizeof(uint64_t));
        TEST_ASSERT(!span.isValid(), "an unmapped page does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::UnmappedHole, "and says the range is not mapped");
        TEST_ASSERT(!span.data(), "and reads as a null pointer");
        TEST_ASSERT(span.empty(), "and holds nothing");
        TEST_ASSERT(std::span<const uint8_t>(span).empty(), "so the std::span it stands in for is empty too");
    }
    {
        auto span = memory.span<uint8_t>(arena.corpseAddressOf(Readable2), 2 * pageSize);
        TEST_ASSERT(!span.isValid(), "a range running into a hole does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::UnmappedHole, "for the same reason");
    }
    {
        auto span = memory.span<uint8_t>(arena.corpseAddressOf(NoAccess), sizeof(uint64_t));
        TEST_ASSERT(!span.isValid(), "an inaccessible page does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::NotReadable, "and says it denies read access");
    }
    {
        auto span = memory.span<uint8_t>(arena.corpseAddressOf(Readable0), 0);
        TEST_ASSERT(!span.isValid(), "a zero-length range does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::InvalidRequest, "and is rejected as a bad request");
    }
    {
        auto span = memory.span<uint8_t>(Address(static_cast<mach_vm_address_t>(8)), sizeof(uint64_t));
        TEST_ASSERT(!span.isValid(), "a near-null address does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::InvalidRequest, "and is rejected as a bad request");
    }
    {
        auto span = memory.span<uint8_t>(arena.corpseAddressOf(Readable0),
            std::numeric_limits<size_t>::max());
        TEST_ASSERT(!span.isValid(), "a range that runs off the address space does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::InvalidRequest, "and is rejected as a bad request");
    }
    {
        // An element count whose byte length overflows is caught before it can wrap into
        // a request that looks reasonable.
        auto span = memory.span<uint64_t>(arena.corpseAddressOf(Readable0),
            std::numeric_limits<size_t>::max() / 4);
        TEST_ASSERT(!span.isValid(), "an element count too large to size does not map");
        TEST_ASSERT_EQ(span.error(), Memory::Error::InvalidRequest, "and is rejected as a bad request");
    }

    {
        auto word = memory.ptr<uint64_t>(arena.corpseAddressOf(Hole));
        TEST_ASSERT(!word, "ptr<T> on an unmapped page does not map");
        TEST_ASSERT(!word.get(), "and reads as a null pointer");
        TEST_ASSERT_EQ(word.error(), Memory::Error::UnmappedHole, "and reports why");
        TEST_ASSERT_EQ(word.sizeInBytes(), static_cast<size_t>(0), "and holds nothing");
    }

    TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0), "nothing is left mapped");

    {
        // Mapping the same page many times must not leak address space.
        static constexpr unsigned rounds = 200;
        for (unsigned round = 0; round < rounds; ++round) {
            auto word = memory.ptr<uint64_t>(arena.corpseAddressOf(Readable0));
            TEST_ASSERT(word.isValid(), "every round maps");
        }
        TEST_ASSERT_EQ(memory.regionCount(), static_cast<size_t>(0),
            "repeated mapping and release leaves nothing behind");
    }
}

#else

void testMemory()
{
    SuiteTracer tracer("Memory");
    if (!tracer.shouldRun())
        return;
    linuxSkip("Memory", "the test arena is laid out with Mach VM calls");
}

#endif // OS(DARWIN)

} // namespace JSCToolsTest

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(MYA)
