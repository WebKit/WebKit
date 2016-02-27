/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER)

#include "JSCInlines.h"

#include "CodeProfiling.h"
#include "ExecutableAllocationFuzz.h"
#include <errno.h>
#if !PLATFORM(WIN)
#include <unistd.h>
#endif
#include <wtf/MetaAllocator.h>
#include <wtf/PageReservation.h>
#include <wtf/VMTags.h>

#if OS(DARWIN)
#include <sys/mman.h>
#endif

#if OS(LINUX)
#include <stdio.h>
#endif

using namespace WTF;

namespace JSC {
    
uintptr_t startOfFixedExecutableMemoryPool;

class FixedVMPoolExecutableAllocator : public MetaAllocator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FixedVMPoolExecutableAllocator()
        : MetaAllocator(jitAllocationGranule) // round up all allocations to 32 bytes
    {
        size_t reservationSize;
        if (Options::jitMemoryReservationSize())
            reservationSize = Options::jitMemoryReservationSize();
        else
            reservationSize = fixedExecutableMemoryPoolSize;
        reservationSize = roundUpToMultipleOf(pageSize(), reservationSize);
        m_reservation = PageReservation::reserveWithGuardPages(reservationSize, OSAllocator::JSJITCodePages, EXECUTABLE_POOL_WRITABLE, true);
        if (m_reservation) {
            ASSERT(m_reservation.size() == reservationSize);
            addFreshFreeSpace(m_reservation.base(), m_reservation.size());
            
            startOfFixedExecutableMemoryPool = reinterpret_cast<uintptr_t>(m_reservation.base());
        }
    }

    virtual ~FixedVMPoolExecutableAllocator();
    
protected:
    virtual void* allocateNewSpace(size_t&) override
    {
        // We're operating in a fixed pool, so new allocation is always prohibited.
        return 0;
    }
    
    virtual void notifyNeedPage(void* page) override
    {
#if USE(MADV_FREE_FOR_JIT_MEMORY)
        UNUSED_PARAM(page);
#else
        m_reservation.commit(page, pageSize());
#endif
    }
    
    virtual void notifyPageIsFree(void* page) override
    {
#if USE(MADV_FREE_FOR_JIT_MEMORY)
        for (;;) {
            int result = madvise(page, pageSize(), MADV_FREE);
            if (!result)
                return;
            ASSERT(result == -1);
            if (errno != EAGAIN) {
                RELEASE_ASSERT_NOT_REACHED(); // In debug mode, this should be a hard failure.
                break; // In release mode, we should just ignore the error - not returning memory to the OS is better than crashing, especially since we _will_ be able to reuse the memory internally anyway.
            }
        }
#else
        m_reservation.decommit(page, pageSize());
#endif
    }

private:
    PageReservation m_reservation;
};

static FixedVMPoolExecutableAllocator* allocator;

void ExecutableAllocator::initializeAllocator()
{
    ASSERT(!allocator);
    allocator = new FixedVMPoolExecutableAllocator();
    CodeProfiling::notifyAllocator(allocator);
}

ExecutableAllocator::ExecutableAllocator(VM&)
{
    ASSERT(allocator);
}

ExecutableAllocator::~ExecutableAllocator()
{
}

FixedVMPoolExecutableAllocator::~FixedVMPoolExecutableAllocator()
{
    m_reservation.deallocate();
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

double ExecutableAllocator::memoryPressureMultiplier(size_t addedMemoryUsage)
{
    MetaAllocator::Statistics statistics = allocator->currentStatistics();
    ASSERT(statistics.bytesAllocated <= statistics.bytesReserved);
    size_t bytesAllocated = statistics.bytesAllocated + addedMemoryUsage;
    size_t bytesAvailable = static_cast<size_t>(
        statistics.bytesReserved * (1 - executablePoolReservationFraction));
    if (bytesAllocated >= bytesAvailable)
        bytesAllocated = bytesAvailable;
    double result = 1.0;
    size_t divisor = bytesAvailable - bytesAllocated;
    if (divisor)
        result = static_cast<double>(bytesAvailable) / divisor;
    if (result < 1.0)
        result = 1.0;
    return result;
}

RefPtr<ExecutableMemoryHandle> ExecutableAllocator::allocate(VM&, size_t sizeInBytes, void* ownerUID, JITCompilationEffort effort)
{
    if (effort != JITCompilationCanFail && Options::reportMustSucceedExecutableAllocations()) {
        dataLog("Allocating ", sizeInBytes, " bytes of executable memory with JITCompilationMustSucceed.\n");
        WTFReportBacktrace();
    }
    
    if (effort == JITCompilationCanFail
        && doExecutableAllocationFuzzingIfEnabled() == PretendToFailExecutableAllocation)
        return nullptr;
    
    if (effort == JITCompilationCanFail) {
        // Don't allow allocations if we are down to reserve.
        MetaAllocator::Statistics statistics = allocator->currentStatistics();
        size_t bytesAllocated = statistics.bytesAllocated + sizeInBytes;
        size_t bytesAvailable = static_cast<size_t>(
            statistics.bytesReserved * (1 - executablePoolReservationFraction));
        if (bytesAllocated > bytesAvailable)
            return nullptr;
    }
    
    RefPtr<ExecutableMemoryHandle> result = allocator->allocate(sizeInBytes, ownerUID);
    if (!result) {
        if (effort != JITCompilationCanFail) {
            dataLog("Ran out of executable memory while allocating ", sizeInBytes, " bytes.\n");
            CRASH();
        }
        return nullptr;
    }
    return result;
}

bool ExecutableAllocator::isValidExecutableMemory(const LockHolder& locker, void* address)
{
    return allocator->isInAllocatedMemory(locker, address);
}

Lock& ExecutableAllocator::getLock() const
{
    return allocator->getLock();
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

#endif
