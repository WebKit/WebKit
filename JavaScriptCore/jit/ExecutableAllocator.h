/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ExecutableAllocator_h
#define ExecutableAllocator_h

#if ENABLE(ASSEMBLER)

#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#include <limits>

#define JIT_ALLOCATOR_PAGE_MASK (ExecutableAllocator::pageSize - 1)
#define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (ExecutableAllocator::pageSize * 4)

namespace JSC {

class ExecutablePool : public RefCounted<ExecutablePool> {
private:
    struct Allocation {
        char* pages;
        size_t size;
    };
    typedef Vector<Allocation, 2> AllocationList;

public:
    static PassRefPtr<ExecutablePool> create(size_t n)
    {
        return adoptRef(new ExecutablePool(n));
    }

    void* alloc(size_t n)
    {
        if (n < static_cast<size_t>(m_end - m_freePtr)) {
            char* result = m_freePtr;
            // ensure m_freePtr is word aligned.
            m_freePtr += n + (sizeof(void*) - n & (sizeof(void*) - 1));
            return result;
        }

        // Insufficient space to allocate in the existing pool
        // so we need allocate into a new pool
        return poolAllocate(n);
    }
    
    ~ExecutablePool()
    {
        AllocationList::const_iterator end = m_pools.end();
        for (AllocationList::const_iterator ptr = m_pools.begin(); ptr != end; ++ptr)
            ExecutablePool::systemRelease(*ptr);
    }

    size_t available() const { return (m_pools.size() > 1) ? 0 : m_end - m_freePtr; }

private:
    static Allocation systemAlloc(size_t n);
    static void systemRelease(const Allocation& alloc);

    ExecutablePool(size_t n)
    {
        size_t allocSize = sizeForAllocation(n);
        Allocation mem = systemAlloc(allocSize);
        m_pools.append(mem);
        m_freePtr = mem.pages;
        if (!m_freePtr)
            CRASH(); // Failed to allocate
        m_end = m_freePtr + allocSize;
    }

    static inline size_t sizeForAllocation(size_t request);

    void* poolAllocate(size_t n)
    {
        size_t allocSize = sizeForAllocation(n);
        
        Allocation result = systemAlloc(allocSize);
        if (!result.pages)
            CRASH(); // Failed to allocate
        
        ASSERT(m_end >= m_freePtr);
        if ((allocSize - n) > static_cast<size_t>(m_end - m_freePtr)) {
            // Replace allocation pool
            m_freePtr = result.pages + n;
            m_end = result.pages + allocSize;
        }

        m_pools.append(result);
        return result.pages;
    }

    char* m_freePtr;
    char* m_end;
    AllocationList m_pools;
};

class ExecutableAllocator {
public:
    static size_t pageSize;
    ExecutableAllocator()
    {
        if (!pageSize)
            intializePageSize();
        m_smallAllocationPool = ExecutablePool::create(JIT_ALLOCATOR_LARGE_ALLOC_SIZE);
    }

    PassRefPtr<ExecutablePool> poolForSize(size_t n)
    {
        // Try to fit in the existing small allocator
        if (n < m_smallAllocationPool->available())
            return m_smallAllocationPool;

        // If the request is large, we just provide a unshared allocator
        if (n > JIT_ALLOCATOR_LARGE_ALLOC_SIZE)
            return ExecutablePool::create(n);

        // Create a new allocator
        RefPtr<ExecutablePool> pool = ExecutablePool::create(JIT_ALLOCATOR_LARGE_ALLOC_SIZE);

        // If the new allocator will result in more free space than in
        // the current small allocator, then we will use it instead
        if ((pool->available() - n) > m_smallAllocationPool->available())
            m_smallAllocationPool = pool;
        return pool.release();
    }

private:
    RefPtr<ExecutablePool> m_smallAllocationPool;
    static void intializePageSize();
};

inline size_t ExecutablePool::sizeForAllocation(size_t request)
{
    if ((std::numeric_limits<size_t>::max() - ExecutableAllocator::pageSize) <= request)
        CRASH(); // Allocation is too large
    
    // Round up to next page boundary
    size_t size = request + JIT_ALLOCATOR_PAGE_MASK;
    size = size & ~JIT_ALLOCATOR_PAGE_MASK;
    ASSERT(size >= request);
    return size;
}

}

#endif // ENABLE(ASSEMBLER)

#endif // !defined(ExecutableAllocator)
