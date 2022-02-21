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
#include "DedicatedWorkerGlobalScope.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "PlatformStrategies.h"
#if ENABLE(WEB_RTC)
#include "RTCRtpScriptTransform.h"
#include "RTCRtpScriptTransformer.h"
#endif
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "StructuredSerializeOptions.h"
#include "WorkerGlobalScopeProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Worker);

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

Worker::Worker(ScriptExecutionContext& context, JSC::RuntimeFlags runtimeFlags, WorkerOptions&& options)
    : ActiveDOMObject(&context)
    , m_options(WTFMove(options))
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

ExceptionOr<Ref<Worker>> Worker::create(ScriptExecutionContext& context, JSC::RuntimeFlags runtimeFlags, const String& url, WorkerOptions&& options)
{
    ASSERT(isMainThread());

    // We don't currently support nested workers, so workers can only be created from documents.
    ASSERT_WITH_SECURITY_IMPLICATION(context.isDocument());

    auto worker = adoptRef(*new Worker(context, runtimeFlags, WTFMove(options)));

    worker->suspendIfNeeded();

    auto scriptURL = worker->resolveURL(url);
    if (scriptURL.hasException())
        return scriptURL.releaseException();

    bool shouldBypassMainWorldContentSecurityPolicy = context.shouldBypassMainWorldContentSecurityPolicy();
    worker->m_shouldBypassMainWorldContentSecurityPolicy = shouldBypassMainWorldContentSecurityPolicy;

    // https://html.spec.whatwg.org/multipage/workers.html#official-moment-of-creation
    worker->m_workerCreationTime = MonotonicTime::now();

    worker->m_scriptLoader = WorkerScriptLoader::create();
    auto contentSecurityPolicyEnforcement = shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective;

    ResourceRequest request { scriptURL.releaseReturnValue() };
    request.setInitiatorIdentifier(worker->m_identifier);

    auto source = options.type == WorkerType::Module ? WorkerScriptLoader::Source::ModuleScript : WorkerScriptLoader::Source::ClassicWorkerScript;
    worker->m_scriptLoader->loadAsynchronously(context, WTFMove(request), source, workerFetchOptions(worker->m_options, FetchOptions::Destination::Worker), contentSecurityPolicyEnforcement, ServiceWorkersMode::All, worker.get(), WorkerRunLoop::defaultMode());

    return worker;
}

Worker::~Worker()
{
    ASSERT(isMainThread());
    ASSERT(scriptExecutionContext()); // The context is protected by worker context proxy, so it cannot be destroyed while a Worker exists.
    allWorkers().remove(this);
    m_contextProxy.workerObjectDestroyed();
}

ExceptionOr<void> Worker::postMessage(JSC::JSGlobalObject& state, JSC::JSValue messageValue, StructuredSerializeOptions&& options)
{
    Vector<RefPtr<MessagePort>> ports;
    auto message = SerializedScriptValue::create(state, messageValue, WTFMove(options.transfer), ports, SerializationContext::WorkerPostMessage);
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
    m_wasTerminated = true;
}

const char* Worker::activeDOMObjectName() const
{
    return "Worker";
}

void Worker::stop()
{
    terminate();
}

void Worker::suspend(ReasonForSuspension reason)
{
    if (reason == ReasonForSuspension::BackForwardCache) {
        m_contextProxy.suspendForBackForwardCache();
        m_isSuspendedForBackForwardCache = true;
    }
}

void Worker::resume()
{
    if (m_isSuspendedForBackForwardCache) {
        m_contextProxy.resumeForBackForwardCache();
        m_isSuspendedForBackForwardCache = false;
    }
}

bool Worker::virtualHasPendingActivity() const
{
    return m_contextProxy.hasPendingActivity() || m_scriptLoader;
}

void Worker::notifyNetworkStateChange(bool isOnLine)
{
    m_contextProxy.notifyNetworkStateChange(isOnLine);
}

void Worker::didReceiveResponse(ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    const URL& responseURL = response.url();
    if (!responseURL.protocolIsBlob() && !responseURL.protocolIs("file") && !SecurityOrigin::create(responseURL)->isUnique())
        m_contentSecurityPolicyResponseHeaders = ContentSecurityPolicyResponseHeaders(response);
    InspectorInstrumentation::didReceiveScriptResponse(scriptExecutionContext(), identifier);
}

void Worker::notifyFinished()
{
    auto clearLoader = makeScopeExit([this] {
        m_scriptLoader = nullptr;
    });

    auto* context = scriptExecutionContext();
    if (!context)
        return;

    if (m_scriptLoader->failed()) {
        queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
        return;
    }

    bool isOnline = platformStrategies()->loaderStrategy()->isOnLine();
    const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders = m_contentSecurityPolicyResponseHeaders ? m_contentSecurityPolicyResponseHeaders.value() : context->contentSecurityPolicy()->responseHeaders();
    ReferrerPolicy referrerPolicy = ReferrerPolicy::EmptyString;
    if (auto policy = parseReferrerPolicy(m_scriptLoader->referrerPolicy(), ReferrerPolicySource::HTTPHeader))
        referrerPolicy = *policy;

    m_contextProxy.startWorkerGlobalScope(m_scriptLoader->lastRequestURL(), m_options.name, context->userAgent(m_scriptLoader->lastRequestURL()), isOnline, m_scriptLoader->script(), contentSecurityPolicyResponseHeaders, m_shouldBypassMainWorldContentSecurityPolicy, m_scriptLoader->crossOriginEmbedderPolicy(), m_workerCreationTime, referrerPolicy, m_options.type, m_options.credentials, m_runtimeFlags);
    InspectorInstrumentation::scriptImported(*context, m_scriptLoader->identifier(), m_scriptLoader->script().toString());
}

void Worker::dispatchEvent(Event& event)
{
    if (m_wasTerminated)
        return;

    AbstractWorker::dispatchEvent(event);
    if (is<ErrorEvent>(event) && !event.defaultPrevented() && event.isTrusted() && scriptExecutionContext()) {
        auto& errorEvent = downcast<ErrorEvent>(event);
        scriptExecutionContext()->reportException(errorEvent.message(), errorEvent.lineno(), errorEvent.colno(), errorEvent.filename(), nullptr, nullptr);
    }
}

#if ENABLE(WEB_RTC)
void Worker::createRTCRtpScriptTransformer(RTCRtpScriptTransform& transform, MessageWithMessagePorts&& options)
{
    if (!scriptExecutionContext())
        return;

    m_contextProxy.postTaskToWorkerGlobalScope([transform = Ref { transform }, options = WTFMove(options)](auto& context) mutable {
        if (auto transformer = downcast<DedicatedWorkerGlobalScope>(context).createRTCRtpScriptTransformer(WTFMove(options)))
            transform->setTransformer(*transformer);
    });

}

void Worker::postTaskToWorkerGlobalScope(Function<void(ScriptExecutionContext&)>&& task)
{
    m_contextProxy.postTaskToWorkerGlobalScope(WTFMove(task));
}
#endif

} // namespace WebCore
