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

#if ENABLE(EXECUTABLE_ALLOCATOR_FIXED)

#include <errno.h>

#include <sys/mman.h>
#include <unistd.h>
#include <wtf/MetaAllocator.h>
#include <wtf/PageReservation.h>
#include <wtf/VMTags.h>

#if OS(LINUX)
#include <stdio.h>
#endif

using namespace WTF;

namespace JSC {
    
#if CPU(ARM)
static const size_t fixedPoolSize = 16 * 1024 * 1024;
#elif CPU(X86_64)
static const size_t fixedPoolSize = 1024 * 1024 * 1024;
#else
static const size_t fixedPoolSize = 32 * 1024 * 1024;
#endif

class FixedVMPoolExecutableAllocator : public MetaAllocator {
public:
    FixedVMPoolExecutableAllocator()
        : MetaAllocator(32) // round up all allocations to 32 bytes
    {
        m_reservation = PageReservation::reserveWithGuardPages(fixedPoolSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true);
#if !ENABLE(INTERPRETER)
        if (!m_reservation)
            CRASH();
#endif
        if (m_reservation) {
            ASSERT(m_reservation.size() == fixedPoolSize);
            addFreshFreeSpace(m_reservation.base(), m_reservation.size());
        }
    }
    
protected:
    virtual void* allocateNewSpace(size_t&)
    {
        // We're operating in a fixed pool, so new allocation is always prohibited.
        return 0;
    }
    
    virtual void notifyNeedPage(void* page)
    {
        m_reservation.commit(page, pageSize());
    }
    
    virtual void notifyPageIsFree(void* page)
    {
        m_reservation.decommit(page, pageSize());
    }

private:
    PageReservation m_reservation;
};

static FixedVMPoolExecutableAllocator* allocator;

void ExecutableAllocator::initializeAllocator()
{
    ASSERT(!allocator);
    allocator = new FixedVMPoolExecutableAllocator();
}

ExecutableAllocator::ExecutableAllocator(JSGlobalData&)
{
    ASSERT(allocator);
}

bool ExecutableAllocator::isValid() const
{
    return !!allocator->bytesReserved();
}

bool ExecutableAllocator::underMemoryPressure()
{
    MetaAllocator::Statistics statistics = allocator->currentStatistics();
    return statistics.bytesAllocated > statistics.bytesReserved / 2;
}

PassRefPtr<ExecutableMemoryHandle> ExecutableAllocator::allocate(JSGlobalData& globalData, size_t sizeInBytes, void* ownerUID)
{
    UNUSED_PARAM(ownerUID);

    RefPtr<ExecutableMemoryHandle> result = allocator->allocate(sizeInBytes);
    if (!result) {
        releaseExecutableMemory(globalData);
        result = allocator->allocate(sizeInBytes);
        if (!result)
            CRASH();
    }
    return result.release();
}

size_t ExecutableAllocator::committedByteCount()
{
    return allocator->bytesCommitted();
}

#if ENABLE(META_ALLOCATOR_PROFILE)
void ExecutableAllocator::dumpProfile()
{
    allocator->dumpProfile();
}
#endif

}


#endif // ENABLE(EXECUTABLE_ALLOCATOR_FIXED)
