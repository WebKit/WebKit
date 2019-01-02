/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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

#include "config.h"
#include "WorkerGlobalScope.h"

#include "ContentSecurityPolicy.h"
#include "Crypto.h"
#include "IDBConnectionProxy.h"
#include "ImageBitmapOptions.h"
#include "InspectorInstrumentation.h"
#include "Microtasks.h"
#include "Performance.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "ServiceWorkerGlobalScope.h"
#include "SocketProvider.h"
#include "WorkerInspectorController.h"
#include "WorkerLoaderProxy.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerReportingProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>

namespace WebCore {
using namespace Inspector;

WorkerGlobalScope::WorkerGlobalScope(const URL& url, Ref<SecurityOrigin>&& origin, const String& identifier, const String& userAgent, bool isOnline, WorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
    : m_url(url)
    , m_identifier(identifier)
    , m_userAgent(userAgent)
    , m_thread(thread)
    , m_script(std::make_unique<WorkerScriptController>(this))
    , m_inspectorController(std::make_unique<WorkerInspectorController>(*this))
    , m_microtaskQueue(std::make_unique<MicrotaskQueue>())
    , m_isOnline(isOnline)
    , m_shouldBypassMainWorldContentSecurityPolicy(shouldBypassMainWorldContentSecurityPolicy)
    , m_eventQueue(*this)
    , m_topOrigin(WTFMove(topOrigin))
#if ENABLE(INDEXED_DATABASE)
    , m_connectionProxy(connectionProxy)
#endif
    , m_socketProvider(socketProvider)
    , m_performance(Performance::create(this, timeOrigin))
    , m_sessionID(sessionID)
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif

    if (m_topOrigin->hasUniversalAccess())
        origin->grantUniversalAccess();
    if (m_topOrigin->needsStorageAccessFromFileURLsQuirk())
        origin->grantStorageAccessFromFileURLsQuirk();

    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(origin)));
    setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(URL { m_url }, *this));
}

WorkerGlobalScope::~WorkerGlobalScope()
{
    ASSERT(thread().thread() == &Thread::current());
    // We need to remove from the contexts map very early in the destructor so that calling postTask() on this WorkerGlobalScope from another thread is safe.
    removeFromContextsMap();

    m_performance = nullptr;
    m_crypto = nullptr;

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread().workerReportingProxy().workerGlobalScopeDestroyed();
}

String WorkerGlobalScope::origin() const
{
    auto* securityOrigin = this->securityOrigin();
    return securityOrigin ? securityOrigin->toString() : emptyString();
}

void WorkerGlobalScope::prepareForTermination()
{
#if ENABLE(INDEXED_DATABASE)
    stopIndexedDatabase();
#endif

    stopActiveDOMObjects();

    m_inspectorController->workerTerminating();

    // Event listeners would keep DOMWrapperWorld objects alive for too long. Also, they have references to JS objects,
    // which become dangling once Heap is destroyed.
    removeAllEventListeners();

    // MicrotaskQueue and RejectedPromiseTracker reference Heap.
    m_microtaskQueue = nullptr;
    removeRejectedPromiseTracker();
}

void WorkerGlobalScope::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();
    m_performance->removeAllEventListeners();
    m_performance->removeAllObservers();
}

bool WorkerGlobalScope::isSecureContext() const
{
    return securityOrigin() && securityOrigin()->isPotentiallyTrustworthy();
}

void WorkerGlobalScope::applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders)
{
    contentSecurityPolicy()->didReceiveHeaders(contentSecurityPolicyResponseHeaders, String { });
}

URL WorkerGlobalScope::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the URL constructor to have this behavior?
    if (url.isNull())
        return URL();
    // Always use UTF-8 in Workers.
    return URL(m_url, url);
}

String WorkerGlobalScope::userAgent(const URL&) const
{
    return m_userAgent;
}

void WorkerGlobalScope::disableEval(const String& errorMessage)
{
    m_script->disableEval(errorMessage);
}

void WorkerGlobalScope::disableWebAssembly(const String& errorMessage)
{
    m_script->disableWebAssembly(errorMessage);
}

SocketProvider* WorkerGlobalScope::socketProvider()
{
    return m_socketProvider.get();
}

#if ENABLE(INDEXED_DATABASE)

IDBClient::IDBConnectionProxy* WorkerGlobalScope::idbConnectionProxy()
{
#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    return m_connectionProxy.get();
#else
    return nullptr;
#endif
}

void WorkerGlobalScope::stopIndexedDatabase()
{
#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    if (m_connectionProxy)
        m_connectionProxy->forgetActivityForCurrentThread();
#endif
}

#endif // ENABLE(INDEXED_DATABASE)

WorkerLocation& WorkerGlobalScope::location() const
{
    if (!m_location)
        m_location = WorkerLocation::create(m_url);
    return *m_location;
}

void WorkerGlobalScope::close()
{
    if (m_closing)
        return;

    // Let current script run to completion but prevent future script evaluations.
    // After m_closing is set, all the tasks in the queue continue to be fetched but only
    // tasks with isCleanupTask()==true will be executed.
    m_closing = true;
    postTask({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context) {
        ASSERT_WITH_SECURITY_IMPLICATION(is<WorkerGlobalScope>(context));
        WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);
        // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
        workerGlobalScope.thread().workerReportingProxy().workerGlobalScopeClosed();
    } });
}

WorkerNavigator& WorkerGlobalScope::navigator()
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(*this, m_userAgent, m_isOnline);
    return *m_navigator;
}

void WorkerGlobalScope::setIsOnline(bool isOnline)
{
    m_isOnline = isOnline;
    if (m_navigator)
        m_navigator->setIsOnline(isOnline);
}

void WorkerGlobalScope::postTask(Task&& task)
{
    thread().runLoop().postTask(WTFMove(task));
}

ExceptionOr<int> WorkerGlobalScope::setTimeout(JSC::ExecState& state, std::unique_ptr<ScheduledAction> action, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!contentSecurityPolicy()->allowEval(&state))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*this, WTFMove(action), Seconds::fromMilliseconds(timeout), true);
}

void WorkerGlobalScope::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(*this, timeoutId);
}

ExceptionOr<int> WorkerGlobalScope::setInterval(JSC::ExecState& state, std::unique_ptr<ScheduledAction> action, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!contentSecurityPolicy()->allowEval(&state))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*this, WTFMove(action), Seconds::fromMilliseconds(timeout), false);
}

void WorkerGlobalScope::clearInterval(int timeoutId)
{
    DOMTimer::removeById(*this, timeoutId);
}

ExceptionOr<void> WorkerGlobalScope::importScripts(const Vector<String>& urls)
{
    ASSERT(contentSecurityPolicy());

    Vector<URL> completedURLs;
    completedURLs.reserveInitialCapacity(urls.size());
    for (auto& entry : urls) {
        URL url = completeURL(entry);
        if (!url.isValid())
            return Exception { SyntaxError };
        completedURLs.uncheckedAppend(WTFMove(url));
    }

    FetchOptions::Cache cachePolicy = FetchOptions::Cache::Default;

#if ENABLE(SERVICE_WORKER)
    bool isServiceWorkerGlobalScope = is<ServiceWorkerGlobalScope>(*this);
    if (isServiceWorkerGlobalScope) {
        // FIXME: We need to add support for the 'imported scripts updated' flag as per:
        // https://w3c.github.io/ServiceWorker/#importscripts
        auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*this);
        auto& registration = serviceWorkerGlobalScope.registration();
        if (registration.updateViaCache() == ServiceWorkerUpdateViaCache::None || registration.needsUpdate())
            cachePolicy = FetchOptions::Cache::NoCache;
    }
#endif

    for (auto& url : completedURLs) {
        // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
        bool shouldBypassMainWorldContentSecurityPolicy = this->shouldBypassMainWorldContentSecurityPolicy();
        if (!shouldBypassMainWorldContentSecurityPolicy && !contentSecurityPolicy()->allowScriptFromSource(url))
            return Exception { NetworkError };

        auto scriptLoader = WorkerScriptLoader::create();
        auto cspEnforcement = shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective;
        if (auto exception = scriptLoader->loadSynchronously(this, url, FetchOptions::Mode::NoCors, cachePolicy, cspEnforcement, resourceRequestIdentifier()))
            return WTFMove(*exception);

        InspectorInstrumentation::scriptImported(*this, scriptLoader->identifier(), scriptLoader->script());

        NakedPtr<JSC::Exception> exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader->script(), URL(scriptLoader->responseURL())), exception);
        if (exception) {
            m_script->setException(exception);
            return { };
        }
    }

    return { };
}

EventTarget* WorkerGlobalScope::errorEventTarget()
{
    return this;
}

void WorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<ScriptCallStack>&&)
{
    thread().workerReportingProxy().postExceptionToWorkerObject(errorMessage, lineNumber, columnNumber, sourceURL);
}

void WorkerGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& message)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(message->source(), message->level(), message->message()));
        return;
    }

    InspectorInstrumentation::addMessageToConsole(*this, WTFMove(message));
}

void WorkerGlobalScope::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    addMessage(source, level, message, { }, 0, 0, nullptr, nullptr, requestIdentifier);
}

void WorkerGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, messageText));
        return;
    }

    std::unique_ptr<Inspector::ConsoleMessage> message;
    if (callStack)
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, callStack.releaseNonNull(), requestIdentifier);
    else
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, sourceURL, lineNumber, columnNumber, state, requestIdentifier);
    InspectorInstrumentation::addMessageToConsole(*this, WTFMove(message));
}

bool WorkerGlobalScope::isContextThread() const
{
    return thread().thread() == &Thread::current();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

WorkerEventQueue& WorkerGlobalScope::eventQueue() const
{
    return m_eventQueue;
}

#if ENABLE(WEB_CRYPTO)

bool WorkerGlobalScope::wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey)
{
    bool result = false;
    bool done = false;
    m_thread.workerLoaderProxy().postTaskToLoader([&result, &key, &wrappedKey, &done, workerGlobalScope = this](ScriptExecutionContext& context) {
        result = context.wrapCryptoKey(key, wrappedKey);
        done = true;
        workerGlobalScope->postTask([](ScriptExecutionContext& context) {
            ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        });
    });

    auto waitResult = MessageQueueMessageReceived;
    while (!done && waitResult != MessageQueueTerminated)
        waitResult = m_thread.runLoop().runInMode(this, WorkerRunLoop::defaultMode());

    return result;
}

bool WorkerGlobalScope::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key)
{
    bool result = false, done = false;
    m_thread.workerLoaderProxy().postTaskToLoader([&result, &wrappedKey, &key, &done, workerGlobalScope = this](ScriptExecutionContext& context) {
        result = context.unwrapCryptoKey(wrappedKey, key);
        done = true;
        workerGlobalScope->postTask([](ScriptExecutionContext& context) {
            ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        });
    });

    auto waitResult = MessageQueueMessageReceived;
    while (!done && waitResult != MessageQueueTerminated)
        waitResult = m_thread.runLoop().runInMode(this, WorkerRunLoop::defaultMode());

    return result;
}

#endif // ENABLE(WEB_CRYPTO)

Crypto& WorkerGlobalScope::crypto()
{
    if (!m_crypto)
        m_crypto = Crypto::create(this);
    return *m_crypto;
}

Performance& WorkerGlobalScope::performance() const
{
    return *m_performance;
}

WorkerCacheStorageConnection& WorkerGlobalScope::cacheStorageConnection()
{
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = WorkerCacheStorageConnection::create(*this);
    return *m_cacheStorageConnection;
}

void WorkerGlobalScope::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    ImageBitmap::createPromise(*this, WTFMove(source), WTFMove(options), WTFMove(promise));
}

void WorkerGlobalScope::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    ImageBitmap::createPromise(*this, WTFMove(source), WTFMove(options), sx, sy, sw, sh, WTFMove(promise));
}

} // namespace WebCore
