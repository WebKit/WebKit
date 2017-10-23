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

#include "Logging.h"
#include "StorageProcessMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebServiceWorkerFetchTaskClient.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <pal/SessionID.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

void ServiceWorkerContextManager::startServiceWorker(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data)
{
    // FIXME: Provide a sensical session ID.
    auto thread = ServiceWorkerThread::create(serverConnectionIdentifier, data, SessionID::defaultSessionID());
    auto threadIdentifier = thread->identifier();
    auto result = m_workerThreadMap.add(threadIdentifier, WTFMove(thread));
    ASSERT(result.isNewEntry);

    result.iterator->value->start();

    LOG(ServiceWorker, "Context process PID: %i started worker thread %s\n", getpid(), data.workerID.utf8().data());

    m_connectionToStorageProcess->send(Messages::StorageProcess::ServiceWorkerContextStarted(serverConnectionIdentifier, data.registrationKey, threadIdentifier, data.workerID), 0);
}

void ServiceWorkerContextManager::startFetch(uint64_t serverConnectionIdentifier, uint64_t fetchIdentifier, uint64_t serviceWorkerIdentifier, ResourceRequest&& request, FetchOptions&& options)
{
    auto serviceWorkerThread = m_workerThreadMap.get(serviceWorkerIdentifier);
    if (!serviceWorkerThread) {
        m_connectionToStorageProcess->send(Messages::StorageProcess::DidNotHandleFetch(serverConnectionIdentifier, fetchIdentifier), 0);
        return;
    }

    auto client = WebServiceWorkerFetchTaskClient::create(m_connectionToStorageProcess.copyRef(), serverConnectionIdentifier, fetchIdentifier);
    serviceWorkerThread->postFetchTask(WTFMove(client), WTFMove(request), WTFMove(options));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
