/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ServiceWorkerFetchTask.h"

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "ServiceWorkerClientFetchMessages.h"
#include "WebCoreArgumentCoders.h"

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerFetchTask::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - ServiceWorkerFetchTask::" fmt, this, ##__VA_ARGS__)

using namespace WebCore;

namespace WebKit {

void ServiceWorkerFetchTask::didReceiveRedirectResponse(const ResourceResponse& response)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveRedirectResponse: %s", m_identifier.fetchIdentifier.loggingString().utf8().data());
    m_connection->send(Messages::ServiceWorkerClientFetch::DidReceiveRedirectResponse { response }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didReceiveResponse(const ResourceResponse& response, bool needsContinueDidReceiveResponseMessage)
{
    m_connection->send(Messages::ServiceWorkerClientFetch::DidReceiveResponse { response, needsContinueDidReceiveResponseMessage }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    m_connection->send(Messages::ServiceWorkerClientFetch::DidReceiveData { data, encodedDataLength }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didReceiveFormData(const IPC::FormDataReference& formData)
{
    m_connection->send(Messages::ServiceWorkerClientFetch::DidReceiveFormData { formData }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didFinish()
{
    RELEASE_LOG_IF_ALLOWED("didFinishFetch: fetchIdentifier: %s", m_identifier.fetchIdentifier.loggingString().utf8().data());
    m_connection->send(Messages::ServiceWorkerClientFetch::DidFinish { }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didFail(const ResourceError& error)
{
    RELEASE_LOG_ERROR_IF_ALLOWED("didFailFetch: fetchIdentifier: %s", m_identifier.fetchIdentifier.loggingString().utf8().data());
    m_connection->send(Messages::ServiceWorkerClientFetch::DidFail { error }, m_identifier.fetchIdentifier);
}

void ServiceWorkerFetchTask::didNotHandle()
{
    RELEASE_LOG_IF_ALLOWED("didNotHandleFetch: fetchIdentifier: %s", m_identifier.fetchIdentifier.loggingString().utf8().data());
    m_connection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, m_identifier.fetchIdentifier);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
