/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include "CLoopStack.h"

#if ENABLE(C_LOOP)

#include "CLoopStackInlines.h"
#include "ConservativeRoots.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "Options.h"
#include <wtf/Lock.h>

namespace JSC {

static size_t committedBytesCount = 0;

static size_t commitSize()
{
    static size_t size = std::max<size_t>(16 * 1024, pageSize());
    return size;
}

static Lock stackStatisticsMutex;

CLoopStack::CLoopStack(VM& vm)
    : m_vm(vm)
    , m_topCallFrame(vm.topCallFrame)
    , m_softReservedZoneSizeInRegisters(0)
{
    size_t capacity = Options::maxPerThreadStackUsage();
    capacity = WTF::roundUpToMultipleOf(pageSize(), capacity);
    ASSERT(capacity && isPageAligned(capacity));

    m_reservation = PageReservation::reserve(WTF::roundUpToMultipleOf(commitSize(), capacity), OSAllocator::UnknownUsage);

    auto* bottomOfStack = highAddress();
    setCLoopStackLimit(bottomOfStack);
    ASSERT(m_end == bottomOfStack);
    m_commitTop = bottomOfStack;
    m_lastStackPointer = bottomOfStack;
    m_currentStackPointer = bottomOfStack;

    m_topCallFrame = 0;
}

CLoopStack::~CLoopStack()
{
    ptrdiff_t sizeToDecommit = reinterpret_cast<char*>(highAddress()) - reinterpret_cast<char*>(m_commitTop);
    m_reservation.decommit(reinterpret_cast<void*>(m_commitTop), sizeToDecommit);
    addToCommittedByteCount(-sizeToDecommit);
    m_reservation.deallocate();
}

bool CLoopStack::grow(Register* newTopOfStack)
{
    Register* newTopOfStackWithReservedZone = newTopOfStack - m_softReservedZoneSizeInRegisters;

    // If we have already committed enough memory to satisfy this request,
    // just update the end pointer and return.
    if (newTopOfStackWithReservedZone >= m_commitTop) {
        setCLoopStackLimit(newTopOfStack);
        return true;
    }

    // Compute the chunk size of additional memory to commit, and see if we
    // have it still within our budget. If not, we'll fail to grow and
    // return false.
    ptrdiff_t delta = reinterpret_cast<char*>(m_commitTop) - reinterpret_cast<char*>(newTopOfStackWithReservedZone);
    delta = WTF::roundUpToMultipleOf(commitSize(), delta);
    Register* newCommitTop = m_commitTop - (delta / sizeof(Register));
    if (newCommitTop < reservationTop())
        return false;

    // Otherwise, the growth is still within our budget. Commit it and return true.
    m_reservation.commit(newCommitTop, delta);
    addToCommittedByteCount(delta);
    m_commitTop = newCommitTop;
    newTopOfStack = m_commitTop + m_softReservedZoneSizeInRegisters;
    setCLoopStackLimit(newTopOfStack);
    return true;
}

void CLoopStack::gatherConservativeRoots(ConservativeRoots& conservativeRoots, JITStubRoutineSet& jitStubRoutines, CodeBlockSet& codeBlocks)
{
    conservativeRoots.add(currentStackPointer(), highAddress(), jitStubRoutines, codeBlocks);
}

void CLoopStack::sanitizeStack()
{
#if !ASAN_ENABLED
    void* stackTop = currentStackPointer();
    ASSERT(stackTop <= highAddress());
    if (m_lastStackPointer < stackTop) {
        char* begin = reinterpret_cast<char*>(m_lastStackPointer);
        char* end = reinterpret_cast<char*>(stackTop);
        memset(begin, 0, end - begin);
    }
    
    m_lastStackPointer = stackTop;
#endif
}

void CLoopStack::releaseExcessCapacity()
{
    Register* highAddressWithReservedZone = highAddress() - m_softReservedZoneSizeInRegisters;
    ptrdiff_t delta = reinterpret_cast<char*>(highAddressWithReservedZone) - reinterpret_cast<char*>(m_commitTop);
    m_reservation.decommit(m_commitTop, delta);
    addToCommittedByteCount(-delta);
    m_commitTop = highAddressWithReservedZone;
}

void CLoopStack::addToCommittedByteCount(long byteCount)
{
    Locker locker { stackStatisticsMutex };
    ASSERT(static_cast<long>(committedBytesCount) + byteCount > -1);
    committedBytesCount += byteCount;
}

void CLoopStack::setSoftReservedZoneSize(size_t reservedZoneSize)
{
    m_softReservedZoneSizeInRegisters = reservedZoneSize / sizeof(Register);
    if (m_commitTop > m_end - m_softReservedZoneSizeInRegisters)
        grow(m_end);
}

bool CLoopStack::isSafeToRecurse() const
{
    void* reservationLimit = reinterpret_cast<int8_t*>(reservationTop() + m_softReservedZoneSizeInRegisters);
    return !m_topCallFrame || (m_topCallFrame->topOfFrame() > reservationLimit);
}

size_t CLoopStack::committedByteCount()
{
    Locker locker { stackStatisticsMutex };
    return committedBytesCount;
}

} // namespace JSC

#endif // ENABLE(C_LOOP)
