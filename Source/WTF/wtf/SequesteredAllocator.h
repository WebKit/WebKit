/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>

#if USE(PROTECTED_JIT)

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <sys/mman.h>
#include <unistd.h>

#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/Lock.h>
#include <wtf/SequesteredImmortalHeap.h>
#include <wtf/StackTrace.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StdMap.h>
#include <wtf/text/WTFString.h>

namespace WTF {

constexpr size_t minArenaGranuleSize { 512 * KB };
constexpr size_t sequesteredArenaAllocatorAlignment { 128 };

class alignas(sequesteredArenaAllocatorAlignment) SequesteredArenaAllocator {
private:
    using AllocationFailureMode = SequesteredImmortalHeap::AllocationFailureMode;

    constexpr static bool verbose { false };
    constexpr static bool trackAllocationDebugInfo { false };
    constexpr static bool eagerlyDecommit { false };
    constexpr static size_t pageSize { 16 * KB };

    class Arena {
        friend SequesteredArenaAllocator;
        constexpr static bool verbose { false };
        constexpr static bool canExpand { true };

    public:
        enum class TopGranulePolicy {
            Retain,
            DoNotRetain,
        };

        Arena() = default;

        Arena(Arena&& other) = delete;
        Arena& operator=(Arena&& other) = delete;
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;

        void* allocate(size_t bytes)
        {
            auto retval = allocateImpl<AllocationFailureMode::Assert>(bytes);
            dataLogLnIf(verbose,
                "Allocator ", parent().id(),
                ": Arena ", parent().debugIndexOfArena(*this),
                ": allocated ", bytes, "B: alloc (", RawPointer(retval),
                "), allocHead (", RawPointer(reinterpret_cast<void*>(m_allocHead)),
                ")");
            return retval;
        }

        void* tryAllocate(size_t bytes)
        {
            auto retval = allocateImpl<AllocationFailureMode::ReturnNull>(bytes);
            dataLogLnIf(verbose && retval,
                "Allocator ", parent().id(),
                ": Arena ", parent().debugIndexOfArena(*this),
                ": allocated ", bytes, "B: alloc (", RawPointer(retval),
                "), allocHead (", RawPointer(reinterpret_cast<void*>(m_allocHead)),
                ")");
            return retval;
        }

        void* alignedAllocate(size_t alignment, size_t bytes)
        {
            auto retval = alignedAllocateImpl<AllocationFailureMode::Assert>(alignment, bytes);
            dataLogLnIf(verbose,
                "Allocator ", parent().id(),
                ": Arena ", parent().debugIndexOfArena(*this),
                ": align-allocated ", bytes, "B: alloc (", RawPointer(retval),
                "), allocHead (",
                RawPointer(reinterpret_cast<void*>(m_allocHead)),
                ")");
            return retval;
        }

        void* tryAlignedAllocate(size_t alignment, size_t bytes)
        {
            auto retval = alignedAllocateImpl<AllocationFailureMode::ReturnNull>(alignment, bytes);
            dataLogLnIf(verbose && retval,
                "Allocator ", parent().id(),
                ": Arena ", parent().debugIndexOfArena(*this),
                ": align-allocated ", bytes, "B: alloc (", RawPointer(retval),
                "), allocHead (",
                RawPointer(reinterpret_cast<void*>(m_allocHead)),
                ")");
            return retval;
        }

        void deallocate(void* /*p*/) { }

        GranuleList reset(TopGranulePolicy retain)
        {
            dataLogLnIf(verbose,
                "Allocator ", parent().id(), ": Arena ", parent().debugIndexOfArena(*this),
                ": ending lifetime: granule is (", currentGranule(),
                "), allocHead (", reinterpret_cast<void*>(m_allocHead),
                "), allocBound (", reinterpret_cast<void*>(m_allocBound), ")");

            GranuleHeader* head { m_granules.head() };
            if (!head || retain == TopGranulePolicy::DoNotRetain) [[unlikely]] {
                if (!head) {
                    ASSERT(!m_allocHead);
                    ASSERT(!m_allocBound);
                }
                m_allocHead = 0;
                m_allocBound = 0;
                return WTF::move(m_granules);
            }

            GranuleList tail { m_granules.splitAt(1) };
            ASSERT(head == m_granules.head());
            setBoundsFromGranule(head);
            return tail;
        }

    private:
        static constexpr size_t minHeadAlignment = alignof(std::max_align_t);

        uintptr_t headIncrementedBy(size_t bytes) const
        {
            const size_t alignmentMask = minHeadAlignment - 1;
            return (m_allocHead + bytes + alignmentMask) & ~alignmentMask;
        }

        // FIXME: convert to headAlignedUpToAndIncrementedBy or similar
        uintptr_t headAlignedUpTo(size_t alignment) const
        {
            ASSERT(alignment);
            const size_t minHeadAlignmentMask = minHeadAlignment - 1;
            const size_t alignmentMask = (alignment - 1) | minHeadAlignmentMask;
            return (m_allocHead + alignmentMask) & ~alignmentMask;
        }

        template<AllocationFailureMode mode>
        void* allocateImpl(size_t bytes)
        {
            uintptr_t allocation = m_allocHead;
            uintptr_t newHead = headIncrementedBy(bytes);
            if (newHead < m_allocBound) [[likely]] {
                m_allocHead = newHead;
                return reinterpret_cast<void*>(allocation);
            }
            if constexpr (!canExpand) {
                if constexpr (mode == AllocationFailureMode::ReturnNull)
                    return nullptr;
                ASSERT(mode == AllocationFailureMode::Assert);
                RELEASE_ASSERT_NOT_REACHED();
            }
            return allocateImplSlowPath<mode>(bytes);
        }

        template <AllocationFailureMode mode>
        void* alignedAllocateImpl(size_t alignment, size_t bytes)
        {
            uintptr_t allocation = headAlignedUpTo(alignment);
            uintptr_t newHead = headIncrementedBy((allocation - m_allocHead) + bytes);
            if (newHead < m_allocBound) [[likely]] {
                m_allocHead = newHead;
                return reinterpret_cast<void*>(allocation);
            }
            if constexpr (!canExpand) {
                if constexpr (mode == AllocationFailureMode::ReturnNull)
                    return nullptr;
                ASSERT(mode == AllocationFailureMode::Assert);
                RELEASE_ASSERT_NOT_REACHED();
            }
            return alignedAllocateImplSlowPath<mode>(alignment, bytes);
        }

        template<AllocationFailureMode mode>
        NEVER_INLINE void* allocateImplSlowPath(size_t bytes)
        {
            auto* granule = addGranule<mode>(bytes);
            if constexpr (mode == AllocationFailureMode::ReturnNull) {
                if (!granule)
                    return nullptr;
            }

            uintptr_t allocation = m_allocHead;
            m_allocHead = headIncrementedBy(bytes);
            ASSERT(m_allocHead <= m_allocBound);

            return reinterpret_cast<void*>(allocation);
        }

        template<AllocationFailureMode mode>
        NEVER_INLINE void* alignedAllocateImplSlowPath(size_t alignment, size_t bytes)
        {
            auto* granule = addGranule<mode>(bytes);
            if constexpr (mode == AllocationFailureMode::ReturnNull) {
                if (!granule)
                    return nullptr;
            }

            uintptr_t allocation = headAlignedUpTo(alignment);
            m_allocHead = headIncrementedBy((allocation - m_allocHead) + bytes);
            ASSERT(m_allocHead <= m_allocBound);

            return reinterpret_cast<void*>(allocation);
        }

        template<AllocationFailureMode mode>
        GranuleHeader* addGranule([[maybe_unused]] size_t minSize)
        {
            ASSERT(minSize <= minArenaGranuleSize);
            GranuleHeader* granule = reinterpret_cast<GranuleHeader*>(mapGranule<mode>(minArenaGranuleSize));
            if constexpr (mode == AllocationFailureMode::ReturnNull) {
                if (!granule) [[unlikely]]
                    return nullptr;
            }

            m_granules.push(granule);
            setBoundsFromGranule(granule);

            static_assert(sizeof(GranuleHeader) >= minHeadAlignment);
            dataLogLnIf(verbose,
                "Allocator ", parent().id(),
                ": Arena ", parent().debugIndexOfArena(*this),
                ": expanded: granule was (", granule->prev(),
                "), now (", granule,
                "); allocHead (",
                RawPointer(reinterpret_cast<void*>(m_allocHead)),
                "), allocBound (",
                RawPointer(reinterpret_cast<void*>(m_allocBound)),
                ")");

            return granule;
        }

        void setBoundsFromGranule(GranuleHeader* granule)
        {
            static_assert(sizeof(GranuleHeader) >= minHeadAlignment);
            m_allocHead = reinterpret_cast<uintptr_t>(granule) + sizeof(GranuleHeader);
            m_allocBound = reinterpret_cast<uintptr_t>(granule) + (pageSize * (1 + granule->additionalPageCount));
        }

        template<AllocationFailureMode mode>
        void* mapGranule(size_t bytes)
        {
            return parent().mapGranule<mode>(bytes);
        }

        SequesteredArenaAllocator& parent()
        {
            return SequesteredArenaAllocator::allocatorForArena(*this);
        }

        GranuleHeader* currentGranule()
        {
            return m_granules.head();
        }

        GranuleList m_granules { };

        uintptr_t m_allocHead { 0 };
        uintptr_t m_allocBound { 0 };
    };
    friend Arena;
public:
    SequesteredArenaAllocator() = default;

    void* malloc(size_t bytes)
    {
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.allocate(bytes);
        registerSuccessfulAllocation(retval, bytes);
        return retval;
    }

    void* alignedMalloc(size_t alignment, size_t bytes)
    {
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.alignedAllocate(alignment, bytes);
        registerSuccessfulAllocation(retval, bytes);
        return retval;
    }

    void* zeroedMalloc(size_t bytes)
    {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.allocate(bytes);
        std::memset(retval, 0, bytes);
        registerSuccessfulAllocation(retval, bytes);
        return retval;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    void* tryMalloc(size_t bytes)
    {
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.tryAllocate(bytes);
        if (retval) [[likely]]
            registerSuccessfulAllocation(retval, bytes);
        return retval;
    }

    void* tryAlignedMalloc(size_t alignment, size_t bytes)
    {
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.tryAlignedAllocate(alignment, bytes);
        if (retval) [[likely]]
            registerSuccessfulAllocation(retval, bytes);
        return retval;
    }

    void* tryZeroedMalloc(size_t bytes)
    {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        ASSERT(m_alive);
        auto retval = m_genericSmallArena.tryAllocate(bytes);
        if (retval) [[likely]] {
            std::memset(retval, 0, bytes);
            registerSuccessfulAllocation(retval, bytes);
        }
        return retval;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    void free(void* p)
    {
        ASSERT(m_alive);
        ASSERT(m_liveAllocations > 0);

        registerSuccessfulFree(p);

        return m_genericSmallArena.deallocate(p);
    }

    void* realloc(void* p, size_t newSize)
    {
        ASSERT(m_alive);
        ASSERT(m_liveAllocations > 0);

        void* newP = m_genericSmallArena.allocate(newSize);
        registerSuccessfulAllocation(newP, newSize);

        return reallocateInto(p, newP, newSize);
    }

    void* tryRealloc(void* p, size_t newSize)
    {
        ASSERT(m_alive);

        void* newP = m_genericSmallArena.tryAllocate(newSize);
        if (!newP)
            return newP;
        registerSuccessfulAllocation(newP, newSize);

        auto oldDebugKey = reinterpret_cast<uintptr_t>(p);
        auto newDebugKey = reinterpret_cast<uintptr_t>(newP);
        RELEASE_ASSERT(m_allocationInfos.count(oldDebugKey) > 0);
        m_allocationInfos[newDebugKey] = m_allocationInfos[oldDebugKey];

        return reallocateInto(p, newP, newSize);
    }

    // Since this is thread-local, we assume beginArenaLifetime and
    // endArenaLifetime cannot race
    void beginArenaLifetime()
    {
        ASSERT(!m_alive);
        m_alive = true;
        m_liveAllocations = 0;
        dataLogLnIf(verbose, "Allocator ", id(), " in thread ", Thread::currentSingleton(),
            ": starting lifetime");
    }

    void endArenaLifetime()
    {
        ASSERT(m_alive);
        if constexpr (trackAllocationDebugInfo) {
            if (m_liveAllocations > 0) {
                logLiveAllocationDebugInfos();
                CRASH();
            }
        } else
            RELEASE_ASSERT(!m_liveAllocations);
        m_alive = false;

        m_decommitQueue.concatenate(m_genericSmallArena.reset(Arena::TopGranulePolicy::Retain));
        if constexpr (!eagerlyDecommit)
            m_decommitQueue.decommit();

        dataLogLnIf(verbose, "Allocator ", id(), " in thread ",
            Thread::currentSingleton(), " ended lifetime: ", m_totalAllocatedBytes, "B allocated");

        if constexpr (verbose)
            m_totalAllocatedBytes = 0;
    }

    bool inArenaLifetime()
    {
        return m_alive;
    }

    int id()
    {
        return SequesteredImmortalHeap::instance().computeSlotIndex(this);
    }

    static SequesteredArenaAllocator* getCurrentAllocator()
    {
        using SelfT = SequesteredArenaAllocator;
        auto ptr = reinterpret_cast<SelfT*>(SequesteredImmortalHeap::instance().getSlot());
        if (!ptr) [[unlikely]] {
            static_assert(sizeof(SequesteredArenaAllocator) <= SequesteredImmortalHeap::slotSize);
            static_assert(!offsetof(SequesteredArenaAllocator, m_decommitQueue));
            ptr = reinterpret_cast<SelfT*>(
                SequesteredImmortalHeap::instance().allocateAndInstall<SelfT>());
        }
        ASSERT(ptr);
        return ptr;
    }
private:
    struct AllocationDebugInfo {
        size_t size;
        String stackTrace;
        String proximateFrame;
        bool live;
    };

    ALWAYS_INLINE void registerSuccessfulAllocation(void* p, size_t bytes) {
        m_liveAllocations++;
        if constexpr (verbose)
            m_totalAllocatedBytes += bytes;

        if constexpr (trackAllocationDebugInfo) {
            auto debugKey = reinterpret_cast<uintptr_t>(p);
            RELEASE_ASSERT(!m_allocationInfos.count(debugKey));

            auto fullTrace = StackTrace::captureStackTrace(1000, 3);
            auto localTrace = StackTrace::captureStackTrace(1, 1);
            AllocationDebugInfo info {
                .size = bytes,
                .stackTrace = fullTrace->toString(),
                .proximateFrame = localTrace->toString(),
                .live = true,
            };
            m_allocationInfos.emplace(std::make_pair(debugKey, info));
        }
    }

    void registerSuccessfulFree(void* p)
    {
        m_liveAllocations--;

        if constexpr (trackAllocationDebugInfo) {
            auto debugKey = reinterpret_cast<uintptr_t>(p);
            RELEASE_ASSERT(m_allocationInfos.count(debugKey) > 0);
            m_allocationInfos[debugKey].live = false;
            m_allocationInfos.erase(debugKey);
        }
    }

    void registerSuccessfulReallocation(void* from, void* to, size_t newSize)
    {
        if constexpr (trackAllocationDebugInfo) {
            auto oldDebugKey = reinterpret_cast<uintptr_t>(from);
            auto newDebugKey = reinterpret_cast<uintptr_t>(to);
            RELEASE_ASSERT(m_allocationInfos.count(oldDebugKey) > 0);
            m_allocationInfos[newDebugKey] = m_allocationInfos[oldDebugKey];
            m_allocationInfos[oldDebugKey].live = false;
            m_allocationInfos[newDebugKey].size = newSize;
        }
    }

    void logLiveAllocationDebugInfos();

    static SequesteredArenaAllocator& allocatorForArena(Arena& arena)
    {
        // This is legal since the existence of a valid Arena& implies the presence
        // of its parent allocator
        uintptr_t arenaInt = reinterpret_cast<uintptr_t>(&arena);
        uintptr_t allocatorInt = (arenaInt / sequesteredArenaAllocatorAlignment)
            * sequesteredArenaAllocatorAlignment;
        return *reinterpret_cast<SequesteredArenaAllocator*>(allocatorInt);
    }

    int debugIndexOfArena(Arena& arena)
    {
        // Only for use in debug/logging -- arenas should not otherwise
        // need to know about their "identity"
        uintptr_t target = reinterpret_cast<uintptr_t>(&arena);
        uintptr_t thisUintptr = reinterpret_cast<uintptr_t>(this);
        uintptr_t firstArena = thisUintptr
            + offsetof(SequesteredArenaAllocator, m_genericSmallArena);
        RELEASE_ASSERT(target >= firstArena && target <= (thisUintptr + sizeof(*this)));

        return (target - firstArena) / sizeof(arena);
    }

    template<AllocationFailureMode mode>
    void* mapGranule(size_t bytes)
    {
        return SequesteredImmortalHeap::instance().mapGranule<mode>(bytes);
    }

    void* reallocateInto(void* from, void* to, size_t toSize)
    {
        // FIXME: reimplement realloc properly, if necessary
        // Hopefully it will not be since it would require us to store the size
        uintptr_t fromInt = reinterpret_cast<uintptr_t>(from);
        uintptr_t nextPageAfterFrom = (fromInt + pageSize - 1) & ~(pageSize - 1);
        size_t maxSafeCopySize = nextPageAfterFrom - fromInt;

        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        size_t copySize = std::min(toSize, maxSafeCopySize);
        std::memcpy(to, from, copySize);
        free(from);
        return to;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    ConcurrentDecommitQueue m_decommitQueue { };
    size_t m_liveAllocations { 0 };
    bool m_alive { false };

    // m_genericSmallArena must be the first Arena member of this class
    // in order for debugIndexOfArena to work
    Arena m_genericSmallArena { };

    // FIXME: wrap this in a child class so we can ifdef it out
    // when we don't need it
    StdMap<uintptr_t, AllocationDebugInfo> m_allocationInfos;
    size_t m_totalAllocatedBytes { 0 };
};

class ArenaLifetime {
public:
    ArenaLifetime()
        : m_slot(SequesteredArenaAllocator::getCurrentAllocator())
    {
        m_slot->beginArenaLifetime();
    }
    ~ArenaLifetime()
    {
        // Should not be created/destroyed in different threads
        ASSERT(SequesteredArenaAllocator::getCurrentAllocator() == m_slot);
        m_slot->endArenaLifetime();
    }
    ArenaLifetime(ArenaLifetime&& other) = delete;
    ArenaLifetime& operator=(ArenaLifetime&&) = delete;
    ArenaLifetime(const ArenaLifetime&) = delete;
    ArenaLifetime& operator=(const ArenaLifetime&) = delete;

    static bool isAlive()
    {
        return SequesteredArenaAllocator::getCurrentAllocator()->inArenaLifetime();
    }
private:
    SequesteredArenaAllocator* m_slot;
};

} // namespace WTF

using WTF::ArenaLifetime;

#endif // USE(PROTECTED_JIT)
