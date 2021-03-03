/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerThreadableLoader.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "DocumentThreadableLoader.h"
#include "InspectorInstrumentation.h"
#include "Performance.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ResourceTiming.h"
#include "SecurityOrigin.h"
#include "ServiceWorker.h"
#include "ServiceWorkerGlobalScope.h"
#include "ThreadableLoader.h"
#include "WorkerLoaderProxy.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/MainThread.h>
#include <wtf/Vector.h>

namespace WebCore {

WorkerThreadableLoader::WorkerThreadableLoader(WorkerOrWorkletGlobalScope& workerOrWorkletGlobalScope, ThreadableLoaderClient& client, const String& taskMode, ResourceRequest&& request, const ThreadableLoaderOptions& options, const String& referrer)
    : m_WorkerOrWorkletGlobalScope(workerOrWorkletGlobalScope)
    , m_workerClientWrapper(ThreadableLoaderClientWrapper::create(client, options.initiator))
    , m_bridge(*new MainThreadBridge(m_workerClientWrapper.get(), workerOrWorkletGlobalScope.workerOrWorkletThread()->workerLoaderProxy(), taskMode, WTFMove(request), options, referrer.isEmpty() ? workerOrWorkletGlobalScope.url().strippedForUseAsReferrer() : referrer, workerOrWorkletGlobalScope))
{
}

WorkerThreadableLoader::~WorkerThreadableLoader()
{
    m_bridge.destroy();
}

void WorkerThreadableLoader::loadResourceSynchronously(WorkerOrWorkletGlobalScope& workerOrWorkletGlobalScope, ResourceRequest&& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    WorkerRunLoop& runLoop = workerOrWorkletGlobalScope.workerOrWorkletThread()->runLoop();

    // Create a unique mode just for this synchronous resource load.
    String mode = makeString("loadResourceSynchronouslyMode", runLoop.createUniqueId());

    auto loader = WorkerThreadableLoader::create(workerOrWorkletGlobalScope, client, mode, WTFMove(request), options, String());
    MessageQueueWaitResult result = MessageQueueMessageReceived;
    while (!loader->done() && result != MessageQueueTerminated)
        result = runLoop.runInMode(&workerOrWorkletGlobalScope, mode);

    if (!loader->done() && result == MessageQueueTerminated)
        loader->cancel();
}

void WorkerThreadableLoader::cancel()
{
    m_bridge.cancel();
}

void WorkerThreadableLoader::computeIsDone()
{
    m_bridge.computeIsDone();
}

struct LoaderTaskOptions {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    LoaderTaskOptions(const ThreadableLoaderOptions&, const String&, Ref<SecurityOrigin>&&);
    ThreadableLoaderOptions options;
    String referrer;
    Ref<SecurityOrigin> origin;
};

LoaderTaskOptions::LoaderTaskOptions(const ThreadableLoaderOptions& options, const String& referrer, Ref<SecurityOrigin>&& origin)
    : options(options.isolatedCopy())
    , referrer(referrer.isolatedCopy())
    , origin(WTFMove(origin))
{
}

WorkerThreadableLoader::MainThreadBridge::MainThreadBridge(ThreadableLoaderClientWrapper& workerClientWrapper, WorkerLoaderProxy& loaderProxy, const String& taskMode,
    ResourceRequest&& request, const ThreadableLoaderOptions& options, const String& outgoingReferrer, WorkerOrWorkletGlobalScope& globalScope)
    : m_workerClientWrapper(&workerClientWrapper)
    , m_loaderProxy(loaderProxy)
    , m_taskMode(taskMode.isolatedCopy())
    , m_workerRequestIdentifier(globalScope.createUniqueIdentifier())
{
    auto* securityOrigin = globalScope.securityOrigin();
    auto* contentSecurityPolicy = globalScope.contentSecurityPolicy();

    ASSERT(securityOrigin);
    ASSERT(contentSecurityPolicy);

    auto securityOriginCopy = securityOrigin->isolatedCopy();
    auto contentSecurityPolicyCopy = makeUnique<ContentSecurityPolicy>(globalScope.url().isolatedCopy());
    contentSecurityPolicyCopy->copyStateFrom(contentSecurityPolicy);
    contentSecurityPolicyCopy->copyUpgradeInsecureRequestStateFrom(*contentSecurityPolicy);

    auto optionsCopy = makeUnique<LoaderTaskOptions>(options, request.httpReferrer().isNull() ? outgoingReferrer : request.httpReferrer(), WTFMove(securityOriginCopy));

    // All loads start out as Document. Inside WorkerThreadableLoader we upgrade this to a Worker load.
    ASSERT(optionsCopy->options.initiatorContext == InitiatorContext::Document);
    optionsCopy->options.initiatorContext = InitiatorContext::Worker;

#if ENABLE(SERVICE_WORKER)
    optionsCopy->options.serviceWorkersMode = is<ServiceWorkerGlobalScope>(globalScope) ? ServiceWorkersMode::None : ServiceWorkersMode::All;
    if (auto* activeServiceWorker = globalScope.activeServiceWorker())
        optionsCopy->options.serviceWorkerRegistrationIdentifier = activeServiceWorker->registrationIdentifier();
#endif

    if (is<WorkerGlobalScope>(globalScope))
        InspectorInstrumentation::willSendRequest(downcast<WorkerGlobalScope>(globalScope), m_workerRequestIdentifier, request);

    // Can we benefit from request being an r-value to create more efficiently its isolated copy?
    m_loaderProxy.postTaskToLoader([this, request = request.isolatedCopy(), options = WTFMove(optionsCopy), contentSecurityPolicyCopy = WTFMove(contentSecurityPolicyCopy)](ScriptExecutionContext& context) mutable {
        ASSERT(isMainThread());
        Document& document = downcast<Document>(context);

        // FIXME: If the site requests a local resource, then this will return a non-zero value but the sync path will return a 0 value.
        // Either this should return 0 or the other code path should call a failure callback.
        m_mainThreadLoader = DocumentThreadableLoader::create(document, *this, WTFMove(request), options->options, WTFMove(options->origin), WTFMove(contentSecurityPolicyCopy), WTFMove(options->referrer), DocumentThreadableLoader::ShouldLogError::No);
        ASSERT(m_mainThreadLoader || m_loadingFinished);
    });
}

void WorkerThreadableLoader::MainThreadBridge::destroy()
{
    // Ensure that no more client callbacks are done in the worker context's thread.
    clearClientWrapper();

    // "delete this" and m_mainThreadLoader::deref() on the worker object's thread.
    m_loaderProxy.postTaskToLoader([self = std::unique_ptr<WorkerThreadableLoader::MainThreadBridge>(this)] (ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        if (self->m_mainThreadLoader)
            self->m_mainThreadLoader->clearClient();
    });
}

void WorkerThreadableLoader::MainThreadBridge::cancel()
{
    m_loaderProxy.postTaskToLoader([this] (ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());

        if (!m_mainThreadLoader)
            return;
        m_mainThreadLoader->cancel();
        m_mainThreadLoader = nullptr;
    });

    // Taking a ref of client wrapper as call to didFail may take out the last reference of it.
    Ref<ThreadableLoaderClientWrapper> protectedWorkerClientWrapper(*m_workerClientWrapper);
    // If the client hasn't reached a termination state, then transition it by sending a cancellation error.
    // Note: no more client callbacks will be done after this method -- we clear the client wrapper to ensure that.
    ResourceError error(ResourceError::Type::Cancellation);
    protectedWorkerClientWrapper->didFail(error);
    protectedWorkerClientWrapper->clearClient();
}

void WorkerThreadableLoader::MainThreadBridge::computeIsDone()
{
    m_loaderProxy.postTaskToLoader([this](auto&) {
        if (!m_mainThreadLoader) {
            notifyIsDone(true);
            return;
        }
        m_mainThreadLoader->computeIsDone();
    });
}

void WorkerThreadableLoader::MainThreadBridge::notifyIsDone(bool isDone)
{
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), isDone](auto&) {
        protectedWorkerClientWrapper->notifyIsDone(isDone);
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::clearClientWrapper()
{
    m_workerClientWrapper->clearClient();
}

void WorkerThreadableLoader::MainThreadBridge::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), bytesSent, totalBytesToBeSent] (ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        protectedWorkerClientWrapper->didSendData(bytesSent, totalBytesToBeSent);
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), workerRequestIdentifier = m_workerRequestIdentifier, identifier, responseData = response.crossThreadData()] (ScriptExecutionContext& context) mutable {
        ASSERT(context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        auto response = ResourceResponse::fromCrossThreadData(WTFMove(responseData));
        protectedWorkerClientWrapper->didReceiveResponse(identifier, response);
        if (is<WorkerGlobalScope>(context))
            InspectorInstrumentation::didReceiveResourceResponse(downcast<WorkerGlobalScope>(context), workerRequestIdentifier, response);
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::didReceiveData(const char* data, int dataLength)
{
    Vector<char> buffer(dataLength);
    memcpy(buffer.data(), data, dataLength);
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), workerRequestIdentifier = m_workerRequestIdentifier, buffer = WTFMove(buffer)] (ScriptExecutionContext& context) mutable {
        ASSERT(context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        protectedWorkerClientWrapper->didReceiveData(buffer.data(), buffer.size());
        if (is<WorkerGlobalScope>(context))
            InspectorInstrumentation::didReceiveData(downcast<WorkerGlobalScope>(context), workerRequestIdentifier, buffer.data(), buffer.size());
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::didFinishLoading(unsigned long identifier)
{
    m_loadingFinished = true;
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), workerRequestIdentifier = m_workerRequestIdentifier, networkLoadMetrics = m_networkLoadMetrics.isolatedCopy(), identifier] (ScriptExecutionContext& context) mutable {
        ASSERT(context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        protectedWorkerClientWrapper->didFinishLoading(identifier);
        if (is<WorkerGlobalScope>(context))
            InspectorInstrumentation::didFinishLoading(downcast<WorkerGlobalScope>(context), workerRequestIdentifier, networkLoadMetrics);
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::didFail(const ResourceError& error)
{
    m_loadingFinished = true;
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), workerRequestIdentifier = m_workerRequestIdentifier, error = error.isolatedCopy()] (ScriptExecutionContext& context) mutable {
        ASSERT(context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        ThreadableLoader::logError(context, error, protectedWorkerClientWrapper->initiator());
        protectedWorkerClientWrapper->didFail(error);
        if (is<WorkerGlobalScope>(context))
            InspectorInstrumentation::didFailLoading(downcast<WorkerGlobalScope>(context), workerRequestIdentifier, error);
    }, m_taskMode);
}

void WorkerThreadableLoader::MainThreadBridge::didFinishTiming(const ResourceTiming& resourceTiming)
{
    m_networkLoadMetrics = resourceTiming.networkLoadMetrics();
    m_loaderProxy.postTaskForModeToWorkerOrWorkletGlobalScope([protectedWorkerClientWrapper = makeRef(*m_workerClientWrapper), resourceTiming = resourceTiming.isolatedCopy()] (ScriptExecutionContext& context) mutable {
        ASSERT(context.isWorkerGlobalScope() || context.isWorkletGlobalScope());
        ASSERT(!resourceTiming.initiator().isEmpty());

        // No need to notify clients, just add the performance timing entry.
        if (is<WorkerGlobalScope>(context))
            downcast<WorkerGlobalScope>(context).performance().addResourceTiming(WTFMove(resourceTiming));
    }, m_taskMode);
}

} // namespace WebCore
