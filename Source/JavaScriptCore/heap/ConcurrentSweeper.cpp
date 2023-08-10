/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "ConcurrentSweeper.h"

#include "DeferGCInlines.h"
#include "HeapInlines.h"
#include "MarkedBlock.h"
#include "VM.h"


namespace JSC {

ConcurrentSweeper::ConcurrentSweeper(const AbstractLocker& locker, VM&, Box<Lock> lock, Ref<AutomaticThreadCondition>&& condition)
    // FIXME: This should probably have its own thread type even if it's just nominally different from Compiler.
    : AutomaticThread(locker, WTFMove(lock), WTFMove(condition), ThreadType::Compiler)
{
}

ConcurrentSweeper::~ConcurrentSweeper()
{
}

Ref<ConcurrentSweeper> ConcurrentSweeper::create(VM& vm)
{
    Box<Lock> lock = Box<Lock>::create();
    Locker locker(*lock);
    return adoptRef(*new ConcurrentSweeper(locker, vm, lock, AutomaticThreadCondition::create()));
}

auto ConcurrentSweeper::poll(const AbstractLocker&) -> PollResult
{
    if (m_shouldStop)
        return PollResult::Stop;

    if (m_currentDirectory)
        return PollResult::Work;

    if (m_directoriesToSweep.isEmpty())
        return PollResult::Wait;

    m_currentDirectory = m_directoriesToSweep.takeFirst();
    return PollResult::Work;
}

auto ConcurrentSweeper::work() -> WorkResult
{
    Locker locker(m_rightToSweep);

    MarkedBlock::Handle* handle;
    {
        Locker locker(m_currentDirectory->bitvectorLock());
        handle = m_currentDirectory->findBlockToSweep(locker, m_currentMarkedBlockIndex);
    }

    if (!handle) {
        m_currentDirectory = nullptr;
        return WorkResult::Continue;
    }

    handle->sweepConcurrently();
    return WorkResult::Continue;
}

void ConcurrentSweeper::notifyPushedDirectories(AbstractLocker& locker)
{
    condition().notifyAll(locker);
}

} // namespace JSC
