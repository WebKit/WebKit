/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include "AbortSignal.h"
#include "EventTarget.h"
#include "TaskPriority.h"

namespace WebCore {

class TaskSignal final : public AbortSignal {
    WTF_MAKE_TZONE_ALLOCATED(TaskSignal);
public:
    static Ref<TaskSignal> create(ScriptExecutionContext*, TaskPriority);
    ~TaskSignal();

    TaskPriority priority() const { return m_priority; }

    void setPriorityInternal(TaskPriority);
    void dispatchPriorityChangeEvent(TaskPriority previousPriority);

    using AbortSignal::addAlgorithm;

private:
    TaskSignal(ScriptExecutionContext*, TaskPriority);

    TaskPriority m_priority;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TaskSignal)
    static bool isType(const WebCore::AbortSignal& s) { return s.eventTargetTag() == WebCore::AbortSignal::EventTargetTagged::TaskSignal; }
    static bool isType(const WebCore::EventTarget& t) { return t.eventTargetInterface() == WebCore::EventTargetInterfaceType::TaskSignal; }
SPECIALIZE_TYPE_TRAITS_END()
