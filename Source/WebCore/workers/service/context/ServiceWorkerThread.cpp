/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerThread.h"

#if ENABLE(SERVICE_WORKER)

#include "CacheStorageProvider.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "ExtendableMessageEvent.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerFetch.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerWindowClient.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <pal/SessionID.h>
#include <runtime/RuntimeFlags.h>
#include <wtf/NeverDestroyed.h>

using namespace PAL;

namespace WebCore {

class DummyServiceWorkerThreadProxy : public WorkerObjectProxy {
public:
    static DummyServiceWorkerThreadProxy& shared()
    {
        static NeverDestroyed<DummyServiceWorkerThreadProxy> proxy;
        return proxy;
    }

private:
    void postExceptionToWorkerObject(const String&, int, int, const String&) final { };
    void postMessageToPageInspector(const String&) final { };
    void workerGlobalScopeDestroyed() final { };
    void postMessageToWorkerObject(Ref<SerializedScriptValue>&&, std::unique_ptr<MessagePortChannelArray>&&) final { };
    void confirmMessageFromWorkerObject(bool) final { };
    void reportPendingActivity(bool) final { };
};

// FIXME: Use a valid WorkerReportingProxy
// FIXME: Use a valid WorkerObjectProxy
// FIXME: Use a valid IDBConnection
// FIXME: Use a valid SocketProvider
// FIXME: Use a valid user agent
// FIXME: Use valid runtime flags

ServiceWorkerThread::ServiceWorkerThread(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data, PAL::SessionID, WorkerLoaderProxy& loaderProxy)
    : WorkerThread(data.scriptURL, data.workerID, ASCIILiteral("WorkerUserAgent"), data.script, loaderProxy, DummyServiceWorkerThreadProxy::shared(), WorkerThreadStartMode::Normal, ContentSecurityPolicyResponseHeaders { }, false, SecurityOrigin::create(data.scriptURL).get(), MonotonicTime::now(), nullptr, nullptr, JSC::RuntimeFlags::createAllEnabled(), SessionID::defaultSessionID())
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
    , m_data(data.isolatedCopy())
    , m_workerObjectProxy(DummyServiceWorkerThreadProxy::shared())
{
    AtomicString::init();
}

ServiceWorkerThread::~ServiceWorkerThread() = default;

Ref<WorkerGlobalScope> ServiceWorkerThread::createWorkerGlobalScope(const URL& url, const String& identifier, const String& userAgent, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, PAL::SessionID sessionID)
{
    return ServiceWorkerGlobalScope::create(m_serverConnectionIdentifier, m_data, url, identifier, userAgent, *this, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, idbConnectionProxy(), socketProvider(), sessionID);
}

void ServiceWorkerThread::runEventLoop()
{
    // FIXME: There will be ServiceWorker specific things to do here.
    WorkerThread::runEventLoop();
}

void ServiceWorkerThread::postFetchTask(Ref<ServiceWorkerFetch::Client>&& client, ResourceRequest&& request, FetchOptions&& options)
{
    // FIXME: instead of directly using runLoop(), we should be using something like WorkerGlobalScopeProxy.
    // FIXME: request and options come straigth from IPC so are already isolated. We should be able to take benefit of that.
    runLoop().postTaskForMode([client = WTFMove(client), request = request.isolatedCopy(), options = options.isolatedCopy()] (ScriptExecutionContext& context) mutable {
        ServiceWorkerFetch::dispatchFetchEvent(WTFMove(client), downcast<WorkerGlobalScope>(context), WTFMove(request), WTFMove(options));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThread::postMessageToServiceWorkerGlobalScope(Ref<SerializedScriptValue>&& message, std::unique_ptr<MessagePortChannelArray>&& channels, const ServiceWorkerClientIdentifier& sourceIdentifier, const String& sourceOrigin)
{
    ScriptExecutionContext::Task task([channels = WTFMove(channels), message = WTFMove(message), sourceIdentifier, sourceOrigin = sourceOrigin.isolatedCopy()] (ScriptExecutionContext& context) mutable {
        auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(context);
        auto ports = MessagePort::entanglePorts(serviceWorkerGlobalScope, WTFMove(channels));
        ExtendableMessageEventSource source = RefPtr<ServiceWorkerClient> { ServiceWorkerWindowClient::create(context, sourceIdentifier) };
        serviceWorkerGlobalScope.dispatchEvent(ExtendableMessageEvent::create(WTFMove(ports), WTFMove(message), sourceOrigin, { }, WTFMove(source)));
        serviceWorkerGlobalScope.thread().workerObjectProxy().confirmMessageFromWorkerObject(serviceWorkerGlobalScope.hasPendingActivity());
    });
    runLoop().postTask(WTFMove(task));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
