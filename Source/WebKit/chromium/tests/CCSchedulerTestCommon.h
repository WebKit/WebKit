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
#include "cc/CCFrameRateController.h"
#include "cc/CCThread.h"
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

namespace WebKitTests {

class FakeCCTimeSourceClient : public WebCore::CCTimeSourceClient {
public:
    FakeCCTimeSourceClient() { reset(); }
    void reset() { m_tickCalled = false; }
    bool tickCalled() const { return m_tickCalled; }

    virtual void onTimerTick() OVERRIDE { m_tickCalled = true; }

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
        m_runPendingTaskOnOverwrite = false;
    }

    void runPendingTaskOnOverwrite(bool enable)
    {
        m_runPendingTaskOnOverwrite = enable;
    }

    bool hasPendingTask() const { return m_pendingTask; }
    void runPendingTask()
    {
        ASSERT(m_pendingTask);
        OwnPtr<Task> task = m_pendingTask.release();
        task->performTask();
    }

    long long pendingDelayMs() const
    {
        EXPECT_TRUE(hasPendingTask());
        return m_pendingTaskDelay;
    }

    virtual void postTask(PassOwnPtr<Task>) { ASSERT_NOT_REACHED(); }
    virtual void postDelayedTask(PassOwnPtr<Task> task, long long delay)
    {
        if (m_runPendingTaskOnOverwrite && hasPendingTask())
            runPendingTask();

        EXPECT_TRUE(!hasPendingTask());
        m_pendingTask = task;
        m_pendingTaskDelay = delay;
    }
    virtual WTF::ThreadIdentifier threadID() const { return 0; }

protected:
    OwnPtr<Task> m_pendingTask;
    long long m_pendingTaskDelay;
    bool m_runPendingTaskOnOverwrite;
};

class FakeCCTimeSource : public WebCore::CCTimeSource {
public:
    FakeCCTimeSource()
        : m_active(false)
        , m_client(0) { }

    virtual ~FakeCCTimeSource() { }

    virtual void setClient(WebCore::CCTimeSourceClient* client) OVERRIDE { m_client = client; }
    virtual void setActive(bool b) OVERRIDE { m_active = b; }
    virtual bool active() const OVERRIDE { return m_active; }
    virtual void setTimebaseAndInterval(double timebase, double interval) OVERRIDE { }
    virtual double lastTickTime() OVERRIDE { return 0; }
    virtual double nextTickTime() OVERRIDE { return 0; }

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
    static PassRefPtr<FakeCCDelayBasedTimeSource> create(double interval, WebCore::CCThread* thread)
    {
        return adoptRef(new FakeCCDelayBasedTimeSource(interval, thread));
    }

    void setMonotonicallyIncreasingTime(double time) { m_monotonicallyIncreasingTime = time; }
    virtual double monotonicallyIncreasingTime() const { return m_monotonicallyIncreasingTime; }

protected:
    FakeCCDelayBasedTimeSource(double interval, WebCore::CCThread* thread)
        : CCDelayBasedTimeSource(interval, thread)
        , m_monotonicallyIncreasingTime(0) { }

    double m_monotonicallyIncreasingTime;
};

class FakeCCFrameRateController : public WebCore::CCFrameRateController {
public:
    FakeCCFrameRateController(PassRefPtr<WebCore::CCTimeSource> timer) : WebCore::CCFrameRateController(timer) { }

    int numFramesPending() const { return m_numFramesPending; }
};

}

#endif // CCSchedulerTestCommon_h
