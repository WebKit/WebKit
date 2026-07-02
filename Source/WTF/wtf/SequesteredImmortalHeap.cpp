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

#include "config.h"
#include <wtf/SequesteredImmortalHeap.h>

#if USE(PROTECTED_JIT)

#include <bmalloc/pas_scavenger.h>

namespace WTF {

GranuleHeader* SequesteredLargeHeap::acquireHeader()
{
    if (!m_emptyHeaders.isEmpty())
        return m_emptyHeaders.removeHead();
    return reinterpret_cast<GranuleHeader*>(
        SequesteredImmortalHeap::instance().immortalMalloc(sizeof(GranuleHeader)));
}

void SequesteredLargeHeap::insertSorted(GranuleHeader* gran)
{
    auto base = reinterpret_cast<uintptr_t>(gran->m_largeBase);
    GranuleHeader* prev = nullptr;
    for (auto* node = m_decommittedList.head(); node; node = node->next()) {
        if (reinterpret_cast<uintptr_t>(node->m_largeBase) > base)
            break;
        prev = node;
    }
    if (prev)
        m_decommittedList.insertAfter(prev, gran);
    else
        m_decommittedList.push(gran);
}

GranuleHeader* SequesteredLargeHeap::allocate(size_t bytes)
{
    Locker lock(m_lock);

    for (auto* node = m_decommittedList.head(); node; node = node->next()) {
        if (node->m_largeSize >= bytes) {
            if (node->m_largeSize != bytes) {
                auto* remainder = acquireHeader();
                remainder->m_type = GranuleHeader::Type::Large;
                remainder->m_largeBase = reinterpret_cast<void*>(
                    reinterpret_cast<uintptr_t>(node->m_largeBase) + bytes);
                remainder->m_largeSize = node->m_largeSize - bytes;

                m_decommittedList.insertAfter(node, remainder);

                node->m_largeSize = bytes;
            }
            m_decommittedList.remove(node);

            while (madvise(node->m_largeBase, bytes, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
            m_liveMap.add(reinterpret_cast<uintptr_t>(node->m_largeBase), node);
            return node;
        }
    }

    mach_vm_address_t addr = 0;
    auto kr = mach_vm_map(mach_task_self(), &addr, bytes,
        (size_t(1) << largeAlignShift) - 1, VM_FLAGS_ANYWHERE,
        MEMORY_OBJECT_NULL, 0, false,
        VM_PROT_READ | VM_PROT_WRITE,
        VM_PROT_READ | VM_PROT_WRITE,
        VM_INHERIT_DEFAULT);
    if (kr != KERN_SUCCESS)
        return nullptr;

    auto* gran = acquireHeader();
    gran->m_type = GranuleHeader::Type::Large;
    gran->m_largeBase = reinterpret_cast<void*>(addr);
    gran->m_largeSize = bytes;
    m_liveMap.add(static_cast<uintptr_t>(addr), gran);
    return gran;
}

void SequesteredLargeHeap::decommit(GranuleHeader* gran)
{
    Locker lock(m_lock);

    auto it = m_liveMap.find(reinterpret_cast<uintptr_t>(gran->m_largeBase));
    RELEASE_ASSERT(it != m_liveMap.end());
    m_liveMap.remove(it);

    while (madvise(gran->m_largeBase, gran->m_largeSize, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }

    insertSorted(gran);

    auto* prev = gran->prev();
    if (prev && reinterpret_cast<uintptr_t>(prev->m_largeBase) + prev->m_largeSize
        == reinterpret_cast<uintptr_t>(gran->m_largeBase)) {
        prev->m_largeSize += gran->m_largeSize;
        m_decommittedList.remove(gran);
        releaseHeader(gran);
        gran = prev;
    }

    auto* next = gran->next();
    if (next && reinterpret_cast<uintptr_t>(gran->m_largeBase) + gran->m_largeSize
        == reinterpret_cast<uintptr_t>(next->m_largeBase)) {
        gran->m_largeSize += next->m_largeSize;
        m_decommittedList.remove(next);
        releaseHeader(next);
    }
}

SequesteredImmortalHeap& SequesteredImmortalHeap::instance()
{
    // FIXME: this storage is not contained within the sequestered region
    static LazyNeverDestroyed<SequesteredImmortalHeap> instance;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        instance.construct();
    });
    return instance.get();
}

void ConcurrentDecommitQueue::decommit()
{
    auto lst = acquireExclusiveCopyOfGranuleList();

    auto* curr = lst.removeHead();
    if (!curr)
        return;

    auto& sih = SequesteredImmortalHeap::instance();

    size_t decommitPageCount { 0 };
    size_t decommitGranuleCount { 0 };
    UNUSED_VARIABLE(decommitPageCount);
    UNUSED_VARIABLE(decommitGranuleCount);

    do {
        auto pages = sih.granuleProvider().decommitGranule(curr);

        dataLogLnIf(verbose,
            "ConcurrentDecommitQueue: decommitted granule at (",
            RawPointer(curr), ") (", pages, " pages)");

        decommitPageCount += pages;
        decommitGranuleCount++;

        curr = lst.removeHead();
    } while (curr);

    dataLogLnIf(verbose, "ConcurrentDecommitQueue: decommitted ",
        decommitGranuleCount, " granules (", decommitPageCount, " pages)");
}

void SequesteredImmortalHeap::installScavenger()
{
    RELEASE_ASSERT(pas_scavenger_try_install_foreign_work_callback(scavenge, 11, nullptr));
}

bool SequesteredImmortalHeap::scavengeImpl(void* /*userdata*/)
{
    dataLogLnIf(verbose, "SequesteredImmortalHeap: scavenging");
    {
        Locker listLocker { m_scavengerLock };
        auto bound = m_slotManager.allocatedCount();
        for (size_t i = 0; i < bound; i++) {
            auto& queue = *reinterpret_cast<ConcurrentDecommitQueue*>(&m_slotManager[i]);
            queue.decommit();
        }
    }
    return false;
}

SequesteredStackAllocator::Result SequesteredStackAllocator::allocate(size_t stackSize, size_t guardSize)
{
    {
        Locker lock(m_lock);
        if (!m_freeList.isEmpty()) {
            auto* handle = m_freeList.removeHead();
            m_inUseList.append(handle);
            return { handle };
        }
    }

    auto& sih = SequesteredImmortalHeap::instance();

    void* handleMemory = sih.immortalMalloc(sizeof(StackHandle));
    auto* handle = new (handleMemory) StackHandle();

    size_t totalSize = stackSize + guardSize;
    void* stackMemory = sih.immortalAlignedMalloc(pageSize(), totalSize);

    int result = mprotect(stackMemory, guardSize, PROT_NONE);
    RELEASE_ASSERT(!result);

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN;
    handle->stack = std::span<std::byte>(
        reinterpret_cast<std::byte*>(reinterpret_cast<uintptr_t>(stackMemory) + guardSize),
        stackSize
    );
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END;

    {
        Locker lock(m_lock);
        m_inUseList.append(handle);
    }
    return { handle };
}

void SequesteredStackAllocator::deallocate(StackHandle* handle)
{
    Locker lock(m_lock);

#if ASSERT_ENABLED
    {
        bool found = false;
        for (auto* h = m_inUseList.head(); h; h = h->next()) {
            if (h == handle) {
                found = true;
                break;
            }
        }
        RELEASE_ASSERT(found);
    }
#endif

    m_inUseList.remove(handle);
    m_freeList.push(handle);
}

GranuleHeader* SequesteredImmortalAllocator::addGranule(size_t minSizeBytes)
{
    RELEASE_ASSERT(minSizeBytes <= inlineGranuleSize - sizeof(GranuleHeader));
    using AllocationFailureMode = SequesteredGranuleProvider::AllocationFailureMode;
    GranuleHeader* granule = SequesteredImmortalHeap::instance().granuleProvider().mapGranule<AllocationFailureMode::Assert>(minSizeBytes);

    if (granule->m_type == GranuleHeader::Type::Large) {
        m_granules.append(granule);
        dataLogLnIf(verbose,
            "SequesteredImmortalAllocator at ", RawPointer(this),
            ": large allocation: ", granule->m_largeSize, "B at ", RawPointer(granule->payload()));
    } else {
        m_granules.push(granule);
        auto* base = granule->payload();
        m_allocHead = reinterpret_cast<uintptr_t>(base);
        m_allocBound = reinterpret_cast<uintptr_t>(base) + granule->size();
        dataLogLnIf(verbose,
            "SequesteredImmortalAllocator at ", RawPointer(this),
            ": expanded: granule was (", RawPointer(m_granules.head()->next()),
            "), now (", RawPointer(m_granules.head()),
            "); allocHead (",
            RawPointer(reinterpret_cast<void*>(m_allocHead)),
            "), allocBound (",
            RawPointer(reinterpret_cast<void*>(m_allocBound)),
            ")");
    }

    return granule;
}

}

#endif // USE(PROTECTED_JIT)
