/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "TaskController.h"

#include "TaskSignal.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TaskController);

Ref<TaskController> TaskController::create(ScriptExecutionContext& context, const TaskControllerInit& init)
{
    return adoptRef(*new TaskController(TaskSignal::create(&context, init.priority)));
}

TaskController::TaskController(Ref<TaskSignal>&& signal)
    : AbortController(WTF::move(signal))
{
}

TaskController::~TaskController() = default;

Ref<TaskSignal> TaskController::taskSignal()
{
    return downcast<TaskSignal>(signal());
}

void TaskController::setPriority(TaskPriority newPriority)
{
    Ref signal = taskSignal();
    auto previousPriority = signal->priority();
    if (previousPriority == newPriority)
        return;
    signal->setPriorityInternal(newPriority);
    signal->dispatchPriorityChangeEvent(previousPriority);
}

} // namespace WebCore
