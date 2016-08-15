/*
 * Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016 Apple Inc. All rights reserved.
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

#include "Base64Utilities.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "ScriptExecutionContext.h"
#include "URL.h"
#include "WorkerEventQueue.h"
#include "WorkerScriptController.h"
#include <memory>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace Inspector {
class ConsoleMessage;
}

namespace WebCore {

class Blob;
class ContentSecurityPolicyResponseHeaders;
class Crypto;
class ScheduledAction;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

namespace IDBClient {
class IDBConnectionProxy;
}

class WorkerGlobalScope : public RefCounted<WorkerGlobalScope>, public Supplementable<WorkerGlobalScope>, public ScriptExecutionContext, public EventTargetWithInlineData, public Base64Utilities {
public:
    virtual ~WorkerGlobalScope();

    bool isWorkerGlobalScope() const override { return true; }

    ScriptExecutionContext* scriptExecutionContext() const final { return const_cast<WorkerGlobalScope*>(this); }

    virtual bool isDedicatedWorkerGlobalScope() const { return false; }

    const URL& url() const final { return m_url; }
    URL completeURL(const String&) const final;

    String userAgent(const URL&) const override;

    void disableEval(const String& errorMessage) override;

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
    void stopIndexedDatabase();
#endif

#if ENABLE(WEB_SOCKETS)
    SocketProvider* socketProvider() final;
#endif

    bool shouldBypassMainWorldContentSecurityPolicy() const final { return m_shouldBypassMainWorldContentSecurityPolicy; }

    WorkerScriptController* script() { return m_script.get(); }
    void clearScript() { m_script = nullptr; }

    WorkerThread& thread() const { return m_thread; }

    using ScriptExecutionContext::hasPendingActivity;

    void postTask(Task&&) final; // Executes the task on context's thread asynchronously.

    // WorkerGlobalScope
    WorkerGlobalScope& self() { return *this; }
    WorkerLocation& location() const;
    void close();

    // WorkerUtils
    virtual void importScripts(const Vector<String>& urls, ExceptionCode&);
    WorkerNavigator& navigator() const;

    // Timers
    int setTimeout(std::unique_ptr<ScheduledAction>, int timeout);
    void clearTimeout(int timeoutId);
    int setInterval(std::unique_ptr<ScheduledAction>, int timeout);
    void clearInterval(int timeoutId);

    bool isContextThread() const override;
    bool isJSExecutionForbidden() const override;

    // These methods are used for GC marking. See JSWorkerGlobalScope::visitChildrenVirtual(SlotVisitor&) in
    // JSWorkerGlobalScopeCustom.cpp.
    WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
    WorkerLocation* optionalLocation() const { return m_location.get(); }

    using RefCounted<WorkerGlobalScope>::ref;
    using RefCounted<WorkerGlobalScope>::deref;

    bool isClosing() { return m_closing; }

    // An observer interface to be notified when the worker thread is getting stopped.
    class Observer {
        WTF_MAKE_NONCOPYABLE(Observer);
    public:
        Observer(WorkerGlobalScope*);
        virtual ~Observer();
        virtual void notifyStop() = 0;
        void stopObserving();
    private:
        WorkerGlobalScope* m_context;
    };
    friend class Observer;
    void registerObserver(Observer*);
    void unregisterObserver(Observer*);
    void notifyObserversOfStop();

    SecurityOrigin* topOrigin() const override { return m_topOrigin.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>);
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0) override;

#if ENABLE(SUBTLE_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) override;
    bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) override;
#endif

    Crypto& crypto() const;

protected:
    WorkerGlobalScope(const URL&, const String& userAgent, WorkerThread&, bool shouldBypassMainWorldContentSecurityPolicy, RefPtr<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*);
    void applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders&);

    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) override;
    void addMessageToWorkerConsole(std::unique_ptr<Inspector::ConsoleMessage>);
    void addMessageToWorkerConsole(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::ExecState* = 0, unsigned long requestIdentifier = 0);

private:
    void refScriptExecutionContext() override { ref(); }
    void derefScriptExecutionContext() override { deref(); }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::ExecState* = 0, unsigned long requestIdentifier = 0) override;

    EventTarget* errorEventTarget() override;

    WorkerEventQueue& eventQueue() const final;

    URL m_url;
    String m_userAgent;

    mutable RefPtr<WorkerLocation> m_location;
    mutable RefPtr<WorkerNavigator> m_navigator;

    std::unique_ptr<WorkerScriptController> m_script;
    WorkerThread& m_thread;

    bool m_closing;
    bool m_shouldBypassMainWorldContentSecurityPolicy;

    HashSet<Observer*> m_workerObservers;

    mutable WorkerEventQueue m_eventQueue;

    RefPtr<SecurityOrigin> m_topOrigin;

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionProxy> m_connectionProxy;
#endif
#if ENABLE(WEB_SOCKETS)
    RefPtr<SocketProvider> m_socketProvider;
#endif

    mutable RefPtr<Crypto> m_crypto;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkerGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
