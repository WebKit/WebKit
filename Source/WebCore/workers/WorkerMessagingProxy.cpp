/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009-2022 Google Inc. All Rights Reserved.
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
#include "WorkerMessagingProxy.h"

#include "CacheStorageProvider.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "DedicatedWorkerGlobalScope.h"
#include "DedicatedWorkerThread.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "FetchRequestCredentials.h"
#include "LoaderStrategy.h"
#include "MessageEvent.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "WebRTCProvider.h"
#include "Worker.h"
#include "WorkerInitializationData.h"
#include "WorkerInspectorProxy.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

// WorkerUserGestureForwarder is a ThreadSafeRefCounted utility class indended for
// holding a non-thread-safe RefCounted UserGestureToken. Because UserGestureToken
// is not intended to be used off the main thread, all WorkerUserGestureForwarder
// public methods, constructor, and destructor, can only be used from the main thread.
// The WorkerUserGestureForwarder, on the other hand, can be ref'd and deref'd from
// non-main thread contexts, allowing it to be passed from main to Worker scopes and
// vice versa.
class WorkerUserGestureForwarder : public ThreadSafeRefCounted<WorkerUserGestureForwarder, WTF::DestructionThread::Main> {
public:
    static Ref<WorkerUserGestureForwarder> create(RefPtr<UserGestureToken>&& token) { return *new WorkerUserGestureForwarder(WTFMove(token)); }

    ~WorkerUserGestureForwarder()
    {
        ASSERT(isMainThread());
        m_token = nullptr;
    }

    UserGestureToken* userGestureToForward() const
    {
        ASSERT(isMainThread());
        if (!m_token || m_token->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwarding))
            return nullptr;
        return m_token.get();
    }

private:
    explicit WorkerUserGestureForwarder(RefPtr<UserGestureToken>&& token)
        : m_token(WTFMove(token))
    {
        ASSERT(isMainThread());
    }

    RefPtr<UserGestureToken> m_token;
};

WorkerGlobalScopeProxy& WorkerGlobalScopeProxy::create(Worker& worker)
{
    return *new WorkerMessagingProxy(worker);
}

WorkerMessagingProxy::WorkerMessagingProxy(Worker& workerObject)
    : m_scriptExecutionContext(workerObject.scriptExecutionContext())
    , m_inspectorProxy(WorkerInspectorProxy::create(workerObject.identifier()))
    , m_workerObject(&workerObject)
{
    ASSERT((is<Document>(*m_scriptExecutionContext) && isMainThread())
        || (is<WorkerGlobalScope>(*m_scriptExecutionContext) && downcast<WorkerGlobalScope>(*m_scriptExecutionContext).thread().thread() == &Thread::current()));

    if (is<Document>(*m_scriptExecutionContext))
        m_loaderContextIdentifier = m_scriptExecutionContext->identifier();
    else
        m_loaderContextIdentifier = downcast<WorkerGlobalScope>(*m_scriptExecutionContext).thread().workerLoaderProxy().loaderContextIdentifier();

    // Nobody outside this class ref counts this object. The original ref
    // is balanced by the deref in workerGlobalScopeDestroyedInternal.
}

WorkerMessagingProxy::~WorkerMessagingProxy()
{
    ASSERT(!m_workerObject);
    ASSERT(!m_scriptExecutionContext
        || (is<Document>(*m_scriptExecutionContext) && isMainThread())
        || (is<WorkerGlobalScope>(*m_scriptExecutionContext) && downcast<WorkerGlobalScope>(*m_scriptExecutionContext).thread().thread() == &Thread::current()));
}

void WorkerMessagingProxy::startWorkerGlobalScope(const URL& scriptURL, PAL::SessionID sessionID, const String& name, WorkerInitializationData&& initializationData, const ScriptBuffer& sourceCode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const CrossOriginEmbedderPolicy& crossOriginEmbedderPolicy, MonotonicTime timeOrigin, ReferrerPolicy referrerPolicy, WorkerType workerType, FetchRequestCredentials credentials, JSC::RuntimeFlags runtimeFlags)
{
    if (!m_scriptExecutionContext)
        return;
    
    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was created.
        return;
    }

    auto* parentWorkerGlobalScope = dynamicDowncast<WorkerGlobalScope>(m_scriptExecutionContext.get());
    WorkerThreadStartMode startMode = m_inspectorProxy->workerStartMode(*m_scriptExecutionContext.get());
    String identifier = m_inspectorProxy->identifier();

    IDBClient::IDBConnectionProxy* proxy = m_scriptExecutionContext->idbConnectionProxy();

    SocketProvider* socketProvider = m_scriptExecutionContext->socketProvider();

    bool isOnline = parentWorkerGlobalScope ? parentWorkerGlobalScope->isOnline() : platformStrategies()->loaderStrategy()->isOnLine();

    WorkerParameters params { scriptURL, m_scriptExecutionContext->url(), name, identifier, WTFMove(initializationData.userAgent), isOnline, contentSecurityPolicyResponseHeaders, shouldBypassMainWorldContentSecurityPolicy, crossOriginEmbedderPolicy, timeOrigin, referrerPolicy, workerType, credentials, m_scriptExecutionContext->settingsValues(), WorkerThreadMode::CreateNewThread, sessionID,
#if ENABLE(SERVICE_WORKER)
        WTFMove(initializationData.serviceWorkerData),
#endif
        initializationData.clientIdentifier.value_or(ScriptExecutionContextIdentifier { })
    };
    auto thread = DedicatedWorkerThread::create(params, sourceCode, *this, *this, *this, startMode, m_scriptExecutionContext->topOrigin(), proxy, socketProvider, runtimeFlags);

    if (parentWorkerGlobalScope)
        parentWorkerGlobalScope->thread().addChildThread(thread);

    workerThreadCreated(thread.get());
    thread->start();

    m_inspectorProxy->workerStarted(m_scriptExecutionContext.get(), thread.ptr(), scriptURL, name);
}

void WorkerMessagingProxy::postMessageToWorkerObject(MessageWithMessagePorts&& message)
{
    if (!m_scriptExecutionContext)
        return;

    // Pass a RefPtr to the WorkerUserGestureForwarder, if present, into the main thread
    // task; the m_userGestureForwarder ivar may be cleared after this function returns.
    m_scriptExecutionContext->postTask([this, message = WTFMove(message), userGestureForwarder = m_userGestureForwarder] (auto& context) mutable {
        Worker* workerObject = this->workerObject();
        if (!workerObject || askedToTerminate())
            return;

        auto ports = MessagePort::entanglePorts(context, WTFMove(message.transferredPorts));
        ActiveDOMObject::queueTaskKeepingObjectAlive(*workerObject, TaskSource::PostedMessageQueue, [worker = Ref { *workerObject }, message = WTFMove(message), userGestureForwarder = WTFMove(userGestureForwarder), ports = WTFMove(ports)] () mutable {
            if (!worker->scriptExecutionContext())
                return;

            auto* globalObject = worker->scriptExecutionContext()->globalObject();
            if (!globalObject)
                return;

            UserGestureIndicator userGestureIndicator(userGestureForwarder ? userGestureForwarder->userGestureToForward() : nullptr);
            auto event = MessageEvent::create(*globalObject, message.message.releaseNonNull(), { }, { }, { }, WTFMove(ports));
            worker->dispatchEvent(event.event);
        });
    });
}

void WorkerMessagingProxy::postTaskToWorkerObject(Function<void(Worker&)>&& function)
{
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->postTask([this, function = WTFMove(function)](auto&) mutable {
        auto* workerObject = this->workerObject();
        if (!workerObject || askedToTerminate())
            return;
        function(*workerObject);
    });
}

void WorkerMessagingProxy::postMessageToWorkerGlobalScope(MessageWithMessagePorts&& message)
{
    RefPtr<WorkerUserGestureForwarder> userGestureForwarder;
    if (isMainThread())
        userGestureForwarder = WorkerUserGestureForwarder::create(UserGestureIndicator::currentUserGesture());

    postTaskToWorkerGlobalScope([this, protectedThis = Ref { *this }, message = WTFMove(message), userGestureForwarder = WTFMove(userGestureForwarder)](auto& scriptContext) mutable {
        ASSERT_WITH_SECURITY_IMPLICATION(scriptContext.isWorkerGlobalScope());
        auto& context = static_cast<DedicatedWorkerGlobalScope&>(scriptContext);
        auto* globalObject = context.globalObject();
        if (!globalObject)
            return;

        // Setting m_userGestureForwarder here, before dispatching the MessageEvent, will allow all calls to
        // worker.postMessage() made during the handling of that MessageEvent to inherit the UserGestureToken
        // held by the forwarder; see postMessageToWorkerObject() above.
        m_userGestureForwarder = WTFMove(userGestureForwarder);

        auto ports = MessagePort::entanglePorts(scriptContext, WTFMove(message.transferredPorts));
        auto event = MessageEvent::create(*globalObject, message.message.releaseNonNull(), { }, { }, std::nullopt, WTFMove(ports));
        context.dispatchEvent(event.event);

        // Because WorkerUserGestureForwarder is defined as DestructionThread::Main, releasing this Ref
        // on the Worker thread will cause the forwarder to be destroyed on the main thread.
        m_userGestureForwarder = nullptr;
    });
}

void WorkerMessagingProxy::postTaskToWorkerGlobalScope(Function<void(ScriptExecutionContext&)>&& task)
{
    if (m_askedToTerminate)
        return;

    if (!m_workerThread) {
        m_queuedEarlyTasks.append(makeUnique<ScriptExecutionContext::Task>(WTFMove(task)));
        return;
    }
    m_workerThread->runLoop().postTask(WTFMove(task));
}

void WorkerMessagingProxy::suspendForBackForwardCache()
{
    if (m_workerThread)
        m_workerThread->suspend();
    else
        m_askedToSuspend = true;
}

void WorkerMessagingProxy::resumeForBackForwardCache()
{
    if (m_workerThread)
        m_workerThread->resume();
    else
        m_askedToSuspend = false;
}

ScriptExecutionContextIdentifier WorkerMessagingProxy::loaderContextIdentifier() const
{
    return m_loaderContextIdentifier;
}

void WorkerMessagingProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    ScriptExecutionContext::postTaskTo(m_loaderContextIdentifier, WTFMove(task));
}

RefPtr<CacheStorageConnection> WorkerMessagingProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_scriptExecutionContext)
        return nullptr;

    auto* document = dynamicDowncast<Document>(*m_scriptExecutionContext);
    ASSERT(document);
    if (!document || !document->page())
        return nullptr;
    return document->page()->cacheStorageProvider().createCacheStorageConnection();
}

StorageConnection* WorkerMessagingProxy::storageConnection()
{
    ASSERT(isMainThread());
    auto* document = dynamicDowncast<Document>(*m_scriptExecutionContext);
    ASSERT(document);

    return document ? document->storageConnection() : nullptr;
}

RefPtr<RTCDataChannelRemoteHandlerConnection> WorkerMessagingProxy::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    auto* document = dynamicDowncast<Document>(*m_scriptExecutionContext);
    ASSERT(document);
    if (!document || !document->page())
        return nullptr;
    return document->page()->webRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

void WorkerMessagingProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->postTask([this, errorMessage = errorMessage.isolatedCopy(), sourceURL = sourceURL.isolatedCopy(), lineNumber, columnNumber] (ScriptExecutionContext&) {
        Worker* workerObject = this->workerObject();
        if (!workerObject)
            return;

        // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
        // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.
        ActiveDOMObject::queueTaskToDispatchEvent(*workerObject, TaskSource::DOMManipulation, ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, { }));
    });
}

void WorkerMessagingProxy::postMessageToDebugger(const String& message)
{
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }, message = message.isolatedCopy()]() mutable {
        if (!m_mayBeDestroyed)
            m_inspectorProxy->sendMessageFromWorkerToFrontend(WTFMove(message));
    });
}

void WorkerMessagingProxy::setResourceCachingDisabledByWebInspector(bool disabled)
{
    postTaskToLoader([disabled] (ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        if (auto* page = downcast<Document>(context).page())
            page->setResourceCachingDisabledByWebInspector(disabled);
    });
}

void WorkerMessagingProxy::workerThreadCreated(DedicatedWorkerThread& workerThread)
{
    ASSERT(!m_askedToTerminate);
    m_workerThread = &workerThread;

    if (m_askedToSuspend) {
        m_askedToSuspend = false;
        m_workerThread->suspend();
    }

    auto queuedEarlyTasks = std::exchange(m_queuedEarlyTasks, { });
    for (auto& task : queuedEarlyTasks)
        m_workerThread->runLoop().postTask(WTFMove(*task));
}

void WorkerMessagingProxy::workerObjectDestroyed()
{
    m_workerObject = nullptr;
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        m_mayBeDestroyed = true;
        if (m_workerThread)
            terminateWorkerGlobalScope();
        else
            workerGlobalScopeDestroyedInternal();
    });
}

void WorkerMessagingProxy::notifyNetworkStateChange(bool isOnline)
{
    if (m_askedToTerminate)
        return;

    if (!m_workerThread)
        return;

    m_workerThread->runLoop().postTask([isOnline] (ScriptExecutionContext& context) {
        auto& globalScope = downcast<WorkerGlobalScope>(context);
        globalScope.setIsOnline(isOnline);
        globalScope.dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void WorkerMessagingProxy::workerGlobalScopeDestroyed()
{
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        workerGlobalScopeDestroyedInternal();
    });
}

void WorkerMessagingProxy::workerGlobalScopeClosed()
{
    if (!m_scriptExecutionContext)
        return;

    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        terminateWorkerGlobalScope();
    });
}

void WorkerMessagingProxy::workerGlobalScopeDestroyedInternal()
{
    // This is always the last task to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;

    m_inspectorProxy->workerTerminated();

    if (auto* workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(m_scriptExecutionContext.get()); workerGlobalScope && m_workerThread)
        workerGlobalScope->thread().removeChildThread(*m_workerThread);

    m_workerThread = nullptr;
    m_scriptExecutionContext = nullptr;

    // This balances the original ref in construction.
    if (m_mayBeDestroyed)
        deref();
}

void WorkerMessagingProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    m_inspectorProxy->workerTerminated();

    if (m_workerThread)
        m_workerThread->stop(nullptr);
    else
        m_scriptExecutionContext = nullptr;
}

} // namespace WebCore
