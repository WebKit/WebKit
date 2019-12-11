/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include <wtf/Condition.h>
#include <wtf/ThreadGroup.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

enum class Mode { Add, AddCurrentThread };
static void testThreadGroup(Mode mode)
{
    std::shared_ptr<ThreadGroup> threadGroup = ThreadGroup::create();
    unsigned numberOfThreads = 16;
    unsigned waitingThreads = 0;
    bool restarting = false;
    Lock lock;
    Condition condition;
    Condition restartCondition;
    Vector<Ref<Thread>> threads;

    {
        auto locker = holdLock(lock);
        for (unsigned i = 0; i < numberOfThreads; ++i) {
            Ref<Thread> thread = Thread::create("ThreadGroupWorker", [&] {
                auto locker = holdLock(lock);
                if (mode == Mode::AddCurrentThread)
                    threadGroup->addCurrentThread();
                ++waitingThreads;
                condition.notifyOne();
                restartCondition.wait(lock, [&] {
                    return restarting;
                });
            });
            if (mode == Mode::Add)
                EXPECT_TRUE(threadGroup->add(thread.get()) == ThreadGroupAddResult::NewlyAdded);
            threads.append(WTFMove(thread));
        }

        condition.wait(lock, [&] {
            return waitingThreads == numberOfThreads;
        });
    }

    {
        auto threadGroupLocker = holdLock(threadGroup->getLock());
        EXPECT_EQ(threads.size(), numberOfThreads);
        EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), numberOfThreads);
        {
            auto locker = holdLock(lock);
            restarting = true;
            restartCondition.notifyAll();
        }

        // While holding ThreadGroup lock, threads do not exit.
        WTF::sleep(100_ms);
        EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), numberOfThreads);
    }
    {
        for (auto& thread : threads)
            thread->waitForCompletion();

        auto threadGroupLocker = holdLock(threadGroup->getLock());
        EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), 0u);
    }
}

TEST(WTF, ThreadGroupAdd)
{
    testThreadGroup(Mode::Add);
}

TEST(WTF, ThreadGroupAddCurrentThread)
{
    testThreadGroup(Mode::AddCurrentThread);
}

TEST(WTF, ThreadGroupDoNotAddDeadThread)
{
    std::shared_ptr<ThreadGroup> threadGroup = ThreadGroup::create();
    Ref<Thread> thread = Thread::create("ThreadGroupWorker", [&] { });
    thread->waitForCompletion();
    EXPECT_TRUE(threadGroup->add(thread.get()) == ThreadGroupAddResult::NotAdded);

    auto threadGroupLocker = holdLock(threadGroup->getLock());
    EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), 0u);
}

TEST(WTF, ThreadGroupAddDuplicateThreads)
{
    bool restarting = false;
    Lock lock;
    Condition restartCondition;
    std::shared_ptr<ThreadGroup> threadGroup = ThreadGroup::create();
    Ref<Thread> thread = Thread::create("ThreadGroupWorker", [&] {
        auto locker = holdLock(lock);
        restartCondition.wait(lock, [&] {
            return restarting;
        });
    });
    EXPECT_TRUE(threadGroup->add(thread.get()) == ThreadGroupAddResult::NewlyAdded);
    EXPECT_TRUE(threadGroup->add(thread.get()) == ThreadGroupAddResult::AlreadyAdded);

    {
        auto threadGroupLocker = holdLock(threadGroup->getLock());
        EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), 1u);
    }

    {
        auto locker = holdLock(lock);
        restarting = true;
        restartCondition.notifyAll();
    }
    thread->waitForCompletion();
    {
        auto threadGroupLocker = holdLock(threadGroup->getLock());
        EXPECT_EQ(threadGroup->threads(threadGroupLocker).size(), 0u);
    }
}

static unsigned countThreadGroups(Vector<Ref<Thread>>& threads)
{
    unsigned count = 0;
    for (auto& thread : threads)
        count += thread->numberOfThreadGroups();

    return count;
}

TEST(WTF, ThreadGroupRemove)
{
    const unsigned NumberOfThreads = 16;

    Lock lock;
    Condition condition;
    Condition threadRunningCondition;
    unsigned waitingThreads = 0;
    bool threadRunning = true;

    Vector<Ref<Thread>> threads;

    auto threadGroup = ThreadGroup::create();
    for (unsigned i = 0; i < NumberOfThreads; i++) {
        auto thread = Thread::create("ThreadGroupWorker", [&]() {
            auto locker = holdLock(lock);
            ++waitingThreads;
            condition.notifyOne();
            threadRunningCondition.wait(lock, [&]() {
                return !threadRunning;
            });
        });
        threadGroup->add(thread.get());
        threads.append(WTFMove(thread));
    }

    EXPECT_EQ(NumberOfThreads, countThreadGroups(threads));

    {
        auto locker = holdLock(lock);
        condition.wait(lock, [&] {
            return waitingThreads == NumberOfThreads;
        });
    }

    EXPECT_EQ(NumberOfThreads, countThreadGroups(threads));

    threadGroup = nullptr;

    EXPECT_EQ(0U, countThreadGroups(threads));

    {
        auto locker = holdLock(lock);
        threadRunning = false;
        threadRunningCondition.notifyAll();
    }
    for (unsigned i = 0; i < NumberOfThreads; i++)
        threads[i]->waitForCompletion();
}

} // namespace TestWebKitAPI
