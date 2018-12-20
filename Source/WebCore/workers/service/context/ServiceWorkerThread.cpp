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
#include "EventNames.h"
#include "ExtendableMessageEvent.h"
#include "JSDOMPromise.h"
#include "LoaderStrategy.h"
#include "PlatformStrategies.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerFetch.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerWindowClient.h"
#include "WorkerDebuggerProxy.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <pal/SessionID.h>
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
    void workerGlobalScopeDestroyed() final { };
    void postMessageToWorkerObject(MessageWithMessagePorts&&) final { };
    void confirmMessageFromWorkerObject(bool) final { };
    void reportPendingActivity(bool) final { };
};

// FIXME: Use a valid WorkerReportingProxy
// FIXME: Use a valid WorkerObjectProxy
// FIXME: Use valid runtime flags

ServiceWorkerThread::ServiceWorkerThread(const ServiceWorkerContextData& data, PAL::SessionID, String&& userAgent, WorkerLoaderProxy& loaderProxy, WorkerDebuggerProxy& debuggerProxy, IDBClient::IDBConnectionProxy* idbConnectionProxy, SocketProvider* socketProvider)
    : WorkerThread(data.scriptURL, emptyString(), "serviceworker:" + Inspector::IdentifiersFactory::createIdentifier(), WTFMove(userAgent), platformStrategies()->loaderStrategy()->isOnLine(), data.script, loaderProxy, debuggerProxy, DummyServiceWorkerThreadProxy::shared(), WorkerThreadStartMode::Normal, data.contentSecurityPolicy, false, data.registration.key.topOrigin().securityOrigin().get(), MonotonicTime::now(), idbConnectionProxy, socketProvider, JSC::RuntimeFlags::createAllEnabled(), data.sessionID)
    , m_data(data.isolatedCopy())
    , m_workerObjectProxy(DummyServiceWorkerThreadProxy::shared())
{
    AtomicString::init();
}

ServiceWorkerThread::~ServiceWorkerThread() = default;

Ref<WorkerGlobalScope> ServiceWorkerThread::createWorkerGlobalScope(const URL& url, Ref<SecurityOrigin>&& origin, const String& name, const String& identifier, const String& userAgent, bool isOnline, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, PAL::SessionID sessionID)
{
    UNUSED_PARAM(name);
    return ServiceWorkerGlobalScope::create(m_data, url, WTFMove(origin), identifier, userAgent, isOnline, *this, contentSecurityPolicy, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, idbConnectionProxy(), socketProvider(), sessionID);
}

void ServiceWorkerThread::runEventLoop()
{
    // FIXME: There will be ServiceWorker specific things to do here.
    WorkerThread::runEventLoop();
}

void ServiceWorkerThread::postFetchTask(Ref<ServiceWorkerFetch::Client>&& client, Optional<ServiceWorkerClientIdentifier>&& clientId, ResourceRequest&& request, String&& referrer, FetchOptions&& options)
{
    // FIXME: instead of directly using runLoop(), we should be using something like WorkerGlobalScopeProxy.
    // FIXME: request and options come straigth from IPC so are already isolated. We should be able to take benefit of that.
    runLoop().postTaskForMode([client = WTFMove(client), clientId, request = request.isolatedCopy(), referrer = referrer.isolatedCopy(), options = options.isolatedCopy()] (ScriptExecutionContext& context) mutable {
        context.postTask([client = WTFMove(client), clientId, request = WTFMove(request), referrer = WTFMove(referrer), options = WTFMove(options)] (ScriptExecutionContext& context) mutable {
            ServiceWorkerFetch::dispatchFetchEvent(WTFMove(client), downcast<ServiceWorkerGlobalScope>(context), clientId, WTFMove(request), WTFMove(referrer), WTFMove(options));
        });
    }, WorkerRunLoop::defaultMode());
}

static void fireMessageEvent(ServiceWorkerGlobalScope& scope, MessageWithMessagePorts&& message, ExtendableMessageEventSource&& source, const URL& sourceURL)
{
    auto ports = MessagePort::entanglePorts(scope, WTFMove(message.transferredPorts));
    auto messageEvent = ExtendableMessageEvent::create(WTFMove(ports), WTFMove(message.message), SecurityOriginData::fromURL(sourceURL).toString(), { }, source);
    scope.dispatchEvent(messageEvent);
    scope.thread().workerObjectProxy().confirmMessageFromWorkerObject(scope.hasPendingActivity());
    scope.updateExtendedEventsSet(messageEvent.ptr());
}

void ServiceWorkerThread::postMessageToServiceWorker(MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    runLoop().postTask([message = WTFMove(message), sourceData = WTFMove(sourceData)] (auto& context) mutable {
        auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(context);
        URL sourceURL;
        ExtendableMessageEventSource source;
        if (WTF::holds_alternative<ServiceWorkerClientData>(sourceData)) {
            RefPtr<ServiceWorkerClient> sourceClient = ServiceWorkerClient::getOrCreate(serviceWorkerGlobalScope, WTFMove(WTF::get<ServiceWorkerClientData>(sourceData)));

            RELEASE_ASSERT(!sourceClient->url().protocolIsInHTTPFamily() || !serviceWorkerGlobalScope.url().protocolIsInHTTPFamily() || protocolHostAndPortAreEqual(serviceWorkerGlobalScope.url(), sourceClient->url()));

            sourceURL = sourceClient->url();
            source = WTFMove(sourceClient);
        } else {
            RefPtr<ServiceWorker> sourceWorker = ServiceWorker::getOrCreate(serviceWorkerGlobalScope, WTFMove(WTF::get<ServiceWorkerData>(sourceData)));

            RELEASE_ASSERT(!sourceWorker->scriptURL().protocolIsInHTTPFamily() || !serviceWorkerGlobalScope.url().protocolIsInHTTPFamily() || protocolHostAndPortAreEqual(serviceWorkerGlobalScope.url(), sourceWorker->scriptURL()));

            sourceURL = sourceWorker->scriptURL();
            source = WTFMove(sourceWorker);
        }
        fireMessageEvent(serviceWorkerGlobalScope, WTFMove(message), ExtendableMessageEventSource { source }, sourceURL);
    });
}

void ServiceWorkerThread::fireInstallEvent()
{
    ScriptExecutionContext::Task task([jobDataIdentifier = m_data.jobDataIdentifier, serviceWorkerIdentifier = this->identifier()] (ScriptExecutionContext& context) mutable {
        context.postTask([jobDataIdentifier, serviceWorkerIdentifier](ScriptExecutionContext& context) {
            auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(context);
            auto installEvent = ExtendableEvent::create(eventNames().installEvent, { }, ExtendableEvent::IsTrusted::Yes);
            serviceWorkerGlobalScope.dispatchEvent(installEvent);

            installEvent->whenAllExtendLifetimePromisesAreSettled([jobDataIdentifier, serviceWorkerIdentifier](HashSet<Ref<DOMPromise>>&& extendLifetimePromises) {
                bool hasRejectedAnyPromise = false;
                for (auto& promise : extendLifetimePromises) {
                    if (promise->status() == DOMPromise::Status::Rejected) {
                        hasRejectedAnyPromise = true;
                        break;
                    }
                }
                callOnMainThread([jobDataIdentifier, serviceWorkerIdentifier, hasRejectedAnyPromise] () mutable {
                    if (auto* connection = SWContextManager::singleton().connection())
                        connection->didFinishInstall(jobDataIdentifier, serviceWorkerIdentifier, !hasRejectedAnyPromise);
                });
            });
        });
    });
    runLoop().postTask(WTFMove(task));
}

void ServiceWorkerThread::fireActivateEvent()
{
    ScriptExecutionContext::Task task([serviceWorkerIdentifier = this->identifier()] (ScriptExecutionContext& context) mutable {
        context.postTask([serviceWorkerIdentifier](ScriptExecutionContext& context) {
            auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(context);
            auto activateEvent = ExtendableEvent::create(eventNames().activateEvent, { }, ExtendableEvent::IsTrusted::Yes);
            serviceWorkerGlobalScope.dispatchEvent(activateEvent);

            activateEvent->whenAllExtendLifetimePromisesAreSettled([serviceWorkerIdentifier](HashSet<Ref<DOMPromise>>&&) {
                callOnMainThread([serviceWorkerIdentifier] () mutable {
                    if (auto* connection = SWContextManager::singleton().connection())
                        connection->didFinishActivation(serviceWorkerIdentifier);
                });
            });
        });
    });
    runLoop().postTask(WTFMove(task));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
