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
#include "ServiceWorkerClientFetch.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "WebServiceWorkerProvider.h"
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>

using namespace WebCore;

namespace WebKit {

ServiceWorkerClientFetch::ServiceWorkerClientFetch(WebServiceWorkerProvider& serviceWorkerProvider, Ref<WebCore::ResourceLoader>&& loader, uint64_t identifier, Ref<IPC::Connection>&& connection, Callback&& callback)
    : m_serviceWorkerProvider(serviceWorkerProvider)
    , m_loader(WTFMove(loader))
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_callback(WTFMove(callback))
{
}

void ServiceWorkerClientFetch::didReceiveResponse(WebCore::ResourceResponse&& response)
{
    auto protectedThis = makeRef(*this);

    if (!(response.httpStatusCode() <= 300 || response.httpStatusCode() >= 400 || response.httpStatusCode() == 304 || response.httpStatusCode() == 305 || response.httpStatusCode() == 306)) {
        // FIXME: Support redirections.
        notImplemented();
        m_loader->didFail({ ResourceError::Type::General });
        if (auto callback = WTFMove(m_callback))
            callback(Result::Succeeded);
        return;
    }

    if (response.type() == ResourceResponse::Type::Error) {
        // Add support for a better error.
        m_loader->didFail({ ResourceError::Type::General });
        if (auto callback = WTFMove(m_callback))
            callback(Result::Succeeded);
        return;
    }

    // In case of main resource and mime type is the default one, we set it to text/html to pass more service worker WPT tests.
    // FIXME: We should refine our MIME type sniffing strategy for synthetic responses.
    if (m_loader->originalRequest().requester() == ResourceRequest::Requester::Main) {
        if (response.mimeType() == defaultMIMEType())
            response.setMimeType(ASCIILiteral("text/html"));
    }
    response.setSource(ResourceResponse::Source::ServiceWorker);
    m_loader->didReceiveResponse(response);
    if (auto callback = WTFMove(m_callback))
        callback(Result::Succeeded);
}

void ServiceWorkerClientFetch::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    m_loader->didReceiveData(reinterpret_cast<const char*>(data.data()), data.size(), encodedDataLength, DataPayloadBytes);
}

void ServiceWorkerClientFetch::didReceiveFormData(const IPC::FormDataReference&)
{
    // FIXME: Implement form data reading.
}

void ServiceWorkerClientFetch::didFinish()
{
    ASSERT(!m_callback);

    auto protectedThis = makeRef(*this);
    m_loader->didFinishLoading(NetworkLoadMetrics { });
    m_serviceWorkerProvider.fetchFinished(m_identifier);
}

void ServiceWorkerClientFetch::didFail()
{
    auto protectedThis = makeRef(*this);
    m_loader->didFail({ ResourceError::Type::General });

    if (auto callback = WTFMove(m_callback))
        callback(Result::Succeeded);

    m_serviceWorkerProvider.fetchFinished(m_identifier);
}

void ServiceWorkerClientFetch::didNotHandle()
{
    if (auto callback = WTFMove(m_callback))
        callback(Result::Unhandled);

    m_serviceWorkerProvider.fetchFinished(m_identifier);
}

void ServiceWorkerClientFetch::cancel()
{
    if (auto callback = WTFMove(m_callback))
        callback(Result::Cancelled);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
