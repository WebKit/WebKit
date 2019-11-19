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
#include "WindowEventLoop.h"

#include "Document.h"

namespace WebCore {

static HashMap<RegistrableDomain, WindowEventLoop*>& windowEventLoopMap()
{
    RELEASE_ASSERT(isMainThread());
    static NeverDestroyed<HashMap<RegistrableDomain, WindowEventLoop*>> map;
    return map.get();
}

Ref<WindowEventLoop> WindowEventLoop::ensureForRegistrableDomain(const RegistrableDomain& domain)
{
    auto addResult = windowEventLoopMap().add(domain, nullptr);
    if (UNLIKELY(addResult.isNewEntry)) {
        auto newEventLoop = adoptRef(*new WindowEventLoop(domain));
        addResult.iterator->value = newEventLoop.ptr();
        return newEventLoop;
    }
    return *addResult.iterator->value;
}

inline WindowEventLoop::WindowEventLoop(const RegistrableDomain& domain)
    : m_domain(domain)
{
}

WindowEventLoop::~WindowEventLoop()
{
    auto didRemove = windowEventLoopMap().remove(m_domain);
    RELEASE_ASSERT(didRemove);
}

void AbstractEventLoop::queueTask(std::unique_ptr<EventLoopTask>&& task)
{
    ASSERT(task->group());
    ASSERT(isContextThread());
    scheduleToRunIfNeeded();
    m_tasks.append(WTFMove(task));
}

void AbstractEventLoop::resumeGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    if (!m_groupsWithSuspenedTasks.contains(group))
        return;
    scheduleToRunIfNeeded();
}

void AbstractEventLoop::stopGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    m_tasks.removeAllMatching([&group] (auto& task) {
        return group.matchesTask(*task);
    });
}

void AbstractEventLoop::scheduleToRunIfNeeded()
{
    if (m_isScheduledToRun)
        return;
    m_isScheduledToRun = true;
    scheduleToRun();
}

void WindowEventLoop::scheduleToRun()
{
    callOnMainThread([eventLoop = makeRef(*this)] () {
        eventLoop->run();
    });
}

bool WindowEventLoop::isContextThread() const
{
    return isMainThread();
}

void AbstractEventLoop::run()
{
    m_isScheduledToRun = false;
    if (m_tasks.isEmpty())
        return;

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
    }
    for (auto& task : m_tasks)
        remainingTasks.append(WTFMove(task));
    m_tasks = WTFMove(remainingTasks);
}

void AbstractEventLoop::clearAllTasks()
{
    m_tasks.clear();
    m_groupsWithSuspenedTasks.clear();
}

class EventLoopFunctionDispatchTask : public EventLoopTask {
public:
    EventLoopFunctionDispatchTask(TaskSource source, EventLoopTaskGroup& group, AbstractEventLoop::TaskFunction&& function)
        : EventLoopTask(source, group)
        , m_function(WTFMove(function))
    {
    }

    void execute() final { m_function(); }

private:
    AbstractEventLoop::TaskFunction m_function;
};

void EventLoopTaskGroup::queueTask(TaskSource source, AbstractEventLoop::TaskFunction&& function)
{
    return queueTask(makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTFMove(function)));
}

} // namespace WebCore
