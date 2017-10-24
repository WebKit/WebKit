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
#include "ServiceWorkerContextManager.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "Logging.h"
#include "StorageProcessMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebServiceWorkerFetchTaskClient.h"
#include <WebCore/MessagePortChannel.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SerializedScriptValue.h>
#include <pal/SessionID.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

ServiceWorkerContextManager::ServiceWorkerContextManager(Ref<IPC::Connection>&& connection, const WebPreferencesStore& store)
    : m_connectionToStorageProcess(WTFMove(connection))
{
    updatePreferences(store);
}

void ServiceWorkerContextManager::updatePreferences(const WebPreferencesStore& store)
{
    RuntimeEnabledFeatures::sharedFeatures().setCacheAPIEnabled(store.getBoolValueForKey(WebPreferencesKey::cacheAPIEnabledKey()));
    RuntimeEnabledFeatures::sharedFeatures().setFetchAPIEnabled(store.getBoolValueForKey(WebPreferencesKey::fetchAPIEnabledKey()));
}

void ServiceWorkerContextManager::startServiceWorker(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data)
{
    // FIXME: Provide a sensical session ID.
    auto serviceWorker = ServiceWorkerThreadProxy::create(serverConnectionIdentifier, data, SessionID::defaultSessionID(), WebProcess::singleton().cacheStorageProvider());
    auto serviceWorkerIdentifier = serviceWorker->identifier();
    auto result = m_workerMap.add(serviceWorkerIdentifier, WTFMove(serviceWorker));
    ASSERT_UNUSED(result, result.isNewEntry);

    LOG(ServiceWorker, "Context process PID: %i started worker thread %s\n", getpid(), data.workerID.utf8().data());

    m_connectionToStorageProcess->send(Messages::StorageProcess::ServiceWorkerContextStarted(serverConnectionIdentifier, data.registrationKey, serviceWorkerIdentifier, data.workerID), 0);
}

void ServiceWorkerContextManager::startFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier, uint64_t serviceWorkerIdentifier, ResourceRequest&& request, FetchOptions&& options)
{
    auto serviceWorker = m_workerMap.get(serviceWorkerIdentifier);
    if (!serviceWorker) {
        m_connectionToStorageProcess->send(Messages::StorageProcess::DidNotHandleFetch(serverConnectionIdentifier, fetchIdentifier), 0);
        return;
    }

    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToStorageProcess.copyRef(), serverConnectionIdentifier, fetchIdentifier);
    serviceWorker->thread().postFetchTask(WTFMove(client), WTFMove(request), WTFMove(options));
}

void ServiceWorkerContextManager::postMessageToServiceWorkerGlobalScope(uint64_t serverConnectionIdentifier, uint64_t serviceWorkerIdentifier, const IPC::DataReference& message, const String& sourceOrigin)
{
    auto* serviceWorker = m_workerMap.get(serviceWorkerIdentifier);
    if (!serviceWorker)
        return;

    // FIXME: We should pass valid MessagePortChannels.
    serviceWorker->thread().postMessageToServiceWorkerGlobalScope(SerializedScriptValue::adopt(message.vector()), nullptr, sourceOrigin);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
