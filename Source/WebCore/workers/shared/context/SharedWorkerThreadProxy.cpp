/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SharedWorkerThreadProxy.h"

#include "CacheStorageProvider.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "LibWebRTCProvider.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "Page.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "SharedWorker.h"
#include "SharedWorkerGlobalScope.h"
#include "SharedWorkerThread.h"

namespace WebCore {

SharedWorkerThreadProxy::SharedWorkerThreadProxy(SharedWorker& sharedWorker)
    : m_sharedWorker(sharedWorker)
    , m_scriptExecutionContext(sharedWorker.scriptExecutionContext())
    , m_identifierForInspector(sharedWorker.identifierForInspector())
{
}

void SharedWorkerThreadProxy::startWorkerGlobalScope(const URL& scriptURL, const String& name, const String& userAgent, bool isOnline, const ScriptBuffer& scriptBuffer, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const CrossOriginEmbedderPolicy& crossOriginEmbedderPolicy, MonotonicTime timeOrigin, ReferrerPolicy referrerPolicy, WorkerType workerType, FetchRequestCredentials credentials, JSC::RuntimeFlags runtimeFlags)
{
    if (m_askedToTerminate)
        return;

    auto parameters = WorkerParameters {
        scriptURL,
        name,
        m_identifierForInspector,
        userAgent,
        isOnline,
        contentSecurityPolicyResponseHeaders,
        shouldBypassMainWorldContentSecurityPolicy,
        crossOriginEmbedderPolicy,
        timeOrigin,
        referrerPolicy,
        workerType,
        credentials,
        m_scriptExecutionContext->settingsValues()
    };

    if (!m_workerThread) {
        m_workerThread = SharedWorkerThread::create(parameters, scriptBuffer, *this, *this, *this, WorkerThreadStartMode::Normal, m_scriptExecutionContext->topOrigin(), m_scriptExecutionContext->idbConnectionProxy(), m_scriptExecutionContext->socketProvider(), runtimeFlags);
        m_workerThread->start();
    }
}

void SharedWorkerThreadProxy::terminateWorkerGlobalScope()
{
    if (m_askedToTerminate)
        return;

    m_askedToTerminate = true;
    if (m_workerThread)
        m_workerThread->stop(nullptr);
}

void SharedWorkerThreadProxy::postMessageToWorkerGlobalScope(MessageWithMessagePorts&& message)
{
    // FIXME: SharedWorker doesn't have postMessage, so this might not be necessary.
    postTaskToWorkerGlobalScope([message = WTFMove(message)](auto& scriptContext) mutable {
        auto& context = downcast<SharedWorkerGlobalScope>(scriptContext);
        auto ports = MessagePort::entanglePorts(scriptContext, WTFMove(message.transferredPorts));
        context.dispatchEvent(MessageEvent::create(WTFMove(ports), message.message.releaseNonNull()));
    });
}

void SharedWorkerThreadProxy::postTaskToWorkerGlobalScope(Function<void(ScriptExecutionContext&)>&& task)
{
    if (m_askedToTerminate)
        return;
    m_workerThread->runLoop().postTask(WTFMove(task));
}

bool SharedWorkerThreadProxy::hasPendingActivity() const
{
    return m_hasPendingActivity && !m_askedToTerminate;
}

void SharedWorkerThreadProxy::workerObjectDestroyed()
{
    m_sharedWorker = nullptr;
    m_scriptExecutionContext->postTask([this] (auto&) {
        m_mayBeDestroyed = true;
        if (m_workerThread)
            terminateWorkerGlobalScope();
        else
            workerGlobalScopeDestroyedInternal();
    });
}

void SharedWorkerThreadProxy::notifyNetworkStateChange(bool isOnline)
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

void SharedWorkerThreadProxy::suspendForBackForwardCache()
{

}

void SharedWorkerThreadProxy::resumeForBackForwardCache()
{

}

void SharedWorkerThreadProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    m_scriptExecutionContext->postTask([this, errorMessage = errorMessage.isolatedCopy(), sourceURL = sourceURL.isolatedCopy(), lineNumber, columnNumber] (ScriptExecutionContext&) {
        if (!m_sharedWorker)
            return;

        // We don't bother checking the askedToTerminate() flag here, because exceptions should *always* be reported even if the thread is terminated.
        // This is intentionally different than the behavior in MessageWorkerTask, because terminated workers no longer deliver messages (section 4.6 of the WebWorker spec), but they do report exceptions.
        ActiveDOMObject::queueTaskToDispatchEvent(*m_sharedWorker, TaskSource::DOMManipulation, ErrorEvent::create(errorMessage, sourceURL, lineNumber, columnNumber, { }));
    });
}

void SharedWorkerThreadProxy::workerGlobalScopeDestroyed()
{
    m_scriptExecutionContext->postTask([this] (ScriptExecutionContext&) {
        workerGlobalScopeDestroyedInternal();
    });
}

void SharedWorkerThreadProxy::postMessageToWorkerObject(MessageWithMessagePorts&&)
{

}

void SharedWorkerThreadProxy::confirmMessageFromWorkerObject(bool)
{
}

void SharedWorkerThreadProxy::reportPendingActivity(bool hasPendingActivity)
{
    m_scriptExecutionContext->postTask([this, hasPendingActivity] (ScriptExecutionContext&) {
        m_hasPendingActivity = hasPendingActivity;
    });
}

RefPtr<CacheStorageConnection> SharedWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    auto& document = downcast<Document>(*m_scriptExecutionContext);
    return document.page()->cacheStorageProvider().createCacheStorageConnection();
}

RefPtr<RTCDataChannelRemoteHandlerConnection> SharedWorkerThreadProxy::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    auto& document = downcast<Document>(*m_scriptExecutionContext);
    if (!document.page())
        return nullptr;
    return document.page()->libWebRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

void SharedWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    m_scriptExecutionContext->postTask(WTFMove(task));
}

bool SharedWorkerThreadProxy::postTaskForModeToWorkerOrWorkletGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    if (m_askedToTerminate)
        return false;

    m_workerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

void SharedWorkerThreadProxy::postMessageToDebugger(const String&)
{

}

void SharedWorkerThreadProxy::setResourceCachingDisabledByWebInspector(bool)
{

}

void SharedWorkerThreadProxy::workerGlobalScopeDestroyedInternal()
{
    // This is always the last task to be performed, so the proxy is not needed for communication
    // in either side any more. However, the Worker object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = nullptr;

    // This balances the original ref in construction.
    if (m_mayBeDestroyed)
        deref();
}

} // namespace WebCore
