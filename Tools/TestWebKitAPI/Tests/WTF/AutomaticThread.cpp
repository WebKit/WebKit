/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/AutomaticThread.h>

#include <wtf/Deque.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

struct SharedState {
    Box<Lock> lock { Box<Lock>::create() };
    Ref<AutomaticThreadCondition> workAvailable { AutomaticThreadCondition::create() };
    Condition changed;
    Deque<unsigned> queue;
    unsigned waits { 0 };
    bool firstItemStarted { false };
    bool releaseFirstItem { false };
    bool secondItemDone { false };
    bool shouldStop { false };
};

class TestThread final : public AutomaticThread {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestThread);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TestThread);
public:
    static Ref<TestThread> create(const AbstractLocker& locker, SharedState& state)
    {
        return adoptRef(*new TestThread(locker, state));
    }

private:
    TestThread(const AbstractLocker& locker, SharedState& state)
        : AutomaticThread(locker, state.lock, state.workAvailable.copyRef(), 10_ms)
        , m_state(state)
    {
    }

    PollResult poll(const AbstractLocker&) final
    {
        if (m_state.shouldStop)
            return PollResult::Stop;
        if (m_state.queue.isEmpty()) {
            m_state.waits++;
            m_state.changed.notifyAll();
            return PollResult::Wait;
        }
        m_item = m_state.queue.takeFirst();
        return PollResult::Work;
    }

    WorkResult work() final
    {
        Locker locker { *m_state.lock };
        if (m_item == 1) {
            m_state.firstItemStarted = true;
            m_state.changed.notifyAll();
            m_state.changed.wait(*m_state.lock, [&] {
                return m_state.releaseFirstItem;
            });
        } else {
            m_state.secondItemDone = true;
            m_state.changed.notifyAll();
        }
        return WorkResult::Continue;
    }

    // Keeps a thread whose wait timed out running, which used to leave it marked as waiting.
    bool shouldSleep(const AbstractLocker&) final { return false; }

    SharedState& m_state;
    unsigned m_item { 0 };
};

} // anonymous namespace

TEST(WTF_AutomaticThread, NotifyAfterTimedOutWaitStartsAnotherThread)
{
    SharedState state;
    Vector<Ref<TestThread>> threads;
    {
        Locker locker { *state.lock };
        threads.append(TestThread::create(locker, state));
        threads.append(TestThread::create(locker, state));
        state.workAvailable->notifyOne(locker);
        state.changed.wait(*state.lock, [&] {
            return state.waits > 0;
        });

        // Queued without a notify, so the first thread only finds it after its wait times out.
        state.queue.append(1);
        state.changed.wait(*state.lock, [&] {
            return state.firstItemStarted;
        });

        // The first thread is busy, so this has to start the second one.
        state.queue.append(2);
        state.workAvailable->notifyOne(locker);
        state.changed.waitFor(*state.lock, 10_s, [&] {
            return state.secondItemDone;
        });
        EXPECT_TRUE(state.secondItemDone);

        state.releaseFirstItem = true;
        state.shouldStop = true;
        state.changed.notifyAll();
        state.workAvailable->notifyAll(locker);
    }
    for (auto& thread : threads)
        thread->join();
}

} // namespace TestWebKitAPI
