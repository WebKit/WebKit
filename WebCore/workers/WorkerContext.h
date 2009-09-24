/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef WorkerContext_h
#define WorkerContext_h

#if ENABLE(WORKERS)

#include "AtomicStringHash.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "ScriptExecutionContext.h"
#include "WorkerScriptController.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class NotificationCenter;
    class ScheduledAction;
    class WorkerLocation;
    class WorkerNavigator;
    class WorkerThread;

    class WorkerContext : public RefCounted<WorkerContext>, public ScriptExecutionContext, public EventTarget {
    public:
        virtual ~WorkerContext();

        virtual bool isWorkerContext() const { return true; }

        virtual ScriptExecutionContext* scriptExecutionContext() const;

        virtual bool isSharedWorkerContext() const { return false; }
        virtual bool isDedicatedWorkerContext() const { return false; }

        const KURL& url() const { return m_url; }
        KURL completeURL(const String&) const;

        virtual String userAgent(const KURL&) const;

        WorkerScriptController* script() { return m_script.get(); }
        void clearScript() { return m_script.clear(); }

        WorkerThread* thread() { return m_thread; }

        bool hasPendingActivity() const;

        virtual void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
        virtual void scriptImported(unsigned long identifier, const String& sourceString);

        virtual void postTask(PassRefPtr<Task>); // Executes the task on context's thread asynchronously.

        // WorkerGlobalScope
        WorkerContext* self() { return this; }
        WorkerLocation* location() const;
        void close();

        DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

        // WorkerUtils
        virtual void importScripts(const Vector<String>& urls, const String& callerURL, int callerLine, ExceptionCode&);
        WorkerNavigator* navigator() const;

        // Timers
        int setTimeout(ScheduledAction*, int timeout);
        void clearTimeout(int timeoutId);
        int setInterval(ScheduledAction*, int timeout);
        void clearInterval(int timeoutId);

        // ScriptExecutionContext
        virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);
        virtual void addMessage(MessageDestination, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);

#if ENABLE(NOTIFICATIONS)
        NotificationCenter* webkitNotifications() const;
#endif

        // These methods are used for GC marking. See JSWorkerContext::markChildren(MarkStack&) in
        // JSWorkerContextCustom.cpp.
        WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
        WorkerLocation* optionalLocation() const { return m_location.get(); }

        using RefCounted<WorkerContext>::ref;
        using RefCounted<WorkerContext>::deref;

        bool isClosing() { return m_closing; }

    protected:
        WorkerContext(const KURL&, const String&, WorkerThread*);

    private:
        virtual void refScriptExecutionContext() { ref(); }
        virtual void derefScriptExecutionContext() { deref(); }

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }
        virtual EventTargetData* eventTargetData();
        virtual EventTargetData* ensureEventTargetData();

        virtual const KURL& virtualURL() const;
        virtual KURL virtualCompleteURL(const String&) const;

        KURL m_url;
        String m_userAgent;

        mutable RefPtr<WorkerLocation> m_location;
        mutable RefPtr<WorkerNavigator> m_navigator;

        OwnPtr<WorkerScriptController> m_script;
        WorkerThread* m_thread;

#if ENABLE_NOTIFICATIONS
        mutable RefPtr<NotificationCenter> m_notifications;
#endif
        bool m_closing;
        EventTargetData m_eventTargetData;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerContext_h
