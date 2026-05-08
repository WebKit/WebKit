/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "TaskSignal.h"

#include "EventNames.h"
#include "TaskPriorityChangeEvent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TaskSignal);

Ref<TaskSignal> TaskSignal::create(ScriptExecutionContext* context, TaskPriority priority)
{
    return adoptRef(*new TaskSignal(context, priority));
}

TaskSignal::TaskSignal(ScriptExecutionContext* context, TaskPriority priority)
    : AbortSignal(context, Aborted::No, EventTargetTagged::TaskSignal)
    , m_priority(priority)
{
}

TaskSignal::~TaskSignal() = default;

void TaskSignal::setPriorityInternal(TaskPriority priority)
{
    m_priority = priority;
}

void TaskSignal::dispatchPriorityChangeEvent(TaskPriority previousPriority)
{
    TaskPriorityChangeEvent::Init init;
    init.previousPriority = previousPriority;
    auto event = TaskPriorityChangeEvent::create(eventNames().prioritychangeEvent, init, Event::IsTrusted::Yes);
    dispatchEvent(event.get());
}

} // namespace WebCore
