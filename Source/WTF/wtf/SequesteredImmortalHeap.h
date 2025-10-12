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
#include <wtf/StdLibExtras.h>

#if USE(PROTECTED_JIT)

#include <cstddef>
#include <cstdint>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wtf/Assertions.h>
#include <wtf/Atomics.h>
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Lock.h>
#include <wtf/PageBlock.h>
#include <wtf/Threading.h>

#if OS(DARWIN)
#include <pthread/tsd_private.h>
#endif

namespace WTF {

struct GranuleHeader : public DoublyLinkedListNode<GranuleHeader> {
    // non-inclusive of the page this is on
    // so a value of 0 encodes 1 total pages
    GranuleHeader* m_prev;
    GranuleHeader* m_next;
    size_t additionalPageCount;
};
using GranuleList = DoublyLinkedList<GranuleHeader>;


class ConcurrentDecommitQueue {
    static constexpr bool verbose { false };
public:
    void concatenate(GranuleList&& granules)
    {
        if (granules.isEmpty())
            return;

        {
            Locker lock(m_decommitLock);
            m_granules.append(granules);
            granules.clear();
        }
    }

    void decommit();
private:
    GranuleList acquireExclusiveCopyOfGranuleList()
    {
        GranuleList granules { };
        {
            Locker lock(m_decommitLock);
            granules = m_granules;
            m_granules.clear();
        }
        return granules;
    }

    GranuleList m_granules { };
    Lock m_decommitLock { };
};

// FIXME: a lot of this, but not all, can be de-duped with SequesteredArenaAllocator::Arena
class SequesteredImmortalAllocator {
    constexpr static bool verbose { false };
    constexpr static size_t minGranuleSize { 16 * KB };
    constexpr static size_t minHeadAlignment { alignof(std::max_align_t) };
public:
    SequesteredImmortalAllocator() = default;

    SequesteredImmortalAllocator(SequesteredImmortalAllocator&& other) = delete;
    SequesteredImmortalAllocator& operator=(SequesteredImmortalAllocator&& other) = delete;
    SequesteredImmortalAllocator(const SequesteredImmortalAllocator&) = delete;
    SequesteredImmortalAllocator& operator=(const SequesteredImmortalAllocator&) = delete;

    void* allocate(size_t bytes)
    {
        void* retval { };
        void* newAllocHead { };
        {
            Locker lock(m_lock);
            retval = allocateImpl(bytes);
            newAllocHead = reinterpret_cast<void*>(m_allocHead);
        }
        dataLogLnIf(verbose,
            "SequesteredImmortalAllocator at ", RawPointer(this),
            ": allocated ", bytes, "B: alloc (", RawPointer(retval),
            "), allocHead (", RawPointer(newAllocHead),
            ")");
        return retval;
    }

    void* alignedAllocate(size_t alignment, size_t bytes)
    {
        void* retval { };
        void* newAllocHead { };
        {
            Locker lock(m_lock);
            retval = alignedAllocateImpl(alignment, bytes);
            newAllocHead = reinterpret_cast<void*>(m_allocHead);
        }
        dataLogLnIf(verbose,
            "SequesteredImmortalAllocator at ", RawPointer(this),
            ": align-allocated ", bytes, "B: alloc (", RawPointer(retval),
            "), allocHead (", RawPointer(newAllocHead),
            ")");
        return retval;
    }
private:
    uintptr_t headIncrementedBy(size_t bytes) const
    {
        constexpr size_t alignmentMask = minHeadAlignment - 1;
        return (m_allocHead + bytes + alignmentMask) & ~alignmentMask;
    }

    void* allocateImpl(size_t bytes)
    {
        uintptr_t allocation = m_allocHead;
        uintptr_t newHead = headIncrementedBy(bytes);
        if (newHead < m_allocBound) [[likely]] {
            m_allocHead = newHead;
            return reinterpret_cast<void*>(allocation);
        }
        return allocateImplSlowPath(bytes);
    }

    void* alignedAllocateImpl(size_t alignment, size_t bytes)
    {
        uintptr_t allocation = WTF::roundUpToMultipleOf<minHeadAlignment>(m_allocHead);
        uintptr_t newHead = headIncrementedBy((allocation - m_allocHead) + bytes);
        if (newHead < m_allocBound) [[likely]] {
            m_allocHead = newHead;
            return reinterpret_cast<void*>(allocation);
        }
        return alignedAllocateImplSlowPath(alignment, bytes);
    }

    NEVER_INLINE void* allocateImplSlowPath(size_t bytes)
    {
        addGranule(bytes);

        uintptr_t allocation = m_allocHead;
        m_allocHead = headIncrementedBy(bytes);
        ASSERT(m_allocHead <= m_allocBound);

        return reinterpret_cast<void*>(allocation);
    }

    NEVER_INLINE void* alignedAllocateImplSlowPath(size_t alignment, size_t bytes)
    {
        addGranule(bytes);

        alignment = std::max(alignment, minHeadAlignment);
        uintptr_t allocation = WTF::roundUpToMultipleOf(alignment, m_allocHead);
        m_allocHead = headIncrementedBy((allocation - m_allocHead) + bytes);
        ASSERT(m_allocHead <= m_allocBound);

        return reinterpret_cast<void*>(allocation);
    }

    GranuleHeader* addGranule(size_t minSize);

    GranuleList m_granules { };
    uintptr_t m_allocHead { 0 };
    uintptr_t m_allocBound { 0 };
    Lock m_lock { };
};

class alignas(16 * KB) SequesteredImmortalHeap {
    friend class WTF::LazyNeverDestroyed<SequesteredImmortalHeap>;
    static constexpr bool verbose { false };
    static constexpr pthread_key_t key = __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY0;
    static constexpr size_t sequesteredImmortalHeapSlotSize { 16 * KB };
public:
    static constexpr size_t slotSize { 128 };
    static constexpr size_t numSlots { 64 };

    enum class AllocationFailureMode {
        Assert,
        ReturnNull
    };

    static SequesteredImmortalHeap& instance();

    template <typename T> requires (sizeof(T) <= slotSize)
    T* allocateAndInstall()
    {
        T* slot = nullptr;
        {
            Locker locker { m_scavengerLock };
            ASSERT(!getUnchecked());
            // FIXME: implement resizing to a larger capacity
            RELEASE_ASSERT(m_nextFreeIndex < numSlots);

            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            void* buff = &(m_allocatorSlots[m_nextFreeIndex++]);
            slot = new (buff) T();
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }
        _pthread_setspecific_direct(key, reinterpret_cast<void*>(slot));
        pthread_key_init_np(key, nullptr);

        dataLogIf(verbose, "SequesteredImmortalHeap: thread (", Thread::currentSingleton(), ") allocated slot ", instance().m_nextFreeIndex - 1, " (", slot, ")");
        return slot;
    }

    void* immortalMalloc(size_t bytes)
    {
        return m_immortalAllocator.allocate(bytes);
    }

    void* immortalAlignedMalloc(size_t alignment, size_t bytes)
    {
        return m_immortalAllocator.alignedAllocate(alignment, bytes);
    }

    void* getSlot()
    {
        return getUnchecked();
    }

    int computeSlotIndex(void* slotPtr)
    {
        auto slot = reinterpret_cast<uintptr_t>(slotPtr);
        auto arrayBase = reinterpret_cast<uintptr_t>(m_allocatorSlots.begin());
        auto arrayBound = reinterpret_cast<uintptr_t>(m_allocatorSlots.begin()) + sizeof(m_allocatorSlots);
        ASSERT_UNUSED(arrayBound, slot >= arrayBase && slot < arrayBound);
        return static_cast<int>((slot - arrayBase) / slotSize);
    }

    static bool scavenge(void* userdata)
    {
        auto& sih = instance();
        return sih.scavengeImpl(userdata);
    }

    template<AllocationFailureMode mode>
    GranuleHeader* mapGranule(size_t bytes)
    {
        void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) [[unlikely]] {
            if constexpr (mode == AllocationFailureMode::ReturnNull)
                return nullptr;
            RELEASE_ASSERT_NOT_REACHED();
        }
        auto* gran = reinterpret_cast<GranuleHeader*>(p);
        gran->additionalPageCount = bytes / pageSize() - 1;
        return gran;
    }

    size_t decommitGranule(GranuleHeader* gran)
    {
        size_t pageCount = 1 + gran->additionalPageCount;
        size_t bytes = pageCount * pageSize();

        // FIXME: experiment with other decommit strategies
        auto success = munmap(gran, bytes);
        RELEASE_ASSERT(!success);

        return pageCount;
    }

private:
    SequesteredImmortalHeap()
    {
        RELEASE_ASSERT(!(reinterpret_cast<uintptr_t>(this) % sequesteredImmortalHeapSlotSize));
        static_assert(sizeof(*this) <= sequesteredImmortalHeapSlotSize);

        auto flags = VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | VM_FLAGS_PERMANENT;
        auto prots = VM_PROT_READ | VM_PROT_WRITE;
        auto* self = reinterpret_cast<mach_vm_address_t*>(this);
        mach_vm_map(mach_task_self(), self, sequesteredImmortalHeapSlotSize, sequesteredImmortalHeapSlotSize - 1, flags, MEMORY_OBJECT_NULL, 0, false, prots, prots, VM_INHERIT_DEFAULT);

        installScavenger();

        // Cannot use dataLog here as it takes a lock
        if constexpr (verbose)
            SAFE_FPRINTF(stderr, "SequesteredImmortalHeap: initialized by thread (%u)\n", Thread::currentSingleton().uid());
    }

    void installScavenger();
    bool scavengeImpl(void* userdata);

    static void* getUnchecked()
    {
        return _pthread_getspecific_direct(key);
    }

    struct alignas(slotSize) Slot {
        std::array<std::byte, slotSize> data;
    };

    Lock m_scavengerLock { };
    size_t m_nextFreeIndex { };
    SequesteredImmortalAllocator m_immortalAllocator { };
    std::array<Slot, numSlots> m_allocatorSlots { };
};

}

#endif // USE(PROTECTED_JIT)
