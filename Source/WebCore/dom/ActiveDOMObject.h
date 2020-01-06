/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#pragma once

#include "ContextDestructionObserver.h"
#include "TaskSource.h"
#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class Document;
class Event;
class EventLoopTaskGroup;
class EventTarget;

enum class ReasonForSuspension {
    JavaScriptDebuggerPaused,
    WillDeferLoading,
    BackForwardCache,
    PageWillBeSuspended,
};

class WEBCORE_EXPORT ActiveDOMObject : public ContextDestructionObserver {
public:
    // The suspendIfNeeded must be called exactly once after object construction to update
    // the suspended state to match that of the ScriptExecutionContext.
    void suspendIfNeeded();
    void assertSuspendIfNeededWasCalled() const;

    virtual bool hasPendingActivity() const;

    // However, the suspend function will sometimes be called even if canSuspendForDocumentSuspension() returns false.
    // That happens in step-by-step JS debugging for example - in this case it would be incorrect
    // to stop the object. Exact semantics of suspend is up to the object in cases like that.

    virtual const char* activeDOMObjectName() const = 0;

    // These functions must not have a side effect of creating or destroying
    // any ActiveDOMObject. That means they must not result in calls to arbitrary JavaScript.
    virtual void suspend(ReasonForSuspension);
    virtual void resume();

    // This function must not have a side effect of creating an ActiveDOMObject.
    // That means it must not result in calls to arbitrary JavaScript.
    // It can, however, have a side effect of deleting an ActiveDOMObject.
    virtual void stop();

    template<typename T> void setPendingActivity(T& thisObject)
    {
        ASSERT(&thisObject == this);
        thisObject.ref();
        ++m_pendingActivityCount;
    }

    template<typename T> void unsetPendingActivity(T& thisObject)
    {
        ASSERT(m_pendingActivityCount > 0);
        --m_pendingActivityCount;
        thisObject.deref();
    }

    template<class T>
    class PendingActivity : public RefCounted<PendingActivity<T>> {
    public:
        explicit PendingActivity(T& thisObject)
            : m_thisObject(thisObject)
        {
            ++(m_thisObject->m_pendingActivityCount);
        }

        ~PendingActivity()
        {
            ASSERT(m_thisObject->m_pendingActivityCount > 0);
            --(m_thisObject->m_pendingActivityCount);
        }

    private:
        Ref<T> m_thisObject;
    };

    template<class T> Ref<PendingActivity<T>> makePendingActivity(T& thisObject)
    {
        ASSERT(&thisObject == this);
        return adoptRef(*new PendingActivity<T>(thisObject));
    }

    bool isContextStopped() const;
    bool isAllowedToRunScript() const;

    template<typename T>
    static void queueTaskKeepingObjectAlive(T& object, TaskSource source, Function<void ()>&& task)
    {
        object.queueTaskInEventLoop(source, [protectedObject = makeRef(object), activity = object.ActiveDOMObject::makePendingActivity(object), task = WTFMove(task)] () {
            task();
        });
    }

    template<typename EventTargetType, typename EventType>
    static void queueTaskToDispatchEvent(EventTargetType& target, TaskSource source, Ref<EventType>&& event)
    {
        target.queueTaskToDispatchEventInternal(target, source, WTFMove(event));
    }

protected:
    explicit ActiveDOMObject(ScriptExecutionContext*);
    explicit ActiveDOMObject(Document*);
    explicit ActiveDOMObject(Document&);
    virtual ~ActiveDOMObject();

private:
    enum CheckedScriptExecutionContextType { CheckedScriptExecutionContext };
    ActiveDOMObject(ScriptExecutionContext*, CheckedScriptExecutionContextType);

    void queueTaskInEventLoop(TaskSource, Function<void ()>&&);
    void queueTaskToDispatchEventInternal(EventTarget&, TaskSource, Ref<Event>&&);

    unsigned m_pendingActivityCount { 0 };
#if ASSERT_ENABLED
    bool m_suspendIfNeededWasCalled { false };
    Ref<Thread> m_creationThread { Thread::current() };
#endif

    friend class ActiveDOMObjectEventDispatchTask;
};

#if !ASSERT_ENABLED

inline void ActiveDOMObject::assertSuspendIfNeededWasCalled() const
{
}

#endif

} // namespace WebCore
