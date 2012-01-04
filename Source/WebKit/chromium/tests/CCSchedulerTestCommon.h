/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCSchedulerTestCommon_h
#define CCSchedulerTestCommon_h

#include "cc/CCDelayBasedTimeSource.h"
#include "cc/CCThread.h"
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

namespace WebKitTests {

class FakeCCTimeSourceClient : public WebCore::CCTimeSourceClient {
public:
    FakeCCTimeSourceClient() { reset(); }
    void reset() { m_tickCalled = false; }
    bool tickCalled() const { return m_tickCalled; }

    virtual void onTimerTick() { m_tickCalled = true; }

protected:
    bool m_tickCalled;
};

class FakeCCThread : public WebCore::CCThread {
public:
    FakeCCThread() { reset(); }
    void reset()
    {
        m_pendingTaskDelay = 0;
        m_pendingTask.clear();
    }

    bool hasPendingTask() const { return m_pendingTask; }
    void runPendingTask()
    {
        ASSERT(m_pendingTask);
        OwnPtr<Task> task = m_pendingTask.release();
        task->performTask();
    }

    long long pendingDelay() const
    {
        EXPECT_TRUE(hasPendingTask());
        return m_pendingTaskDelay;
    }

    virtual void postTask(PassOwnPtr<Task>) { ASSERT_NOT_REACHED(); }
    virtual void postDelayedTask(PassOwnPtr<Task> task, long long delay)
    {
        EXPECT_TRUE(!hasPendingTask());
        m_pendingTask = task;
        m_pendingTaskDelay = delay;
    }
    virtual WTF::ThreadIdentifier threadID() const { return 0; }

protected:
    OwnPtr<Task> m_pendingTask;
    long long m_pendingTaskDelay;
};

class FakeCCTimeSource : public WebCore::CCTimeSource {
public:
    FakeCCTimeSource()
        : m_active(false)
        , m_client(0) { }

    virtual ~FakeCCTimeSource() { }

    virtual void setClient(WebCore::CCTimeSourceClient* client) { m_client = client; }
    virtual void setActive(bool b) { m_active = b; }

    void tick()
    {
        ASSERT(m_active);
        if (m_client)
            m_client->onTimerTick();
    }

protected:
    bool m_active;
    WebCore::CCTimeSourceClient* m_client;
};

class FakeCCDelayBasedTimeSource : public WebCore::CCDelayBasedTimeSource {
public:
    static PassRefPtr<FakeCCDelayBasedTimeSource> create(double intervalMs, WebCore::CCThread* thread)
    {
        return adoptRef(new FakeCCDelayBasedTimeSource(intervalMs, thread));
    }

    void setMonotonicallyIncreasingTimeMs(double time) { m_monotonicallyIncreasingTimeMs = time; }
    virtual double monotonicallyIncreasingTimeMs() const { return m_monotonicallyIncreasingTimeMs; }

protected:
    FakeCCDelayBasedTimeSource(double intervalMs, WebCore::CCThread* thread)
        : CCDelayBasedTimeSource(intervalMs, thread)
        , m_monotonicallyIncreasingTimeMs(0) { }

    double m_monotonicallyIncreasingTimeMs;
};

}

#endif // CCSchedulerTestCommon_h
