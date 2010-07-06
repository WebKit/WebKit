/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "config.h"

#include "ExecutableAllocator.h"

#if ENABLE(ASSEMBLER) && OS(DARWIN) && CPU(X86_64)

#include <errno.h>

#include "TCSpinLock.h"
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wtf/AVLTree.h>
#include <wtf/VMTags.h>

using namespace WTF;

namespace JSC {

#define TWO_GB (2u * 1024u * 1024u * 1024u)
#define SIXTEEN_MB (16u * 1024u * 1024u)

// FreeListEntry describes a free chunk of memory, stored in the freeList.
struct FreeListEntry {
    FreeListEntry(void* pointer, size_t size)
        : pointer(pointer)
        , size(size)
        , nextEntry(0)
        , less(0)
        , greater(0)
        , balanceFactor(0)
    {
    }

    // All entries of the same size share a single entry
    // in the AVLTree, and are linked together in a linked
    // list, using nextEntry.
    void* pointer;
    size_t size;
    FreeListEntry* nextEntry;

    // These fields are used by AVLTree.
    FreeListEntry* less;
    FreeListEntry* greater;
    int balanceFactor;
};

// Abstractor class for use in AVLTree.
// Nodes in the AVLTree are of type FreeListEntry, keyed on
// (and thus sorted by) their size.
struct AVLTreeAbstractorForFreeList {
    typedef FreeListEntry* handle;
    typedef int32_t size;
    typedef size_t key;

    handle get_less(handle h) { return h->less; }
    void set_less(handle h, handle lh) { h->less = lh; }
    handle get_greater(handle h) { return h->greater; }
    void set_greater(handle h, handle gh) { h->greater = gh; }
    int get_balance_factor(handle h) { return h->balanceFactor; }
    void set_balance_factor(handle h, int bf) { h->balanceFactor = bf; }

    static handle null() { return 0; }

    int compare_key_key(key va, key vb) { return va - vb; }
    int compare_key_node(key k, handle h) { return compare_key_key(k, h->size); }
    int compare_node_node(handle h1, handle h2) { return compare_key_key(h1->size, h2->size); }
};

// Used to reverse sort an array of FreeListEntry pointers.
static int reverseSortFreeListEntriesByPointer(const void* leftPtr, const void* rightPtr)
{
    FreeListEntry* left = *(FreeListEntry**)leftPtr;
    FreeListEntry* right = *(FreeListEntry**)rightPtr;

    return (intptr_t)(right->pointer) - (intptr_t)(left->pointer);
}

// Used to reverse sort an array of pointers.
static int reverseSortCommonSizedAllocations(const void* leftPtr, const void* rightPtr)
{
    void* left = *(void**)leftPtr;
    void* right = *(void**)rightPtr;

    return (intptr_t)right - (intptr_t)left;
}

class FixedVMPoolAllocator
{
    // The free list is stored in a sorted tree.
    typedef AVLTree<AVLTreeAbstractorForFreeList, 40> SizeSortedFreeTree;

    // Use madvise as apropriate to prevent freed pages from being spilled,
    // and to attempt to ensure that used memory is reported correctly.
#if HAVE(MADV_FREE_REUSE)
    void release(void* position, size_t size)
    {
        while (madvise(position, size, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
    }

    void reuse(void* position, size_t size)
    {
        while (madvise(position, size, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
    }
#elif HAVE(MADV_DONTNEED)
    void release(void* position, size_t size)
    {
        while (madvise(position, size, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
    }

    void reuse(void*, size_t) {}
#else
    void release(void*, size_t) {}
    void reuse(void*, size_t) {}
#endif

    // All addition to the free list should go through this method, rather than
    // calling insert directly, to avoid multiple entries beging added with the
    // same key.  All nodes being added should be singletons, they should not
    // already be a part of a chain.
    void addToFreeList(FreeListEntry* entry)
    {
        ASSERT(!entry->nextEntry);

        if (entry->size == m_commonSize) {
            m_commonSizedAllocations.append(entry->pointer);
            delete entry;
        } else if (FreeListEntry* entryInFreeList = m_freeList.search(entry->size, m_freeList.EQUAL)) {
            // m_freeList already contain an entry for this size - insert this node into the chain.
            entry->nextEntry = entryInFreeList->nextEntry;
            entryInFreeList->nextEntry = entry;
        } else
            m_freeList.insert(entry);
    }

    // We do not attempt to coalesce addition, which may lead to fragmentation;
    // instead we periodically perform a sweep to try to coalesce neigboring
    // entries in m_freeList.  Presently this is triggered at the point 16MB
    // of memory has been released.
    void coalesceFreeSpace()
    {
        Vector<FreeListEntry*> freeListEntries;
        SizeSortedFreeTree::Iterator iter;
        iter.start_iter_least(m_freeList);

        // Empty m_freeList into a Vector.
        for (FreeListEntry* entry; (entry = *iter); ++iter) {
            // Each entry in m_freeList might correspond to multiple
            // free chunks of memory (of the same size).  Walk the chain
            // (this is likely of couse only be one entry long!) adding
            // each entry to the Vector (at reseting the next in chain
            // pointer to separate each node out).
            FreeListEntry* next;
            do {
                next = entry->nextEntry;
                entry->nextEntry = 0;
                freeListEntries.append(entry);
            } while ((entry = next));
        }
        // All entries are now in the Vector; purge the tree.
        m_freeList.purge();

        // Reverse-sort the freeListEntries and m_commonSizedAllocations Vectors.
        // We reverse-sort so that we can logically work forwards through memory,
        // whilst popping items off the end of the Vectors using last() and removeLast().
        qsort(freeListEntries.begin(), freeListEntries.size(), sizeof(FreeListEntry*), reverseSortFreeListEntriesByPointer);
        qsort(m_commonSizedAllocations.begin(), m_commonSizedAllocations.size(), sizeof(void*), reverseSortCommonSizedAllocations);

        // The entries from m_commonSizedAllocations that cannot be
        // coalesced into larger chunks will be temporarily stored here.
        Vector<void*> newCommonSizedAllocations;

        // Keep processing so long as entries remain in either of the vectors.
        while (freeListEntries.size() || m_commonSizedAllocations.size()) {
            // We're going to try to find a FreeListEntry node that we can coalesce onto.
            FreeListEntry* coalescionEntry = 0;

            // Is the lowest addressed chunk of free memory of common-size, or is it in the free list?
            if (m_commonSizedAllocations.size() && (!freeListEntries.size() || (m_commonSizedAllocations.last() < freeListEntries.last()->pointer))) {
                // Pop an item from the m_commonSizedAllocations vector - this is the lowest
                // addressed free chunk.  Find out the begin and end addresses of the memory chunk.
                void* begin = m_commonSizedAllocations.last();
                void* end = (void*)((intptr_t)begin + m_commonSize);
                m_commonSizedAllocations.removeLast();

                // Try to find another free chunk abutting onto the end of the one we have already found.
                if (freeListEntries.size() && (freeListEntries.last()->pointer == end)) {
                    // There is an existing FreeListEntry for the next chunk of memory!
                    // we can reuse this.  Pop it off the end of m_freeList.
                    coalescionEntry = freeListEntries.last();
                    freeListEntries.removeLast();
                    // Update the existing node to include the common-sized chunk that we also found. 
                    coalescionEntry->pointer = (void*)((intptr_t)coalescionEntry->pointer - m_commonSize);
                    coalescionEntry->size += m_commonSize;
                } else if (m_commonSizedAllocations.size() && (m_commonSizedAllocations.last() == end)) {
                    // There is a second common-sized chunk that can be coalesced.
                    // Allocate a new node.
                    m_commonSizedAllocations.removeLast();
                    coalescionEntry = new FreeListEntry(begin, 2 * m_commonSize);
                } else {
                    // Nope - this poor little guy is all on his own. :-(
                    // Add him into the newCommonSizedAllocations vector for now, we're
                    // going to end up adding him back into the m_commonSizedAllocations
                    // list when we're done.
                    newCommonSizedAllocations.append(begin);
                    continue;
                }
            } else {
                ASSERT(freeListEntries.size());
                ASSERT(!m_commonSizedAllocations.size() || (freeListEntries.last()->pointer < m_commonSizedAllocations.last()));
                // The lowest addressed item is from m_freeList; pop it from the Vector.
                coalescionEntry = freeListEntries.last();
                freeListEntries.removeLast();
            }
            
            // Right, we have a FreeListEntry, we just need check if there is anything else
            // to coalesce onto the end.
            ASSERT(coalescionEntry);
            while (true) {
                // Calculate the end address of the chunk we have found so far.
                void* end = (void*)((intptr_t)coalescionEntry->pointer - coalescionEntry->size);

                // Is there another chunk adjacent to the one we already have?
                if (freeListEntries.size() && (freeListEntries.last()->pointer == end)) {
                    // Yes - another FreeListEntry -pop it from the list.
                    FreeListEntry* coalescee = freeListEntries.last();
                    freeListEntries.removeLast();
                    // Add it's size onto our existing node.
                    coalescionEntry->size += coalescee->size;
                    delete coalescee;
                } else if (m_commonSizedAllocations.size() && (m_commonSizedAllocations.last() == end)) {
                    // We can coalesce the next common-sized chunk.
                    m_commonSizedAllocations.removeLast();
                    coalescionEntry->size += m_commonSize;
                } else
                    break; // Nope, nothing to be added - stop here.
            }

            // We've coalesced everything we can onto the current chunk.
            // Add it back into m_freeList.
            addToFreeList(coalescionEntry);
        }

        // All chunks of free memory larger than m_commonSize should be
        // back in m_freeList by now.  All that remains to be done is to
        // copy the contents on the newCommonSizedAllocations back into
        // the m_commonSizedAllocations Vector.
        ASSERT(m_commonSizedAllocations.size() == 0);
        m_commonSizedAllocations.append(newCommonSizedAllocations);
    }

public:

    FixedVMPoolAllocator(size_t commonSize, size_t totalHeapSize)
        : m_commonSize(commonSize)
        , m_countFreedSinceLastCoalesce(0)
        , m_totalHeapSize(totalHeapSize)
    {
        // Cook up an address to allocate at, using the following recipe:
        //   17 bits of zero, stay in userspace kids.
        //   26 bits of randomness for ASLR.
        //   21 bits of zero, at least stay aligned within one level of the pagetables.
        //
        // But! - as a temporary workaround for some plugin problems (rdar://problem/6812854),
        // for now instead of 2^26 bits of ASLR lets stick with 25 bits of randomization plus
        // 2^24, which should put up somewhere in the middle of usespace (in the address range
        // 0x200000000000 .. 0x5fffffffffff).
        intptr_t randomLocation = arc4random() & ((1 << 25) - 1);
        randomLocation += (1 << 24);
        randomLocation <<= 21;
        m_base = mmap(reinterpret_cast<void*>(randomLocation), m_totalHeapSize, INITIAL_PROTECTION_FLAGS, MAP_PRIVATE | MAP_ANON, VM_TAG_FOR_EXECUTABLEALLOCATOR_MEMORY, 0);
        if (!m_base)
            CRASH();

        // For simplicity, we keep all memory in m_freeList in a 'released' state.
        // This means that we can simply reuse all memory when allocating, without
        // worrying about it's previous state, and also makes coalescing m_freeList
        // simpler since we need not worry about the possibility of coalescing released
        // chunks with non-released ones.
        release(m_base, m_totalHeapSize);
        m_freeList.insert(new FreeListEntry(m_base, m_totalHeapSize));
    }

    void* alloc(size_t size)
    {
        void* result;

        // Freed allocations of the common size are not stored back into the main
        // m_freeList, but are instead stored in a separate vector.  If the request
        // is for a common sized allocation, check this list.
        if ((size == m_commonSize) && m_commonSizedAllocations.size()) {
            result = m_commonSizedAllocations.last();
            m_commonSizedAllocations.removeLast();
        } else {
            // Serach m_freeList for a suitable sized chunk to allocate memory from.
            FreeListEntry* entry = m_freeList.search(size, m_freeList.GREATER_EQUAL);

            // This would be bad news.
            if (!entry) {
                // Errk!  Lets take a last-ditch desparation attempt at defragmentation...
                coalesceFreeSpace();
                // Did that free up a large enough chunk?
                entry = m_freeList.search(size, m_freeList.GREATER_EQUAL);
                // No?...  *BOOM!*
                if (!entry)
                    CRASH();
            }
            ASSERT(entry->size != m_commonSize);

            // Remove the entry from m_freeList.  But! -
            // Each entry in the tree may represent a chain of multiple chunks of the
            // same size, and we only want to remove one on them.  So, if this entry
            // does have a chain, just remove the first-but-one item from the chain.
            if (FreeListEntry* next = entry->nextEntry) {
                // We're going to leave 'entry' in the tree; remove 'next' from its chain.
                entry->nextEntry = next->nextEntry;
                next->nextEntry = 0;
                entry = next;
            } else
                m_freeList.remove(entry->size);

            // Whoo!, we have a result!
            ASSERT(entry->size >= size);
            result = entry->pointer;

            // If the allocation exactly fits the chunk we found in the,
            // m_freeList then the FreeListEntry node is no longer needed.
            if (entry->size == size)
                delete entry;
            else {
                // There is memory left over, and it is not of the common size.
                // We can reuse the existing FreeListEntry node to add this back
                // into m_freeList.
                entry->pointer = (void*)((intptr_t)entry->pointer + size);
                entry->size -= size;
                addToFreeList(entry);
            }
        }

        // Call reuse to report to the operating system that this memory is in use.
        ASSERT(isWithinVMPool(result, size));
        reuse(result, size);
        return result;
    }

    void free(void* pointer, size_t size)
    {
        // Call release to report to the operating system that this
        // memory is no longer in use, and need not be paged out.
        ASSERT(isWithinVMPool(pointer, size));
        release(pointer, size);

        // Common-sized allocations are stored in the m_commonSizedAllocations
        // vector; all other freed chunks are added to m_freeList.
        if (size == m_commonSize)
            m_commonSizedAllocations.append(pointer);
        else
            addToFreeList(new FreeListEntry(pointer, size));

        // Do some housekeeping.  Every time we reach a point that
        // 16MB of allocations have been freed, sweep m_freeList
        // coalescing any neighboring fragments.
        m_countFreedSinceLastCoalesce += size;
        if (m_countFreedSinceLastCoalesce >= SIXTEEN_MB) {
            m_countFreedSinceLastCoalesce = 0;
            coalesceFreeSpace();
        }
    }

private:

#ifndef NDEBUG
    bool isWithinVMPool(void* pointer, size_t size)
    {
        return pointer >= m_base && (reinterpret_cast<char*>(pointer) + size <= reinterpret_cast<char*>(m_base) + m_totalHeapSize);
    }
#endif

    // Freed space from the most common sized allocations will be held in this list, ...
    const size_t m_commonSize;
    Vector<void*> m_commonSizedAllocations;

    // ... and all other freed allocations are held in m_freeList.
    SizeSortedFreeTree m_freeList;

    // This is used for housekeeping, to trigger defragmentation of the freed lists.
    size_t m_countFreedSinceLastCoalesce;

    void* m_base;
    size_t m_totalHeapSize;
};

void ExecutableAllocator::intializePageSize()
{
    ExecutableAllocator::pageSize = getpagesize();
}

static FixedVMPoolAllocator* allocator = 0;
static SpinLock spinlock = SPINLOCK_INITIALIZER;

ExecutablePool::Allocation ExecutablePool::systemAlloc(size_t size)
{
  SpinLockHolder lock_holder(&spinlock);

    if (!allocator)
        allocator = new FixedVMPoolAllocator(JIT_ALLOCATOR_LARGE_ALLOC_SIZE, TWO_GB);
    ExecutablePool::Allocation alloc = {reinterpret_cast<char*>(allocator->alloc(size)), size};
    return alloc;
}

void ExecutablePool::systemRelease(const ExecutablePool::Allocation& allocation) 
{
  SpinLockHolder lock_holder(&spinlock);

    ASSERT(allocator);
    allocator->free(allocation.pages, allocation.size);
}

}

#endif // HAVE(ASSEMBLER)
