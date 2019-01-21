/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 */

#include "config.h"
#include "Worker.h"

#include "ContentSecurityPolicy.h"
#include "Event.h"
#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "PlatformStrategies.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "WorkerGlobalScopeProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashSet<Worker*>& allWorkers()
{
    static NeverDestroyed<HashSet<Worker*>> set;
    return set;
}

void Worker::networkStateChanged(bool isOnLine)
{
    for (auto* worker : allWorkers())
        worker->notifyNetworkStateChange(isOnLine);
}

inline Worker::Worker(ScriptExecutionContext& context, JSC::RuntimeFlags runtimeFlags, const Options& options)
    : ActiveDOMObject(&context)
    , m_name(options.name)
    , m_identifier("worker:" + Inspector::IdentifiersFactory::createIdentifier())
    , m_contextProxy(WorkerGlobalScopeProxy::create(*this))
    , m_runtimeFlags(runtimeFlags)
{
    static bool addedListener;
    if (!addedListener) {
        platformStrategies()->loaderStrategy()->addOnlineStateChangeListener(&networkStateChanged);
        addedListener = true;
    }

    auto addResult = allWorkers().add(this);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

ExceptionOr<Ref<Worker>> Worker::create(ScriptExecutionContext& context, JSC::RuntimeFlags runtimeFlags, const String& url, const Options& options)
{
    ASSERT(isMainThread());

    // We don't currently support nested workers, so workers can only be created from documents.
    ASSERT_WITH_SECURITY_IMPLICATION(context.isDocument());

    auto worker = adoptRef(*new Worker(context, runtimeFlags, options));

    worker->suspendIfNeeded();

    bool shouldBypassMainWorldContentSecurityPolicy = context.shouldBypassMainWorldContentSecurityPolicy();
    auto scriptURL = worker->resolveURL(url, shouldBypassMainWorldContentSecurityPolicy);
    if (scriptURL.hasException())
        return scriptURL.releaseException();

    worker->m_shouldBypassMainWorldContentSecurityPolicy = shouldBypassMainWorldContentSecurityPolicy;

    // The worker context does not exist while loading, so we must ensure that the worker object is not collected, nor are its event listeners.
    worker->setPendingActivity(worker.get());

    // https://html.spec.whatwg.org/multipage/workers.html#official-moment-of-creation
    worker->m_workerCreationTime = MonotonicTime::now();

    worker->m_scriptLoader = WorkerScriptLoader::create();
    auto contentSecurityPolicyEnforcement = shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceChildSrcDirective;

    ResourceRequest request { scriptURL.releaseReturnValue() };
    request.setInitiatorIdentifier(worker->m_identifier);

    FetchOptions fetchOptions;
    fetchOptions.mode = FetchOptions::Mode::SameOrigin;
    fetchOptions.cache = FetchOptions::Cache::Default;
    fetchOptions.redirect = FetchOptions::Redirect::Follow;
    fetchOptions.destination = FetchOptions::Destination::Worker;
    worker->m_scriptLoader->loadAsynchronously(context, WTFMove(request), WTFMove(fetchOptions), contentSecurityPolicyEnforcement, ServiceWorkersMode::All, worker);
    return WTFMove(worker);
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by worker context proxy, so it cannot be destroyed while a Worker exists.
    allWorkers().remove(this);
    m_contextProxy.workerObjectDestroyed();
}

ExceptionOr<void> Worker::postMessage(JSC::ExecState& state, JSC::JSValue messageValue, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(state, messageValue, WTFMove(transfer), ports, SerializationContext::WorkerPostMessage);
    if (message.hasException())
        return message.releaseException();

    // Disentangle the port in preparation for sending it to the remote context.
    auto channels = MessagePort::disentanglePorts(WTFMove(ports));
    if (channels.hasException())
        return channels.releaseException();

    m_contextProxy.postMessageToWorkerGlobalScope({ message.releaseReturnValue(), channels.releaseReturnValue() });
    return { };
}

void Worker::terminate()
{
    m_contextProxy.terminateWorkerGlobalScope();
}

bool Worker::canSuspendForDocumentSuspension() const
{
    // FIXME: It is not currently possible to suspend a worker, so pages with workers can not go into page cache.
    return false;
}

const char* Worker::activeDOMObjectName() const
{
    return "Worker";
}

void Worker::stop()
{
    terminate();
}

bool Worker::hasPendingActivity() const
{
    return m_contextProxy.hasPendingActivity() || ActiveDOMObject::hasPendingActivity();
}

void Worker::notifyNetworkStateChange(bool isOnLine)
{
    m_contextProxy.notifyNetworkStateChange(isOnLine);
}

void Worker::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    const URL& responseURL = response.url();
    if (!responseURL.protocolIsBlob() && !responseURL.protocolIs("file") && !SecurityOrigin::create(responseURL)->isUnique())
        m_contentSecurityPolicyResponseHeaders = ContentSecurityPolicyResponseHeaders(response);
    InspectorInstrumentation::didReceiveScriptResponse(scriptExecutionContext(), identifier);
}

void Worker::notifyFinished()
{
    auto* context = scriptExecutionContext();
    PAL::SessionID sessionID = context ? context->sessionID() : PAL::SessionID();

    if (m_scriptLoader->failed() || !sessionID.isValid())
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
    else {
        bool isOnline = platformStrategies()->loaderStrategy()->isOnLine();
        const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders = m_contentSecurityPolicyResponseHeaders ? m_contentSecurityPolicyResponseHeaders.value() : scriptExecutionContext()->contentSecurityPolicy()->responseHeaders();
        m_contextProxy.startWorkerGlobalScope(m_scriptLoader->url(), m_name, scriptExecutionContext()->userAgent(m_scriptLoader->url()), isOnline, m_scriptLoader->script(), contentSecurityPolicyResponseHeaders, m_shouldBypassMainWorldContentSecurityPolicy, m_workerCreationTime, m_runtimeFlags, sessionID);
        InspectorInstrumentation::scriptImported(*scriptExecutionContext(), m_scriptLoader->identifier(), m_scriptLoader->script());
    }
    m_scriptLoader = nullptr;

    unsetPendingActivity(*this);
}

} // namespace WebCore
