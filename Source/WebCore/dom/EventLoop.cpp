/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "ScriptExecutionContextInlines.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSMicrotaskDispatcher.h>
#include <JavaScriptCore/MicrotaskQueueInlines.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EventLoopTask);
WTF_MAKE_TZONE_ALLOCATED_IMPL(EventLoopTimerHandle);
WTF_MAKE_TZONE_ALLOCATED_IMPL(EventLoopTaskGroup);

class EventLoopTimer final : public RefCountedAndCanMakeWeakPtr<EventLoopTimer>, public TimerBase {
    WTF_MAKE_TZONE_ALLOCATED(EventLoopTimer);
public:
    enum class Type : bool { OneShot, Repeating };
    static Ref<EventLoopTimer> create(Type type, std::unique_ptr<EventLoopTask>&& task) { return adoptRef(*new EventLoopTimer(type, WTF::move(task))); }

    Type NODELETE type() const { return m_type; }
    EventLoopTaskGroup* NODELETE group() const { return m_task ? m_task->group() : nullptr; }
    bool NODELETE isSuspended() const { return m_suspended; }

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

    void adjustNextFireTime(Seconds delta)
    {
        if (!m_suspended)
            TimerBase::augmentFireInterval(delta);
        else if (m_savedIsActive)
            m_savedNextFireInterval += delta;
        else {
            m_savedIsActive = true;
            m_savedNextFireInterval = delta;
            m_savedRepeatInterval = 0_s;
        }
    }

    void adjustRepeatInterval(Seconds delta)
    {
        if (!m_suspended)
            TimerBase::augmentRepeatInterval(delta);
        else if (m_savedIsActive) {
            m_savedNextFireInterval += delta;
            m_savedRepeatInterval += delta;
        } else {
            m_savedIsActive = true;
            m_savedNextFireInterval = delta;
            m_savedRepeatInterval = delta;
        }
    }

private:
    EventLoopTimer(Type type, std::unique_ptr<EventLoopTask>&& task)
        : m_task(WTF::move(task))
        , m_type(type)
    {
    }

    void fired() final
    {
        Ref protectedThis { *this };
        if (!m_task)
            return;
        WeakPtr group = m_task->group();
        m_task->execute();
        if (group && m_type == Type::OneShot)
            group->removeScheduledTimer(*this);
    }

    std::unique_ptr<EventLoopTask> m_task;

    Seconds m_savedNextFireInterval;
    Seconds m_savedRepeatInterval;
    Type m_type;
    bool m_suspended { false };
    bool m_savedIsActive { false };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EventLoopTimer);

EventLoopTimerHandle::EventLoopTimerHandle() = default;

EventLoopTimerHandle::EventLoopTimerHandle(EventLoopTimer& timer)
    : m_timer(&timer)
{ }

EventLoopTimerHandle::EventLoopTimerHandle(const EventLoopTimerHandle&) = default;
EventLoopTimerHandle::EventLoopTimerHandle(EventLoopTimerHandle&&) = default;

EventLoopTimerHandle::~EventLoopTimerHandle()
{
    RefPtr timer = std::exchange(m_timer, nullptr);
    if (!timer)
        return;
    if (CheckedPtr group = timer->group(); group && timer->refCount() == 1) {
        if (timer->type() == EventLoopTimer::Type::OneShot)
            group->removeScheduledTimer(*timer);
        else
            group->removeRepeatingTimer(*timer);
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
    m_tasks.append(WTF::move(task));
}

EventLoopTimerHandle EventLoop::scheduleTask(Seconds timeout, TimerAlignment* alignment, HasReachedMaxNestingLevel hasReachedMaxNestingLevel, std::unique_ptr<EventLoopTask>&& action)
{
    auto timer = EventLoopTimer::create(EventLoopTimer::Type::OneShot, WTF::move(action));
    if (alignment)
        timer->setTimerAlignment(*alignment);
    timer->setHasReachedMaxNestingLevel(hasReachedMaxNestingLevel == HasReachedMaxNestingLevel::Yes);
    timer->startOneShot(timeout);
    if (timer->group()->isSuspended())
        timer->suspend();

    ASSERT(timer->group());
    timer->group()->didAddTimer(timer);

    EventLoopTimerHandle handle { timer };
    m_scheduledTasks.add(timer);
    invalidateNextTimerFireTimeCache();
    return handle;
}

void EventLoop::removeScheduledTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::OneShot);
    m_scheduledTasks.remove(timer);
    invalidateNextTimerFireTimeCache();
}

EventLoopTimerHandle EventLoop::scheduleRepeatingTask(Seconds nextTimeout, Seconds interval, TimerAlignment* alignment, HasReachedMaxNestingLevel hasReachedMaxNestingLevel, std::unique_ptr<EventLoopTask>&& action)
{
    auto timer = EventLoopTimer::create(EventLoopTimer::Type::Repeating, WTF::move(action));
    if (alignment)
        timer->setTimerAlignment(*alignment);
    timer->setHasReachedMaxNestingLevel(hasReachedMaxNestingLevel == HasReachedMaxNestingLevel::Yes);
    timer->startRepeating(nextTimeout, interval);
    if (timer->group()->isSuspended())
        timer->suspend();

    ASSERT(timer->group());
    timer->group()->didAddTimer(timer);

    EventLoopTimerHandle handle { timer };
    m_repeatingTasks.add(timer);
    invalidateNextTimerFireTimeCache();
    return handle;
}

void EventLoop::removeRepeatingTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::Repeating);
    m_repeatingTasks.remove(timer);
    invalidateNextTimerFireTimeCache();
}

void EventLoop::queueMicrotask(JSC::QueuedTask&& microtask)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& microtaskQueue = this->microtaskQueue();
    microtaskQueue.enqueue(WTF::move(microtask));
    scheduleToRunIfNeeded(); // FIXME: Remove this once everything is integrated with the event loop.
}

void EventLoop::performMicrotaskCheckpoint(JSC::VM& vm)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& microtaskQueue = this->microtaskQueue();
    microtaskQueue.performMicrotaskCheckpoint(vm);
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
    for (CheckedRef group : m_associatedGroups) {
        if (!group->isReadyToStop())
            return;
    }
    auto associatedGroups = std::exchange(m_associatedGroups, { });
    for (CheckedRef group : associatedGroups)
        group->stopAndDiscardAllTasks();
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
    if (microtaskQueue().isScheduledToRun())
        return;
    microtaskQueue().setIsScheduledToRun(true);
    scheduleToRun();
}

void EventLoop::run(JSC::VM& vm, std::optional<ApproximateTime> deadline)
{
    microtaskQueue().setIsScheduledToRun(false);
    bool didPerformMicrotaskCheckpoint = false;

    if (!m_tasks.isEmpty()) {
        auto tasks = std::exchange(m_tasks, { });
        m_groupsWithSuspendedTasks.clear();
        TaskVector remainingTasks;
        bool hasReachedDeadline = false;
        for (auto& task : tasks) {
            {
                CheckedPtr group = task->group();
                if (!group || group->isStoppedPermanently())
                    continue;

                hasReachedDeadline = hasReachedDeadline || (deadline && ApproximateTime::now() > *deadline);
                if (group->isSuspended() || hasReachedDeadline) {
                    m_groupsWithSuspendedTasks.add(*group);
                    remainingTasks.append(WTF::move(task));
                    continue;
                }
            }

            task->execute();
            didPerformMicrotaskCheckpoint = true;
            performMicrotaskCheckpoint(vm);
        }
        for (auto& task : m_tasks)
            remainingTasks.append(WTF::move(task));
        m_tasks = WTF::move(remainingTasks);

        if (!m_tasks.isEmpty() && hasReachedDeadline)
            scheduleToRunIfNeeded();
    }

    // FIXME: Remove this once everything is integrated with the event loop.
    if (!didPerformMicrotaskCheckpoint)
        performMicrotaskCheckpoint(vm);
}

void EventLoop::clearAllTasks()
{
    m_tasks.clear();
    m_groupsWithSuspendedTasks.clear();
}

bool EventLoop::hasTasksForFullyActiveDocument() const
{
    return m_tasks.containsIf([](auto& task) {
        auto group = task->group();
        return group && !group->isStoppedPermanently() && !group->isSuspended();
    });
}

void EventLoop::forEachAssociatedContext(NOESCAPE const Function<void(ScriptExecutionContext&)>& apply)
{
    m_associatedContexts.forEach(apply);
}

bool EventLoop::findMatchingAssociatedContext(NOESCAPE const Function<bool(ScriptExecutionContext&)>& predicate)
{
    for (Ref context : m_associatedContexts) {
        if (predicate(context.get()))
            return true;
    }
    return false;
}

void EventLoop::addAssociatedContext(ScriptExecutionContext& context)
{
    m_associatedContexts.add(context);
}

void EventLoop::removeAssociatedContext(ScriptExecutionContext& context)
{
    m_associatedContexts.remove(context);
}

Markable<MonotonicTime> EventLoop::nextTimerFireTime() const
{
    if (!m_nextTimerFireTimeCache) {
        Markable<MonotonicTime> nextFireTime;
        auto updateResult = [&](auto& tasks) {
            for (auto& timer : tasks) {
                if (timer.isSuspended())
                    continue;
                if (!nextFireTime || timer.nextFireTime() < *nextFireTime)
                    nextFireTime = timer.nextFireTime();
            }
        };
        updateResult(m_scheduledTasks);
        updateResult(m_repeatingTasks);
        m_nextTimerFireTimeCache = nextFireTime;
    }
    return m_nextTimerFireTimeCache;
}

EventLoopTaskGroup::EventLoopTaskGroup(EventLoop& eventLoop)
    : m_eventLoop(eventLoop)
{
    eventLoop.registerGroup(*this);
}

EventLoopTaskGroup::~EventLoopTaskGroup()
{
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->unregisterGroup(*this);
}

void EventLoopTaskGroup::setScriptExecutionContext(ScriptExecutionContext& context)
{
    m_context = context;
}

void EventLoopTaskGroup::stopAndDiscardAllTasks()
{
    ASSERT(isReadyToStop());
    m_state = State::Stopped;
    if (RefPtr context = m_context.get()) {
        context->forEachMicrotaskGlobalObject([](auto& globalObject) {
            globalObject.setMicrotaskRunnability(JSC::QueuedTaskResult::Discard);
        });
    }
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->stopGroup(*this);
}

void EventLoopTaskGroup::markAsReadyToStop()
{
    if (isReadyToStop() || isStoppedPermanently())
        return;

    bool wasSuspended = isSuspended();
    m_state = State::ReadyToStop;
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->stopAssociatedGroupsIfNecessary();

    for (Ref timer : m_timers)
        timer->stop();

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
    for (Ref timer : m_timers)
        timer->suspend();
    if (RefPtr eventLoop = m_eventLoop.get())
        m_eventLoop->invalidateNextTimerFireTimeCache();
}

void EventLoopTaskGroup::resume()
{
    ASSERT(!isStoppedPermanently());
    ASSERT(!isReadyToStop());
    m_state = State::Running;
    if (RefPtr eventLoop = m_eventLoop.get()) {
        eventLoop->resumeGroup(*this);
        eventLoop->invalidateNextTimerFireTimeCache();
    }
    for (Ref timer : m_timers)
        timer->resume();
}

void EventLoopTaskGroup::queueTask(std::unique_ptr<EventLoopTask>&& task)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;
    ASSERT(task->group() == this);
    protect(m_eventLoop)->queueTask(WTF::move(task));
}

class EventLoopFunctionDispatchTask : public EventLoopTask {
    WTF_MAKE_TZONE_ALLOCATED(EventLoopFunctionDispatchTask);
public:
    EventLoopFunctionDispatchTask(TaskSource source, EventLoopTaskGroup& group, EventLoop::TaskFunction&& function)
        : EventLoopTask(source, group)
        , m_function(WTF::move(function))
    {
    }

    void execute() final { m_function(); }

private:
    EventLoop::TaskFunction m_function;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EventLoopFunctionDispatchTask);

void EventLoopTaskGroup::queueTask(TaskSource source, EventLoop::TaskFunction&& function)
{
    return queueTask(makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTF::move(function)));
}

class EventLoopFunctionMicrotaskDispatcher final : public WebCoreMicrotaskDispatcher {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(EventLoopFunctionMicrotaskDispatcher);
public:
    EventLoopFunctionMicrotaskDispatcher(EventLoopTaskGroup& group, EventLoop::TaskFunction&& function)
        : WebCoreMicrotaskDispatcher(Type::WebCoreFunction, group)
        , m_function(WTF::move(function))
    {
    }

    ~EventLoopFunctionMicrotaskDispatcher() final = default;

    JSC::QueuedTask::Result run(JSC::QueuedTask&) final
    {
        auto runnability = currentRunnability();
        if (runnability == JSC::QueuedTask::Result::Executed)
            m_function();
        return runnability;
    }

    static Ref<EventLoopFunctionMicrotaskDispatcher> create(EventLoopTaskGroup& group, EventLoop::TaskFunction&& function)
    {
        return adoptRef(*new EventLoopFunctionMicrotaskDispatcher(group, WTF::move(function)));
    }

private:
    EventLoop::TaskFunction m_function;
};

WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(EventLoopFunctionMicrotaskDispatcher);

void EventLoopTaskGroup::queueMicrotask(JSC::VM& vm, EventLoop::TaskFunction&& function)
{
    JSC::JSLockHolder locker(vm);
    auto* cell = JSC::JSMicrotaskDispatcher::create(vm, EventLoopFunctionMicrotaskDispatcher::create(*this, WTF::move(function)));
    queueMicrotask(JSC::QueuedTask { cell });
}

void EventLoopTaskGroup::queueMicrotask(JSC::QueuedTask&& task)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;

    protect(m_eventLoop)->queueMicrotask(WTF::move(task));
}

void EventLoopTaskGroup::performMicrotaskCheckpoint(JSC::VM& vm)
{
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->performMicrotaskCheckpoint(vm);
}

void EventLoopTaskGroup::runAtEndOfMicrotaskCheckpoint(EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return;

    SUPPRESS_UNCOUNTED_LOCAL auto& microtaskQueue = this->microtaskQueue();
    microtaskQueue.addCheckpointTask(makeUnique<EventLoopFunctionDispatchTask>(TaskSource::IndexedDB, *this, WTF::move(function)));
}

EventLoopTimerHandle EventLoopTaskGroup::scheduleTask(Seconds timeout, TaskSource source, EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return { };
    return protect(m_eventLoop)->scheduleTask(timeout, nullptr, HasReachedMaxNestingLevel::No, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTF::move(function)));
}

EventLoopTimerHandle EventLoopTaskGroup::scheduleTask(Seconds timeout, TimerAlignment& alignment, HasReachedMaxNestingLevel hasReachedMaxNestingLevel, TaskSource source, EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return { };
    return protect(m_eventLoop)->scheduleTask(timeout, &alignment, hasReachedMaxNestingLevel, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTF::move(function)));
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
    return protect(m_eventLoop)->scheduleRepeatingTask(nextTimeout, interval, nullptr, HasReachedMaxNestingLevel::No, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTF::move(function)));
}

EventLoopTimerHandle EventLoopTaskGroup::scheduleRepeatingTask(Seconds nextTimeout, Seconds interval, TimerAlignment& alignment, HasReachedMaxNestingLevel hasReachedMaxNestingLevel, TaskSource source, EventLoop::TaskFunction&& function)
{
    if (m_state == State::Stopped || !m_eventLoop)
        return { };
    return protect(m_eventLoop)->scheduleRepeatingTask(nextTimeout, interval, &alignment, hasReachedMaxNestingLevel, makeUnique<EventLoopFunctionDispatchTask>(source, *this, WTF::move(function)));
}

void EventLoopTaskGroup::removeRepeatingTimer(EventLoopTimer& timer)
{
    ASSERT(timer.type() == EventLoopTimer::Type::Repeating);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->removeRepeatingTimer(timer);
    m_timers.remove(timer);
}

void EventLoopTaskGroup::didChangeTimerAlignmentInterval(EventLoopTimerHandle handle)
{
    if (!handle.m_timer)
        return;
    ASSERT(m_timers.contains(*handle.m_timer));
    handle.m_timer->didChangeAlignmentInterval();
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->invalidateNextTimerFireTimeCache();
}

void EventLoopTaskGroup::setTimerHasReachedMaxNestingLevel(EventLoopTimerHandle handle, bool value)
{
    if (!handle.m_timer)
        return;
    ASSERT(m_timers.contains(*handle.m_timer));
    handle.m_timer->setHasReachedMaxNestingLevel(value);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->invalidateNextTimerFireTimeCache();
}

void EventLoopTaskGroup::adjustTimerNextFireTime(EventLoopTimerHandle handle, Seconds delta)
{
    RefPtr timer = handle.m_timer;
    if (!timer)
        return;
    ASSERT(m_timers.contains(*timer));
    timer->adjustNextFireTime(delta);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->invalidateNextTimerFireTimeCache();
}

void EventLoopTaskGroup::adjustTimerRepeatInterval(EventLoopTimerHandle handle, Seconds delta)
{
    RefPtr timer = handle.m_timer;
    if (!timer)
        return;
    ASSERT(m_timers.contains(*timer));
    timer->adjustRepeatInterval(delta);
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->invalidateNextTimerFireTimeCache();
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

} // namespace WebCore
