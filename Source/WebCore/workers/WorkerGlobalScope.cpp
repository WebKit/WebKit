/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "ExceptionCode.h"
#include "IDBConnectionProxy.h"
#include "InspectorInstrumentation.h"
#include "Performance.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "SocketProvider.h"
#include "WorkerInspectorController.h"
#include "WorkerLoaderProxy.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerReportingProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <inspector/ScriptArguments.h>
#include <inspector/ScriptCallStack.h>

using namespace Inspector;

namespace WebCore {

WorkerGlobalScope::WorkerGlobalScope(const URL& url, const String& identifier, const String& userAgent, WorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider)
    : m_url(url)
    , m_identifier(identifier)
    , m_userAgent(userAgent)
    , m_thread(thread)
    , m_script(std::make_unique<WorkerScriptController>(this))
    , m_inspectorController(std::make_unique<WorkerInspectorController>(*this))
    , m_shouldBypassMainWorldContentSecurityPolicy(shouldBypassMainWorldContentSecurityPolicy)
    , m_eventQueue(*this)
    , m_topOrigin(WTFMove(topOrigin))
#if ENABLE(INDEXED_DATABASE)
    , m_connectionProxy(connectionProxy)
#endif
#if ENABLE(WEB_SOCKETS)
    , m_socketProvider(socketProvider)
#endif
#if ENABLE(WEB_TIMING)
    , m_performance(Performance::create(*this, timeOrigin))
#endif
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif
#if !ENABLE(WEB_SOCKETS)
    UNUSED_PARAM(socketProvider);
#endif
#if !ENABLE(WEB_TIMING)
    UNUSED_PARAM(timeOrigin);
#endif

    auto origin = SecurityOrigin::create(url);
    if (m_topOrigin->hasUniversalAccess())
        origin->grantUniversalAccess();
    if (m_topOrigin->needsStorageAccessFromFileURLsQuirk())
        origin->grantStorageAccessFromFileURLsQuirk();

    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(origin)));
    setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(*this));
}

WorkerGlobalScope::~WorkerGlobalScope()
{
    ASSERT(currentThread() == thread().threadID());

#if ENABLE(WEB_TIMING)
    m_performance = nullptr;
#endif

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread().workerReportingProxy().workerGlobalScopeDestroyed();
}

void WorkerGlobalScope::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(WEB_TIMING)
    m_performance->removeAllEventListeners();
#endif
}

void WorkerGlobalScope::applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders)
{
    contentSecurityPolicy()->didReceiveHeaders(contentSecurityPolicyResponseHeaders);
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

#if ENABLE(WEB_SOCKETS)

SocketProvider* WorkerGlobalScope::socketProvider()
{
    return m_socketProvider.get();
}

#endif

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
    ASSERT(m_connectionProxy);
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

WorkerNavigator& WorkerGlobalScope::navigator() const
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(m_userAgent);
    return *m_navigator;
}

void WorkerGlobalScope::postTask(Task&& task)
{
    thread().runLoop().postTask(WTFMove(task));
}

int WorkerGlobalScope::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout)
{
    return DOMTimer::install(*this, WTFMove(action), std::chrono::milliseconds(timeout), true);
}

void WorkerGlobalScope::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(*this, timeoutId);
}

int WorkerGlobalScope::setInterval(std::unique_ptr<ScheduledAction> action, int timeout)
{
    return DOMTimer::install(*this, WTFMove(action), std::chrono::milliseconds(timeout), false);
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
            return Exception { SYNTAX_ERR };
        completedURLs.uncheckedAppend(WTFMove(url));
    }

    for (auto& url : completedURLs) {
        // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
        bool shouldBypassMainWorldContentSecurityPolicy = this->shouldBypassMainWorldContentSecurityPolicy();
        if (!shouldBypassMainWorldContentSecurityPolicy && !contentSecurityPolicy()->allowScriptFromSource(url))
            return Exception { NETWORK_ERR };

        auto scriptLoader = WorkerScriptLoader::create();
        scriptLoader->loadSynchronously(this, url, FetchOptions::Mode::NoCors, shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective, resourceRequestIdentifier());

        // If the fetching attempt failed, throw a NETWORK_ERR exception and abort all these steps.
        if (scriptLoader->failed())
            return Exception { NETWORK_ERR };

        InspectorInstrumentation::scriptImported(*this, scriptLoader->identifier(), scriptLoader->script());

        NakedPtr<JSC::Exception> exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader->script(), scriptLoader->responseURL()), exception);
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
    return currentThread() == thread().threadID();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

WorkerEventQueue& WorkerGlobalScope::eventQueue() const
{
    return m_eventQueue;
}

#if ENABLE(SUBTLE_CRYPTO)

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

#endif // ENABLE(SUBTLE_CRYPTO)

Crypto& WorkerGlobalScope::crypto()
{
    if (!m_crypto)
        m_crypto = Crypto::create(*this);
    return *m_crypto;
}

#if ENABLE(WEB_TIMING)

Performance& WorkerGlobalScope::performance() const
{
    return *m_performance;
}

#endif

} // namespace WebCore
