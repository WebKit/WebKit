/*
 * Copyright (C) 2014 Yoav Weiss (yoav@yoav.ws)
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "EventLoop.h"
#include <JavaScriptCore/MicrotaskQueue.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class VM;
} // namespace JSC

namespace WebCore {

class WebCoreMicrotaskDispatcher : public JSC::MicrotaskDispatcher {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(WebCoreMicrotaskDispatcher);
public:
    WebCoreMicrotaskDispatcher(Type type, EventLoopTaskGroup& group)
        : JSC::MicrotaskDispatcher(type)
        , m_group(group)
    {
    }

    bool isRunnable() const final
    {
        return currentRunnability() == JSC::QueuedTask::Result::Executed;
    }

    JSC::QueuedTask::Result NODELETE currentRunnability() const;

private:
    WeakPtr<EventLoopTaskGroup> m_group;
};

class MicrotaskQueue final : public JSC::MicrotaskQueue {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(MicrotaskQueue, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<MicrotaskQueue> create(JSC::VM&, EventLoop&);
    WEBCORE_EXPORT ~MicrotaskQueue();

    WEBCORE_EXPORT void performMicrotaskCheckpoint(JSC::VM&);

    WEBCORE_EXPORT void addCheckpointTask(std::unique_ptr<EventLoopTask>&&);

    bool isPerformingCheckpoint() const { return m_performingMicrotaskCheckpoint; }

private:
    WEBCORE_EXPORT MicrotaskQueue(JSC::VM&, EventLoop&);

    void scheduleToRunIfNeeded() override;

    bool m_performingMicrotaskCheckpoint { false };
    WeakPtr<EventLoop> m_eventLoop;

    EventLoop::TaskVector m_checkpointTasks;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebCoreMicrotaskDispatcher)
    static bool isType(const JSC::MicrotaskDispatcher& dispatcher) { return dispatcher.isWebCoreMicrotaskDispatcher(); }
SPECIALIZE_TYPE_TRAITS_END()
