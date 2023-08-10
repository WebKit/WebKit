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

#pragma once

#include <wtf/AutomaticThread.h>

namespace JSC {

class Heap;
class BlockDirectory;

class ConcurrentSweeper final : public AutomaticThread {
    WTF_MAKE_NONCOPYABLE(ConcurrentSweeper);
    ConcurrentSweeper(const AbstractLocker&, VM&, Box<Lock>, Ref<AutomaticThreadCondition>&&);
public:
    ~ConcurrentSweeper();

    static Ref<ConcurrentSweeper> create(VM&);

    void suspendSweeping() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        m_rightToSweep.lock();
        m_isSuspended = true;
    }

    void resumeSweeping() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        m_isSuspended = false;
        m_rightToSweep.unlock();
    }

    bool isSuspended() const { return m_isSuspended; }

    void shouldStop() { m_shouldStop = true; }
    bool isStopped() const { return m_shouldStop; }

    Lock& lock() { return AutomaticThread::lock(); }
    // Requires lock();
    void pushDirectoryToSweep(AbstractLocker&, BlockDirectory* directory)
    {
        m_directoriesToSweep.append(directory);
    }
    void notifyPushedDirectories(AbstractLocker&);

    const char* name() const override { return "Concurrent Sweeper"; }
private:
    PollResult poll(const AbstractLocker&) override;
    WorkResult work() override;

    // This should probably be a Darwin's OSUnfairLock because that supports priority boosting which we don't support here.
    Lock m_rightToSweep;
    // FIXME: Handle the worklist growing faster than we empty it and blowing up.
    Deque<BlockDirectory*> m_directoriesToSweep;
    BlockDirectory* m_currentDirectory { nullptr };
    unsigned m_currentMarkedBlockIndex { 0 };

    bool m_isSuspended { false };
    bool m_shouldStop { false };
};

} // namespace JSC
