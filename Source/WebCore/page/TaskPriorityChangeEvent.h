/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 */

#pragma once

#include "Event.h"
#include "TaskPriority.h"

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class TaskPriorityChangeEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(TaskPriorityChangeEvent);
public:
    struct Init : EventInit {
        TaskPriority previousPriority { TaskPriority::UserVisible };
    };

    static Ref<TaskPriorityChangeEvent> create(const AtomString& type, const Init& init, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new TaskPriorityChangeEvent(type, init, isTrusted));
    }



    TaskPriority previousPriority() const { return m_previousPriority; }

private:
    TaskPriorityChangeEvent(const AtomString& type, const Init& init, IsTrusted isTrusted)
        : Event(EventInterfaceType::TaskPriorityChangeEvent, type, init, isTrusted)
        , m_previousPriority(init.previousPriority)
    {
    }

    TaskPriority m_previousPriority;
};

} // namespace WebCore
