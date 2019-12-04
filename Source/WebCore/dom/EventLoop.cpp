/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "EventLoop.h"

#include "Microtasks.h"

namespace WebCore {

void EventLoop::queueTask(std::unique_ptr<EventLoopTask>&& task)
{
    ASSERT(task->taskSource() != TaskSource::Microtask);
    ASSERT(task->group());
    ASSERT(isContextThread());
    scheduleToRunIfNeeded();
    m_tasks.append(WTFMove(task));
}

void EventLoop::queueMicrotask(std::unique_ptr<EventLoopTask>&& microtask)
{
    ASSERT(microtask->taskSource() == TaskSource::Microtask);
    microtaskQueue().append(WTFMove(microtask));
    scheduleToRunIfNeeded(); // FIXME: Remove this once everything is integrated with the event loop.
}

void EventLoop::performMicrotaskCheckpoint()
{
    microtaskQueue().performMicrotaskCheckpoint();
}

void EventLoop::resumeGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    if (!m_groupsWithSuspenedTasks.contains(group))
        return;
    scheduleToRunIfNeeded();
}

void EventLoop::stopGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    m_tasks.removeAllMatching([&group] (auto& task) {
        return group.matchesTask(*task);
    });
}

void EventLoop::scheduleToRunIfNeeded()
{
    if (m_isScheduledToRun)
        return;
    m_isScheduledToRun = true;
    scheduleToRun();
}

void EventLoop::run()
{
    m_isScheduledToRun = false;
    bool didPerformMicrotaskCheckpoint = false;

    if (!m_tasks.isEmpty()) {
        auto tasks = std::exchange(m_tasks, { });
        m_groupsWithSuspenedTasks.clear();
        Vector<std::unique_ptr<EventLoopTask>> remainingTasks;
        for (auto& task : tasks) {
            auto* group = task->group();
            if (!group || group->isStoppedPermanently())
                continue;

            if (group->isSuspended()) {
                m_groupsWithSuspenedTasks.add(group);
                remainingTasks.append(WTFMove(task));
                continue;
            }

            task->execute();
            didPerformMicrotaskCheckpoint = true;
            microtaskQueue().performMicrotaskCheckpoint();
        }
        for (auto& task : m_tasks)
            remainingTasks.append(WTFMove(task));
        m_tasks = WTFMove(remainingTasks);
    }

    // FIXME: Remove this once everything is integrated with the event loop.
    if (!didPerformMicrotaskCheckpoint)
        microtaskQueue().performMicrotaskCheckpoint();
}

void EventLoop::clearAllTasks()
{
    m_tasks.clear();
    m_groupsWithSuspenedTasks.clear();
}

void EventLoopTaskGroup::queueTask(std::unique_ptr<EventLoopTask>&& task)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;
    ASSERT(task->group() == this);
    m_eventLoop->queueTask(WTFMove(task));
}

class EventLoopFunctionDispatchTask : public EventLoopTask {
public:
    EventLoopFunctionDispatchTask(TaskSource source, EventLoopTaskGroup& group, EventLoop::TaskFunction&& function)
        : EventLoopTask(source, group)
        , m_function(WTFMove(function))
    {
    }

    void execute() final { m_function(); }

private:
    EventLoop::TaskFunction m_function;
};

void EventLoopTaskGroup::queueTask(TaskSource source, EventLoop::TaskFunction&& function)
{
    return queueTask(makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTFMove(function)));
}

void EventLoopTaskGroup::queueMicrotask(EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;
    m_eventLoop->queueMicrotask(makeUnique<EventLoopFunctionDispatchTask>(TaskSource::Microtask, *this, WTFMove(function)));
}

void EventLoopTaskGroup::performMicrotaskCheckpoint()
{
    if (m_eventLoop)
        m_eventLoop->performMicrotaskCheckpoint();
}

} // namespace WebCore
