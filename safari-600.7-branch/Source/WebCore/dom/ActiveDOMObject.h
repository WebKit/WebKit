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

#ifndef ActiveDOMObject_h
#define ActiveDOMObject_h

#include "ContextDestructionObserver.h"
#include <wtf/Assertions.h>
#include <wtf/Forward.h>

namespace WebCore {

class ActiveDOMObject : public ContextDestructionObserver {
public:
    explicit ActiveDOMObject(ScriptExecutionContext*);

    // The suspendIfNeeded must be called exactly once after object construction to update
    // the suspended state to match that of the ScriptExecutionContext.
    void suspendIfNeeded();
    void assertSuspendIfNeededWasCalled() const;

    virtual bool hasPendingActivity() const;

    // The canSuspend function is used by the caller if there is a choice between suspending
    // and stopping. For example, a page won't be suspended and placed in the back/forward
    // cache if it contains any objects that cannot be suspended.

    // However, the suspend function will sometimes be called even if canSuspend returns false.
    // That happens in step-by-step JS debugging for example - in this case it would be incorrect
    // to stop the object. Exact semantics of suspend is up to the object in cases like that.

    enum ReasonForSuspension {
        JavaScriptDebuggerPaused,
        WillDeferLoading,
        DocumentWillBecomeInactive,
        PageWillBeSuspended,
        DocumentWillBePaused
    };

    // These three functions must not have a side effect of creating or destroying
    // any ActiveDOMObject. That means they must not result in calls to arbitrary JavaScript.
    virtual bool canSuspend() const;
    virtual void suspend(ReasonForSuspension);
    virtual void resume();

    // This function must not have a side effect of creating an ActiveDOMObject.
    // That means it must not result in calls to arbitrary JavaScript.
    // It can, however, have a side effect of deleting an ActiveDOMObject.
    virtual void stop();

    template<class T> void setPendingActivity(T* thisObject)
    {
        ASSERT(thisObject == this);
        thisObject->ref();
        ++m_pendingActivityCount;
    }

    template<class T> void unsetPendingActivity(T* thisObject)
    {
        ASSERT(m_pendingActivityCount > 0);
        --m_pendingActivityCount;
        thisObject->deref();
    }

protected:
    virtual ~ActiveDOMObject();

private:
    unsigned m_pendingActivityCount;
#if !ASSERT_DISABLED
    bool m_suspendIfNeededWasCalled;
#endif
};

#if ASSERT_DISABLED

inline void ActiveDOMObject::assertSuspendIfNeededWasCalled() const
{
}

#endif

} // namespace WebCore

#endif // ActiveDOMObject_h
