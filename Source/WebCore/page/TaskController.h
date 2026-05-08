/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include "AbortController.h"
#include "TaskPriority.h"

namespace WebCore {

class ScriptExecutionContext;
class TaskSignal;

struct TaskControllerInit {
    TaskPriority priority { TaskPriority::UserVisible };
};

class TaskController final : public AbortController {
    WTF_MAKE_TZONE_ALLOCATED(TaskController);
public:
    using Init = TaskControllerInit;

    static Ref<TaskController> create(ScriptExecutionContext&, const TaskControllerInit& = { });
    ~TaskController();

    void setPriority(TaskPriority);

    bool isTaskController() const final { return true; }
    Ref<TaskSignal> taskSignal();

private:
    TaskController(Ref<TaskSignal>&&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TaskController)
    static bool isType(const WebCore::AbortController& controller) { return controller.isTaskController(); }
SPECIALIZE_TYPE_TRAITS_END()
