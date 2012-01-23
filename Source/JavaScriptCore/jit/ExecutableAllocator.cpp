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

#include "config.h"

#include "ExecutableAllocator.h"

#if ENABLE(EXECUTABLE_ALLOCATOR_DEMAND)
#include <wtf/MetaAllocator.h>
#include <wtf/PageReservation.h>
#include <wtf/VMTags.h>
#endif

#if ENABLE(ASSEMBLER)

using namespace WTF;

namespace JSC {

#if ENABLE(EXECUTABLE_ALLOCATOR_DEMAND)

class DemandExecutableAllocator : public MetaAllocator {
public:
    DemandExecutableAllocator()
        : MetaAllocator(32) // round up all allocations to 32 bytes
    {
        // Don't preallocate any memory here.
    }
    
    virtual ~DemandExecutableAllocator()
    {
        for (unsigned i = 0; i < reservations.size(); ++i)
            reservations.at(i).deallocate();
    }

protected:
    virtual void* allocateNewSpace(size_t& numPages)
    {
        size_t newNumPages = (((numPages * pageSize() + JIT_ALLOCATOR_LARGE_ALLOC_SIZE - 1) / JIT_ALLOCATOR_LARGE_ALLOC_SIZE * JIT_ALLOCATOR_LARGE_ALLOC_SIZE) + pageSize() - 1) / pageSize();
        
        ASSERT(newNumPages >= numPages);
        
        numPages = newNumPages;
        
        PageReservation reservation = PageReservation::reserve(numPages * pageSize(), OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true);
        if (!reservation)
            CRASH();
        
        reservations.append(reservation);
        
        return reservation.base();
    }
    
    virtual void notifyNeedPage(void* page)
    {
        OSAllocator::commit(page, pageSize(), EXECUTABLE_POOL_WRITABLE, true);
    }
    
    virtual void notifyPageIsFree(void* page)
    {
        OSAllocator::decommit(page, pageSize());
    }

private:
    Vector<PageReservation, 16> reservations;
};

static DemandExecutableAllocator* allocator;

void ExecutableAllocator::initializeAllocator()
{
    ASSERT(!allocator);
    allocator = new DemandExecutableAllocator();
}

ExecutableAllocator::ExecutableAllocator(JSGlobalData&)
{
    ASSERT(allocator);
}

bool ExecutableAllocator::isValid() const
{
    return true;
}

bool ExecutableAllocator::underMemoryPressure()
{
    return false;
}

PassRefPtr<ExecutableMemoryHandle> ExecutableAllocator::allocate(JSGlobalData&, size_t sizeInBytes, void* ownerUID)
{
    UNUSED_PARAM(ownerUID);

    RefPtr<ExecutableMemoryHandle> result = allocator->allocate(sizeInBytes);
    if (!result)
        CRASH();
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

#endif // ENABLE(EXECUTABLE_ALLOCATOR_DEMAND)

#if ENABLE(ASSEMBLER_WX_EXCLUSIVE)

#if OS(WINDOWS)
#error "ASSEMBLER_WX_EXCLUSIVE not yet suported on this platform."
#endif

void ExecutableAllocator::reprotectRegion(void* start, size_t size, ProtectionSetting setting)
{
    size_t pageSize = WTF::pageSize();

    // Calculate the start of the page containing this region,
    // and account for this extra memory within size.
    intptr_t startPtr = reinterpret_cast<intptr_t>(start);
    intptr_t pageStartPtr = startPtr & ~(pageSize - 1);
    void* pageStart = reinterpret_cast<void*>(pageStartPtr);
    size += (startPtr - pageStartPtr);

    // Round size up
    size += (pageSize - 1);
    size &= ~(pageSize - 1);

    mprotect(pageStart, size, (setting == Writable) ? PROTECTION_FLAGS_RW : PROTECTION_FLAGS_RX);
}

#endif

#if CPU(ARM_TRADITIONAL) && OS(LINUX) && COMPILER(RVCT)

__asm void ExecutableAllocator::cacheFlush(void* code, size_t size)
{
    ARM
    push {r7}
    add r1, r1, r0
    mov r7, #0xf0000
    add r7, r7, #0x2
    mov r2, #0x0
    svc #0x0
    pop {r7}
    bx lr
}

#endif

}

#endif // HAVE(ASSEMBLER)
