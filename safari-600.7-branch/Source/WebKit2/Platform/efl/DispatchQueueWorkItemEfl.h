/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef DispatchQueueWorkItemEfl_h
#define DispatchQueueWorkItemEfl_h

#include <WorkQueue.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <wtf/Functional.h>
#include <wtf/RefCounted.h>

class WorkItem {
public:
    static std::unique_ptr<WorkItem> create(PassRefPtr<WorkQueue> workQueue, std::function<void ()> function)
    {
        return std::unique_ptr<WorkItem>(new WorkItem(workQueue, WTF::move(function)));
    }
    void dispatch() { m_function(); }

protected:
    WorkItem(PassRefPtr<WorkQueue> workQueue, std::function<void ()> function)
        : m_workQueue(workQueue)
        , m_function(WTF::move(function))
    {
    }

private:
    RefPtr<WorkQueue> m_workQueue;
    std::function<void ()> m_function;
};

class TimerWorkItem : public WorkItem {
public:
    static std::unique_ptr<TimerWorkItem> create(PassRefPtr<WorkQueue> workQueue, std::function<void ()> function, std::chrono::nanoseconds delayNanoSeconds)
    {
        ASSERT(delayNanoSeconds.count() >= 0);
        return std::unique_ptr<TimerWorkItem>(new TimerWorkItem(workQueue, WTF::move(function), monotonicallyIncreasingTime() * 1000000000.0 + delayNanoSeconds.count()));
    }
    double expirationTimeNanoSeconds() const { return m_expirationTimeNanoSeconds; }
    bool hasExpired(double currentTimeNanoSeconds) const { return currentTimeNanoSeconds >= m_expirationTimeNanoSeconds; }

protected:
    TimerWorkItem(PassRefPtr<WorkQueue> workQueue, std::function<void ()> function, double expirationTimeNanoSeconds)
        : WorkItem(workQueue, WTF::move(function))
        , m_expirationTimeNanoSeconds(expirationTimeNanoSeconds)
    {
    }

private:
    double m_expirationTimeNanoSeconds;
};

#endif // DispatchQueueWorkItemEfl_h
