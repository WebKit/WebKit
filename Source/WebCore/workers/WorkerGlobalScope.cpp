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

#include "ActiveDOMObject.h"
#include "ContentSecurityPolicy.h"
#include "Crypto.h"
#include "DOMTimer.h"
#include "DOMURL.h"
#include "DOMWindow.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "IDBConnectionProxy.h"
#include "InspectorConsoleInstrumentation.h"
#include "MessagePort.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "SocketProvider.h"
#include "URL.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerObjectProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include "WorkerThreadableLoader.h"
#include <bindings/ScriptValue.h>
#include <inspector/ConsoleMessage.h>
#include <inspector/ScriptCallStack.h>
#include <wtf/RefPtr.h>

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "NotificationCenter.h"
#endif

using namespace Inspector;

namespace WebCore {

WorkerGlobalScope::WorkerGlobalScope(const URL& url, const String& userAgent, WorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, RefPtr<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider)
    : m_url(url)
    , m_userAgent(userAgent)
    , m_script(std::make_unique<WorkerScriptController>(this))
    , m_thread(thread)
    , m_closing(false)
    , m_shouldBypassMainWorldContentSecurityPolicy(shouldBypassMainWorldContentSecurityPolicy)
    , m_eventQueue(*this)
    , m_topOrigin(topOrigin)
#if ENABLE(INDEXED_DATABASE)
    , m_connectionProxy(connectionProxy)
#endif
#if ENABLE(WEB_SOCKETS)
    , m_socketProvider(socketProvider)
#endif
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif
#if !ENABLE(WEB_SOCKETS)
    UNUSED_PARAM(socketProvider);
#endif

    auto origin = SecurityOrigin::create(url);
    if (m_topOrigin->hasUniversalAccess())
        origin->grantUniversalAccess();

    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(origin)));
    setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(*this));
}

WorkerGlobalScope::~WorkerGlobalScope()
{
    ASSERT(currentThread() == thread().threadID());

    // Make sure we have no observers.
    notifyObserversOfStop();

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread().workerReportingProxy().workerGlobalScopeDestroyed();
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

void WorkerGlobalScope::importScripts(const Vector<String>& urls, ExceptionCode& ec)
{
    ASSERT(contentSecurityPolicy());
    ec = 0;
    Vector<URL> completedURLs;
    for (auto& entry : urls) {
        URL url = scriptExecutionContext()->completeURL(entry);
        if (!url.isValid()) {
            ec = SYNTAX_ERR;
            return;
        }
        completedURLs.append(WTFMove(url));
    }

    for (auto& url : completedURLs) {
        // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
        bool shouldBypassMainWorldContentSecurityPolicy = scriptExecutionContext()->shouldBypassMainWorldContentSecurityPolicy();
        if (!scriptExecutionContext()->contentSecurityPolicy()->allowScriptFromSource(url, shouldBypassMainWorldContentSecurityPolicy)) {
            ec = NETWORK_ERR;
            return;
        }

        Ref<WorkerScriptLoader> scriptLoader = WorkerScriptLoader::create();
        scriptLoader->loadSynchronously(scriptExecutionContext(), url, FetchOptions::Mode::NoCors, shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective);

        // If the fetching attempt failed, throw a NETWORK_ERR exception and abort all these steps.
        if (scriptLoader->failed()) {
            ec = NETWORK_ERR;
            return;
        }

        InspectorInstrumentation::scriptImported(scriptExecutionContext(), scriptLoader->identifier(), scriptLoader->script());

        NakedPtr<JSC::Exception> exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader->script(), scriptLoader->responseURL()), exception);
        if (exception) {
            m_script->setException(exception);
            return;
        }
    }
}

EventTarget* WorkerGlobalScope::errorEventTarget()
{
    return this;
}

void WorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<ScriptCallStack>&&)
{
    thread().workerReportingProxy().postExceptionToWorkerObject(errorMessage, lineNumber, columnNumber, sourceURL);
}

void WorkerGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage> message)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(message->source(), message->level(), message->message()));
        return;
    }

    thread().workerReportingProxy().postConsoleMessageToWorkerObject(message->source(), message->level(), message->message(), message->line(), message->column(), message->url());
    addMessageToWorkerConsole(WTFMove(message));
}

void WorkerGlobalScope::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    thread().workerReportingProxy().postConsoleMessageToWorkerObject(source, level, message, 0, 0, String());
    addMessageToWorkerConsole(source, level, message, String(), 0, 0, 0, 0, requestIdentifier);
}

void WorkerGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    thread().workerReportingProxy().postConsoleMessageToWorkerObject(source, level, message, lineNumber, columnNumber, sourceURL);
    addMessageToWorkerConsole(source, level, message, sourceURL, lineNumber, columnNumber, WTFMove(callStack), state, requestIdentifier);
}

void WorkerGlobalScope::addMessageToWorkerConsole(MessageSource source, MessageLevel level, const String& messageText, const String& suggestedURL, unsigned suggestedLineNumber, unsigned suggestedColumnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    std::unique_ptr<Inspector::ConsoleMessage> message;

    if (callStack)
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, WTFMove(callStack), requestIdentifier);
    else
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, suggestedURL, suggestedLineNumber, suggestedColumnNumber, state, requestIdentifier);

    addMessageToWorkerConsole(WTFMove(message));
}

void WorkerGlobalScope::addMessageToWorkerConsole(std::unique_ptr<Inspector::ConsoleMessage> message)
{
    ASSERT(isContextThread());
    InspectorInstrumentation::addMessageToConsole(this, WTFMove(message));
}

bool WorkerGlobalScope::isContextThread() const
{
    return currentThread() == thread().threadID();
}

bool WorkerGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

WorkerGlobalScope::Observer::Observer(WorkerGlobalScope* context)
    : m_context(context)
{
    ASSERT(m_context && m_context->isContextThread());
    m_context->registerObserver(this);
}

WorkerGlobalScope::Observer::~Observer()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
}

void WorkerGlobalScope::Observer::stopObserving()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
    m_context = 0;
}

void WorkerGlobalScope::registerObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.add(observer);
}

void WorkerGlobalScope::unregisterObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.remove(observer);
}

void WorkerGlobalScope::notifyObserversOfStop()
{
    HashSet<Observer*>::iterator iter = m_workerObservers.begin();
    while (iter != m_workerObservers.end()) {
        WorkerGlobalScope::Observer* observer = *iter;
        observer->stopObserving();
        observer->notifyStop();
        iter = m_workerObservers.begin();
    }
}

WorkerEventQueue& WorkerGlobalScope::eventQueue() const
{
    return m_eventQueue;
}

#if ENABLE(SUBTLE_CRYPTO)
bool WorkerGlobalScope::wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&)
{
    return false;
}

bool WorkerGlobalScope::unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&)
{
    return false;
}
#endif // ENABLE(SUBTLE_CRYPTO)

Crypto& WorkerGlobalScope::crypto() const
{
    if (!m_crypto)
        m_crypto = Crypto::create(*scriptExecutionContext());
    return *m_crypto;
}

} // namespace WebCore
