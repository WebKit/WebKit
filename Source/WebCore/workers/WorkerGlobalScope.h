/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include "CacheStorageConnection.h"
#include "EventTarget.h"
#include "ImageBitmap.h"
#include "ScriptExecutionContext.h"
#include "Supplementable.h"
#include "URL.h"
#include "WorkerCacheStorageConnection.h"
#include "WorkerEventQueue.h"
#include "WorkerScriptController.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <memory>
#include <pal/SessionID.h>

namespace WebCore {

class ContentSecurityPolicyResponseHeaders;
class Crypto;
class MicrotaskQueue;
class Performance;
class ScheduledAction;
class WorkerInspectorController;
class WorkerLocation;
class WorkerNavigator;
class WorkerThread;

namespace IDBClient {
class IDBConnectionProxy;
}

class WorkerGlobalScope : public RefCounted<WorkerGlobalScope>, public Supplementable<WorkerGlobalScope>, public ScriptExecutionContext, public EventTargetWithInlineData, public Base64Utilities {
public:
    virtual ~WorkerGlobalScope();

    virtual bool isDedicatedWorkerGlobalScope() const { return false; }
    virtual bool isServiceWorkerGlobalScope() const { return false; }

    const URL& url() const final { return m_url; }
    String origin() const final;
    const String& identifier() const { return m_identifier; }

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
#endif

    WorkerCacheStorageConnection& cacheStorageConnection();

    WorkerScriptController* script() { return m_script.get(); }
    void clearScript() { m_script = nullptr; }

    WorkerInspectorController& inspectorController() const { return *m_inspectorController; }

    MicrotaskQueue& microtaskQueue() const { return *m_microtaskQueue; }

    WorkerThread& thread() const { return m_thread; }

    using ScriptExecutionContext::hasPendingActivity;

    void postTask(Task&&) final; // Executes the task on context's thread asynchronously.

    WorkerGlobalScope& self() { return *this; }
    WorkerLocation& location() const;
    void close();

    virtual ExceptionOr<void> importScripts(const Vector<String>& urls);
    WorkerNavigator& navigator();

    void setIsOnline(bool);

    ExceptionOr<int> setTimeout(JSC::ExecState&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearTimeout(int timeoutId);
    ExceptionOr<int> setInterval(JSC::ExecState&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearInterval(int timeoutId);

    bool isContextThread() const final;
    bool isSecureContext() const final;

    WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
    WorkerLocation* optionalLocation() const { return m_location.get(); }

    using RefCounted::ref;
    using RefCounted::deref;

    bool isClosing() { return m_closing; }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    Crypto& crypto();
    Performance& performance() const;

    void prepareForTermination();

    void removeAllEventListeners() final;

    void createImageBitmap(ImageBitmap::Source&&, ImageBitmapOptions&&, ImageBitmap::Promise&&);
    void createImageBitmap(ImageBitmap::Source&&, int sx, int sy, int sw, int sh, ImageBitmapOptions&&, ImageBitmap::Promise&&);

    unsigned long createUniqueIdentifier() { return m_uniqueIdentifier++; }

protected:
    WorkerGlobalScope(const URL&, Ref<SecurityOrigin>&&, const String& identifier, const String& userAgent, bool isOnline, WorkerThread&, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, PAL::SessionID);

    void applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders&);

private:
    void refScriptExecutionContext() final { ref(); }
    void derefScriptExecutionContext() final { deref(); }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    // The following addMessage and addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::ExecState*, unsigned long requestIdentifier) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) final;

    bool isWorkerGlobalScope() const final { return true; }

    ScriptExecutionContext* scriptExecutionContext() const final { return const_cast<WorkerGlobalScope*>(this); }
    URL completeURL(const String&) const final;
    PAL::SessionID sessionID() const final { return m_sessionID; }
    String userAgent(const URL&) const final;
    void disableEval(const String& errorMessage) final;
    void disableWebAssembly(const String& errorMessage) final;
    EventTarget* errorEventTarget() final;
    WorkerEventQueue& eventQueue() const final;
    String resourceRequestIdentifier() const final { return m_identifier; }
    SocketProvider* socketProvider() final;

    bool shouldBypassMainWorldContentSecurityPolicy() const final { return m_shouldBypassMainWorldContentSecurityPolicy; }
    bool isJSExecutionForbidden() const final;
    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

#if ENABLE(WEB_CRYPTO)
    // The following two functions are side effects of providing extra protection to serialized
    // CryptoKey data that went through the structured clone algorithm to local storage such as
    // IndexedDB. They don't provide any proctection against communications between mainThread
    // and workerThreads. In fact, they cause extra expense as workerThreads cannot talk to clients
    // to unwrap/wrap crypto keys. Hence, workerThreads must always ask mainThread to unwrap/wrap
    // keys, which results in a second communication and plain keys being transferred between
    // workerThreads and the mainThread.
    bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) final;
    bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) final;
#endif

#if ENABLE(INDEXED_DATABASE)
    void stopIndexedDatabase();
#endif

    URL m_url;
    String m_identifier;
    String m_userAgent;

    mutable RefPtr<WorkerLocation> m_location;
    mutable RefPtr<WorkerNavigator> m_navigator;

    WorkerThread& m_thread;
    std::unique_ptr<WorkerScriptController> m_script;
    std::unique_ptr<WorkerInspectorController> m_inspectorController;
    std::unique_ptr<MicrotaskQueue> m_microtaskQueue;

    bool m_closing { false };
    bool m_isOnline;
    bool m_shouldBypassMainWorldContentSecurityPolicy;

    mutable WorkerEventQueue m_eventQueue;

    Ref<SecurityOrigin> m_topOrigin;

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionProxy> m_connectionProxy;
#endif

    RefPtr<SocketProvider> m_socketProvider;

    RefPtr<Performance> m_performance;
    mutable RefPtr<Crypto> m_crypto;

    PAL::SessionID m_sessionID;
    RefPtr<WorkerCacheStorageConnection> m_cacheStorageConnection;

    unsigned long m_uniqueIdentifier { 1 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkerGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
