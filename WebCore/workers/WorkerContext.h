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
#include "EventTarget.h"
#include "ScriptExecutionContext.h"
#include "WorkerScriptController.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class ScheduledAction;
    class WorkerLocation;
    class WorkerNavigator;
    class WorkerThread;

    class WorkerContext : public RefCounted<WorkerContext>, public ScriptExecutionContext, public EventTarget {
    public:
        static PassRefPtr<WorkerContext> create(const KURL& url, const String& userAgent, WorkerThread* thread)
        {
            return adoptRef(new WorkerContext(url, userAgent, thread));
        }

        virtual ~WorkerContext();

        virtual bool isWorkerContext() const { return true; }

        virtual WorkerContext* toWorkerContext() { return this; }

        virtual ScriptExecutionContext* scriptExecutionContext() const;

        const KURL& url() const { return m_url; }
        KURL completeURL(const String&) const;

        virtual String userAgent(const KURL&) const;

        WorkerScriptController* script() { return m_script.get(); }
        void clearScript() { return m_script.clear(); }

        WorkerThread* thread() { return m_thread; }

        bool hasPendingActivity() const;

        virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);
        virtual void addMessage(MessageDestination, MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);
        virtual void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
        virtual void scriptImported(unsigned long identifier, const String& sourceString);

        virtual void postTask(PassRefPtr<Task>); // Executes the task on context's thread asynchronously.

        // WorkerGlobalScope
        WorkerContext* self() { return this; }
        WorkerLocation* location() const;
        void close();

        // WorkerUtils
        void importScripts(const Vector<String>& urls, const String& callerURL, int callerLine, ExceptionCode&);
        WorkerNavigator* navigator() const;

        // DedicatedWorkerGlobalScope
        void postMessage(const String&, ExceptionCode&);
        void postMessage(const String&, MessagePort*, ExceptionCode&);
        void setOnmessage(PassRefPtr<EventListener> eventListener) { m_onmessageListener = eventListener; }
        EventListener* onmessage() const { return m_onmessageListener.get(); }

        // Timers
        int setTimeout(ScheduledAction*, int timeout);
        void clearTimeout(int timeoutId);
        int setInterval(ScheduledAction*, int timeout);
        void clearInterval(int timeoutId);

        // EventTarget
        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

        typedef Vector<RefPtr<EventListener> > ListenerVector;
        typedef HashMap<AtomicString, ListenerVector> EventListenersMap;
        EventListenersMap& eventListeners() { return m_eventListeners; }

        void dispatchMessage(const String&, PassRefPtr<MessagePort>);

        // These methods are used for GC marking. See JSWorkerContext::mark() in
        // JSWorkerContextCustom.cpp.
        WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
        WorkerLocation* optionalLocation() const { return m_location.get(); }

        using RefCounted<WorkerContext>::ref;
        using RefCounted<WorkerContext>::deref;

    private:
        WorkerContext(const KURL&, const String&, WorkerThread*);

        virtual void refScriptExecutionContext() { ref(); }
        virtual void derefScriptExecutionContext() { deref(); }
        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        virtual const KURL& virtualURL() const;
        virtual KURL virtualCompleteURL(const String&) const;

        KURL m_url;
        String m_userAgent;

        mutable RefPtr<WorkerLocation> m_location;
        mutable RefPtr<WorkerNavigator> m_navigator;

        OwnPtr<WorkerScriptController> m_script;
        WorkerThread* m_thread;

        RefPtr<EventListener> m_onmessageListener;
        EventListenersMap m_eventListeners;

        bool m_closing;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerContext_h
