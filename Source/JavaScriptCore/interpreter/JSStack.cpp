/*
 * Copyright (C) 2008, 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSStackInlines.h"

#include "ConservativeRoots.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "Options.h"

namespace JSC {

#if !ENABLE(JIT)
static size_t committedBytesCount = 0;

static Mutex& stackStatisticsMutex()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Mutex, staticMutex, ());
    return staticMutex;
}    
#endif // !ENABLE(JIT)

JSStack::JSStack(VM& vm)
    : m_vm(vm)
    , m_topCallFrame(vm.topCallFrame)
#if !ENABLE(JIT)
    , m_end(0)
    , m_reservedZoneSizeInRegisters(0)
#endif
{
#if !ENABLE(JIT)
    size_t capacity = Options::maxPerThreadStackUsage();
    ASSERT(capacity && isPageAligned(capacity));

    m_reservation = PageReservation::reserve(WTF::roundUpToMultipleOf(commitSize, capacity), OSAllocator::JSVMStackPages);
    setStackLimit(highAddress());
    m_commitTop = highAddress();
    
    m_lastStackTop = baseOfStack();
#endif // !ENABLE(JIT)

    m_topCallFrame = 0;
}

#if !ENABLE(JIT)
JSStack::~JSStack()
{
    ptrdiff_t sizeToDecommit = reinterpret_cast<char*>(highAddress()) - reinterpret_cast<char*>(m_commitTop);
    m_reservation.decommit(reinterpret_cast<void*>(m_commitTop), sizeToDecommit);
    addToCommittedByteCount(-sizeToDecommit);
    m_reservation.deallocate();
}

bool JSStack::growSlowCase(Register* newTopOfStack)
{
    Register* newTopOfStackWithReservedZone = newTopOfStack - m_reservedZoneSizeInRegisters;

    // If we have already committed enough memory to satisfy this request,
    // just update the end pointer and return.
    if (newTopOfStackWithReservedZone >= m_commitTop) {
        setStackLimit(newTopOfStack);
        return true;
    }

    // Compute the chunk size of additional memory to commit, and see if we
    // have it is still within our budget. If not, we'll fail to grow and
    // return false.
    ptrdiff_t delta = reinterpret_cast<char*>(m_commitTop) - reinterpret_cast<char*>(newTopOfStackWithReservedZone);
    delta = WTF::roundUpToMultipleOf(commitSize, delta);
    Register* newCommitTop = m_commitTop - (delta / sizeof(Register));
    if (newCommitTop < reservationTop())
        return false;

    // Otherwise, the growth is still within our budget. Go ahead and commit
    // it and return true.
    m_reservation.commit(newCommitTop, delta);
    addToCommittedByteCount(delta);
    m_commitTop = newCommitTop;
    setStackLimit(newTopOfStack);
    return true;
}

void JSStack::gatherConservativeRoots(ConservativeRoots& conservativeRoots)
{
    conservativeRoots.add(topOfStack() + 1, highAddress());
}

void JSStack::gatherConservativeRoots(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks)
{
    conservativeRoots.add(topOfStack() + 1, highAddress(), jitStubRoutines, codeBlocks);
}

void JSStack::sanitizeStack()
{
#if !defined(ADDRESS_SANITIZER)
    ASSERT(topOfStack() <= baseOfStack());
    
    if (m_lastStackTop < topOfStack()) {
        char* begin = reinterpret_cast<char*>(m_lastStackTop + 1);
        char* end = reinterpret_cast<char*>(topOfStack() + 1);
        memset(begin, 0, end - begin);
    }
    
    m_lastStackTop = topOfStack();
#endif
}

void JSStack::releaseExcessCapacity()
{
    Register* highAddressWithReservedZone = highAddress() - m_reservedZoneSizeInRegisters;
    ptrdiff_t delta = reinterpret_cast<char*>(highAddressWithReservedZone) - reinterpret_cast<char*>(m_commitTop);
    m_reservation.decommit(m_commitTop, delta);
    addToCommittedByteCount(-delta);
    m_commitTop = highAddressWithReservedZone;
}

void JSStack::initializeThreading()
{
    stackStatisticsMutex();
}

void JSStack::addToCommittedByteCount(long byteCount)
{
    MutexLocker locker(stackStatisticsMutex());
    ASSERT(static_cast<long>(committedBytesCount) + byteCount > -1);
    committedBytesCount += byteCount;
}

void JSStack::setReservedZoneSize(size_t reservedZoneSize)
{
    m_reservedZoneSizeInRegisters = reservedZoneSize / sizeof(Register);
    if (m_commitTop >= (m_end + 1) - m_reservedZoneSizeInRegisters)
        growSlowCase(m_end + 1);
}
#endif // !ENABLE(JIT)

#if ENABLE(JIT)
Register* JSStack::lowAddress() const
{
    ASSERT(wtfThreadData().stack().isGrowingDownward());
    return reinterpret_cast<Register*>(m_vm.stackLimit());
}

Register* JSStack::highAddress() const
{
    ASSERT(wtfThreadData().stack().isGrowingDownward());
    return reinterpret_cast<Register*>(wtfThreadData().stack().origin());
}
#endif // ENABLE(JIT)

size_t JSStack::committedByteCount()
{
#if !ENABLE(JIT)
    MutexLocker locker(stackStatisticsMutex());
    return committedBytesCount;
#else
    // When using the C stack, we don't know how many stack pages are actually
    // committed. So, we use the current stack usage as an estimate.
    ASSERT(wtfThreadData().stack().isGrowingDownward());
    int8_t* current = reinterpret_cast<int8_t*>(&current);
    int8_t* high = reinterpret_cast<int8_t*>(wtfThreadData().stack().origin());
    return high - current;
#endif
}

} // namespace JSC
