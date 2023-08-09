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
#include "ScriptExecutionContext.h"
#include "SuspendableTimer.h"

namespace WebCore {

class EventLoopTimer final : public RefCounted<EventLoopTimer>, public TimerBase, public CanMakeWeakPtr<EventLoopTimer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : bool { OneShot, Repeating };
    static Ref<EventLoopTimer> create(Type type, std::unique_ptr<EventLoopTask>&& task) { return adoptRef(*new EventLoopTimer(type, WTFMove(task))); }

    Type type() const { return m_type; }
    EventLoopTaskGroup* group() const { return m_task ? m_task->group() : nullptr; }

    void stop()
    {
        if (!m_suspended)
            TimerBase::stop();
        else
            m_suspended = false;
        m_task = nullptr;
    }

    void suspend()
    {
        m_suspended = true;
        m_savedIsActive = TimerBase::isActive();
        if (m_savedIsActive) {
            m_savedNextFireInterval = TimerBase::nextUnalignedFireInterval();
            m_savedRepeatInterval = TimerBase::repeatInterval();
            TimerBase::stop();
        }
    }

    void resume()
    {
        ASSERT(m_suspended);
        m_suspended = false;

        if (m_savedIsActive)
            start(m_savedNextFireInterval, m_savedRepeatInterval);
    }

    void startRepeating(Seconds nextFireInterval, Seconds repeatInterval)
    {
        ASSERT(m_type == Type::Repeating);
        if (!m_suspended)
            TimerBase::start(nextFireInterval, repeatInterval);
        else {
            m_savedIsActive = true;
            m_savedNextFireInterval = nextFireInterval;
            m_savedRepeatInterval = repeatInterval;
        }
    }

    void startOneShot(Seconds interval)
    {
        ASSERT(m_type == Type::OneShot);
        if (!m_suspended)
            TimerBase::startOneShot(interval);
        else {
            m_savedIsActive = true;
            m_savedNextFireInterval = interval;
            m_savedRepeatInterval = 0_s;
        }
    }

private:
    EventLoopTimer(Type type, std::unique_ptr<EventLoopTask>&& task)
        : m_task(WTFMove(task))
        , m_type(type)
    {
    }

    void fired() final
    {
        Ref protectedThis { *this };
        if (!m_task)
            return;
        m_task->execute();
        if (m_type == Type::OneShot)
            m_task->group()->removeScheduledTimer(*this);
    }

    std::unique_ptr<EventLoopTask> m_task;

    Seconds m_savedNextFireInterval;
    Seconds m_savedRepeatInterval;
    Type m_type;
    bool m_suspended { false };
    bool m_savedIsActive { false };
};

EventLoopTimerHandle::EventLoopTimerHandle() = default;

EventLoopTimerHandle::EventLoopTimerHandle(EventLoopTimer& timer)
    : m_timer(&timer)
{ }

EventLoopTimerHandle::EventLoopTimerHandle(const EventLoopTimerHandle&) = default;
EventLoopTimerHandle::EventLoopTimerHandle(EventLoopTimerHandle&&) = default;

EventLoopTimerHandle::~EventLoopTimerHandle()
{
    if (!m_timer)
        return;
    if (auto* group = m_timer->group(); group && m_timer->refCount() == 1) {
        if (m_timer->type() == EventLoopTimer::Type::OneShot)
            group->removeScheduledTimer(*m_timer);
        else
            group->removeRepeatingTimer(*m_timer);
    }
}

EventLoopTimerHandle& EventLoopTimerHandle::operator=(const EventLoopTimerHandle&) = default;
EventLoopTimerHandle& EventLoopTimerHandle::operator=(std::nullptr_t)
{
    m_timer = nullptr;
    return *this;
}

EventLoop::EventLoop() = default;
EventLoop::~EventLoop() = default;

void EventLoop::queueTask(std::unique_ptr<EventLoopTask>&& task)
{
    ASSERT(task->taskSource() != TaskSource::Microtask);
    ASSERT(task->group());
    ASSERT(isContextThread());
    scheduleToRunIfNeeded();
    m_tasks.append(WTFMove(task));
}

EventLoopTimerHandle EventLoop::scheduleTask(Seconds timeout, std::unique_ptr<EventLoopTask>&& action)
{
    auto timer = EventLoopTimer::create(EventLoopTimer::Type::OneShot, WTFMove(action));
    timer->startOneShot(timeout);

    ASSERT(timer->group());
    timer->group()->didAddTimer(timer);

    EventLoopTimerHandle handle { timer };
    m_scheduledTasks.add(timer);
    return handle;
}

void EventLoop::removeScheduledTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::OneShot);
    m_scheduledTasks.remove(timer);
}

EventLoopTimerHandle EventLoop::scheduleRepeatingTask(Seconds nextTimeout, Seconds interval, std::unique_ptr<EventLoopTask>&& action)
{
    auto timer = EventLoopTimer::create(EventLoopTimer::Type::Repeating, WTFMove(action));
    timer->startRepeating(nextTimeout, interval);

    ASSERT(timer->group());
    timer->group()->didAddTimer(timer);

    EventLoopTimerHandle handle { timer };
    m_repeatingTasks.add(timer);
    return handle;
}

void EventLoop::removeRepeatingTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::Repeating);
    m_repeatingTasks.remove(timer);
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
    if (!m_groupsWithSuspendedTasks.contains(group))
        return;
    scheduleToRunIfNeeded();
}

void EventLoop::registerGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    m_associatedGroups.add(group);
}

void EventLoop::unregisterGroup(EventLoopTaskGroup& group)
{
    ASSERT(isContextThread());
    if (m_associatedGroups.remove(group))
        stopAssociatedGroupsIfNecessary();
}

void EventLoop::stopAssociatedGroupsIfNecessary()
{
    ASSERT(isContextThread());
    for (auto& group : m_associatedGroups) {
        if (!group.isReadyToStop())
            return;
    }
    auto associatedGroups = std::exchange(m_associatedGroups, { });
    for (auto& group : associatedGroups)
        group.stopAndDiscardAllTasks();
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
        m_groupsWithSuspendedTasks.clear();
        Vector<std::unique_ptr<EventLoopTask>> remainingTasks;
        for (auto& task : tasks) {
            auto* group = task->group();
            if (!group || group->isStoppedPermanently())
                continue;

            if (group->isSuspended()) {
                m_groupsWithSuspendedTasks.add(*group);
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
    m_groupsWithSuspendedTasks.clear();
}

void EventLoopTaskGroup::markAsReadyToStop()
{
    if (isReadyToStop() || isStoppedPermanently())
        return;

    bool wasSuspended = isSuspended();
    m_state = State::ReadyToStop;
    if (auto* eventLoop = m_eventLoop.get())
        eventLoop->stopAssociatedGroupsIfNecessary();

    for (auto& timer : m_timers)
        timer.stop();

    if (wasSuspended && !isStoppedPermanently()) {
        // We we get marked as ready to stop while suspended (happens when a CachedPage gets destroyed) then the
        // queued tasks will never be able to run (since tasks don't run while suspended and we will never resume).
        // As a result, we can simply discard our tasks and stop permanently.
        stopAndDiscardAllTasks();
    }
}

void EventLoopTaskGroup::suspend()
{
    ASSERT(!isStoppedPermanently());
    ASSERT(!isReadyToStop());
    m_state = State::Suspended;
    // We don't remove suspended tasks to preserve the ordering.
    // EventLoop::run checks whether each task's group is suspended or not.
    for (auto& timer : m_timers)
        timer.suspend();
}

void EventLoopTaskGroup::resume()
{
    ASSERT(!isStoppedPermanently());
    ASSERT(!isReadyToStop());
    m_state = State::Running;
    if (auto* eventLoop = m_eventLoop.get())
        eventLoop->resumeGroup(*this);
    for (auto& timer : m_timers)
        timer.resume();
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

void EventLoopTaskGroup::runAtEndOfMicrotaskCheckpoint(EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;

    microtaskQueue().addCheckpointTask(makeUnique<EventLoopFunctionDispatchTask>(TaskSource::IndexedDB, *this, WTFMove(function)));
}

EventLoopTimerHandle EventLoopTaskGroup::scheduleTask(Seconds timeout, TaskSource source, EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return { };
    return m_eventLoop->scheduleTask(timeout, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTFMove(function)));
}

void EventLoopTaskGroup::removeScheduledTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::OneShot);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->removeScheduledTimer(timer);
    m_timers.remove(timer);
}

EventLoopTimerHandle EventLoopTaskGroup::scheduleRepeatingTask(Seconds nextTimeout, Seconds interval, TaskSource source, EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return { };
    return m_eventLoop->scheduleRepeatingTask(nextTimeout, interval, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTFMove(function)));
}

void EventLoopTaskGroup::removeRepeatingTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::Repeating);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->removeRepeatingTimer(timer);
    m_timers.remove(timer);
}

void EventLoopTaskGroup::didAddTimer(EventLoopTimer& timer)
{
    auto result = m_timers.add(timer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void EventLoopTaskGroup::didRemoveTimer(EventLoopTimer& timer)
{
    auto didRemove = m_timers.remove(timer);
    ASSERT_UNUSED(didRemove, didRemove);
}

void EventLoop::forEachAssociatedContext(const Function<void(ScriptExecutionContext&)>& apply)
{
    m_associatedContexts.forEach(apply);
}

void EventLoop::addAssociatedContext(ScriptExecutionContext& context)
{
    m_associatedContexts.add(context);
}

void EventLoop::removeAssociatedContext(ScriptExecutionContext& context)
{
    m_associatedContexts.remove(context);
}

} // namespace WebCore
