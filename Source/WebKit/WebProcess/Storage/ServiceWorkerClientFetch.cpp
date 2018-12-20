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
#include "WebSWClientConnection.h"
#include "WebServiceWorkerProvider.h"
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {
using namespace WebCore;

Ref<ServiceWorkerClientFetch> ServiceWorkerClientFetch::create(WebServiceWorkerProvider& serviceWorkerProvider, Ref<WebCore::ResourceLoader>&& loader, FetchIdentifier identifier, Ref<WebSWClientConnection>&& connection, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Callback&& callback)
{
    auto fetch = adoptRef(*new ServiceWorkerClientFetch { serviceWorkerProvider, WTFMove(loader), identifier, WTFMove(connection), shouldClearReferrerOnHTTPSToHTTPRedirect, WTFMove(callback) });
    fetch->start();
    return fetch;
}

ServiceWorkerClientFetch::~ServiceWorkerClientFetch()
{
}

ServiceWorkerClientFetch::ServiceWorkerClientFetch(WebServiceWorkerProvider& serviceWorkerProvider, Ref<WebCore::ResourceLoader>&& loader, FetchIdentifier identifier, Ref<WebSWClientConnection>&& connection, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Callback&& callback)
    : m_serviceWorkerProvider(serviceWorkerProvider)
    , m_loader(WTFMove(loader))
    , m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_callback(WTFMove(callback))
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
{
}

void ServiceWorkerClientFetch::start()
{
    auto request = m_loader->request();
    auto& options = m_loader->options();

    auto referrer = request.httpReferrer();

    m_didFail = false;
    m_didFinish = false;

    // We are intercepting fetch calls after going through the HTTP layer, which may add some specific headers.
    cleanHTTPRequestHeadersForAccessControl(request, options.httpHeadersToKeep);

    ASSERT(options.serviceWorkersMode != ServiceWorkersMode::None);
    m_serviceWorkerRegistrationIdentifier = options.serviceWorkerRegistrationIdentifier.value();
    m_connection->startFetch(m_identifier, m_serviceWorkerRegistrationIdentifier, request, options, referrer);

    m_redirectionStatus = RedirectionStatus::None;
}

// https://fetch.spec.whatwg.org/#http-fetch step 3.3
Optional<ResourceError> ServiceWorkerClientFetch::validateResponse(const ResourceResponse& response)
{
    // FIXME: make a better error reporting.
    if (response.type() == ResourceResponse::Type::Error)
        return ResourceError { ResourceError::Type::General };

    auto& options = m_loader->options();
    if (options.mode != FetchOptions::Mode::NoCors && response.tainting() == ResourceResponse::Tainting::Opaque)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker is opaque"_s, ResourceError::Type::AccessControl };

    // Navigate mode induces manual redirect.
    if (options.redirect != FetchOptions::Redirect::Manual && options.mode != FetchOptions::Mode::Navigate && response.tainting() == ResourceResponse::Tainting::Opaqueredirect)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker is opaque redirect"_s, ResourceError::Type::AccessControl };

    if ((options.redirect != FetchOptions::Redirect::Follow || options.mode == FetchOptions::Mode::Navigate) && response.isRedirected())
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Response served by service worker has redirections"_s, ResourceError::Type::AccessControl };

    return WTF::nullopt;
}

void ServiceWorkerClientFetch::didReceiveResponse(ResourceResponse&& response)
{
    m_isCheckingResponse = true;
    callOnMainThread([this, protectedThis = makeRef(*this), response = WTFMove(response)]() mutable {
        if (!m_loader) {
            m_isCheckingResponse = false;
            return;
        }

        if (auto error = validateResponse(response)) {
            m_isCheckingResponse = false;
            m_loader->didFail(error.value());
            ASSERT(!m_loader);
            if (auto callback = WTFMove(m_callback))
                callback(Result::Succeeded);
            return;
        }
        response.setSource(ResourceResponse::Source::ServiceWorker);

        if (response.isRedirection() && response.httpHeaderFields().contains(HTTPHeaderName::Location)) {
            m_isCheckingResponse = false;
            continueLoadingAfterCheckingResponse();
            m_redirectionStatus = RedirectionStatus::Receiving;
            m_loader->willSendRequest(m_loader->request().redirectedRequest(response, m_shouldClearReferrerOnHTTPSToHTTPRedirect), response, [protectedThis = makeRef(*this), this](ResourceRequest&& request) {
                if (request.isNull() || !m_callback)
                    return;

                ASSERT(request == m_loader->request());
                if (m_redirectionStatus == RedirectionStatus::Received) {
                    start();
                    return;
                }
                m_redirectionStatus = RedirectionStatus::Following;
            });
            return;
        }

        // In case of main resource and mime type is the default one, we set it to text/html to pass more service worker WPT tests.
        // FIXME: We should refine our MIME type sniffing strategy for synthetic responses.
        if (m_loader->originalRequest().requester() == ResourceRequest::Requester::Main) {
            if (response.mimeType() == defaultMIMEType()) {
                response.setMimeType("text/html"_s);
                response.setTextEncodingName("UTF-8"_s);
            }
        }

        // As per https://fetch.spec.whatwg.org/#main-fetch step 9, copy request's url list in response's url list if empty.
        if (response.url().isNull())
            response.setURL(m_loader->request().url());

        m_loader->didReceiveResponse(response, [this, protectedThis = WTFMove(protectedThis)] {
            m_isCheckingResponse = false;
            continueLoadingAfterCheckingResponse();
            if (auto callback = WTFMove(m_callback))
                callback(Result::Succeeded);
        });
    });
}

void ServiceWorkerClientFetch::didReceiveData(const IPC::DataReference& dataReference, int64_t encodedDataLength)
{
    auto* data = reinterpret_cast<const char*>(dataReference.data());
    if (!m_buffer) {
        m_buffer = SharedBuffer::create(data, dataReference.size());
        m_encodedDataLength = encodedDataLength;
    } else {
        m_buffer->append(data, dataReference.size());
        m_encodedDataLength += encodedDataLength;
    }

    if (m_isCheckingResponse)
        return;

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        if (!m_loader || !m_encodedDataLength)
            return;

        m_loader->didReceiveBuffer(m_buffer.releaseNonNull(), m_encodedDataLength, DataPayloadBytes);
        m_encodedDataLength = 0;
    });
}

void ServiceWorkerClientFetch::didReceiveFormData(const IPC::FormDataReference&)
{
    // FIXME: Implement form data reading.
}

void ServiceWorkerClientFetch::didFinish()
{
    m_didFinish = true;

    if (m_isCheckingResponse)
        return;

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        if (!m_loader)
            return;

        switch (m_redirectionStatus) {
        case RedirectionStatus::None:
            break;
        case RedirectionStatus::Receiving:
            m_redirectionStatus = RedirectionStatus::Received;
            return;
        case RedirectionStatus::Following:
            start();
            return;
        case RedirectionStatus::Received:
            ASSERT_NOT_REACHED();
            m_redirectionStatus = RedirectionStatus::None;
        }

        ASSERT(!m_callback);

        m_loader->didFinishLoading(NetworkLoadMetrics { });
        m_serviceWorkerProvider.fetchFinished(m_identifier);
    });
}

void ServiceWorkerClientFetch::didFail(ResourceError&& error)
{
    m_didFail = true;
    m_error = WTFMove(error);

    if (m_isCheckingResponse)
        return;

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        if (!m_loader)
            return;

        auto* document = m_loader->frame() ? m_loader->frame()->document() : nullptr;
        if (document) {
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, m_error.localizedDescription());
            if (m_loader->options().destination != FetchOptions::Destination::EmptyString)
                document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Cannot load ", m_error.failingURL().string(), "."));
        }

        m_loader->didFail(m_error);

        if (auto callback = WTFMove(m_callback))
            callback(Result::Succeeded);

        m_serviceWorkerProvider.fetchFinished(m_identifier);
    });
}

void ServiceWorkerClientFetch::didNotHandle()
{
    ASSERT(!m_isCheckingResponse);

    callOnMainThread([this, protectedThis = makeRef(*this)] {
        if (!m_loader)
            return;

        if (auto callback = WTFMove(m_callback))
            callback(Result::Unhandled);

        m_serviceWorkerProvider.fetchFinished(m_identifier);
    });
}

void ServiceWorkerClientFetch::cancel()
{
    if (auto callback = WTFMove(m_callback))
        callback(Result::Cancelled);

    if (!m_didFinish && !m_didFail) {
        m_connection->cancelFetch(m_identifier, m_serviceWorkerRegistrationIdentifier);

    }
    m_loader = nullptr;
    m_buffer = nullptr;
}

void ServiceWorkerClientFetch::continueLoadingAfterCheckingResponse()
{
    ASSERT(!m_isCheckingResponse);
    if (!m_loader)
        return;

    if (m_encodedDataLength) {
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            if (!m_loader || !m_encodedDataLength)
                return;
            m_loader->didReceiveBuffer(m_buffer.releaseNonNull(), m_encodedDataLength, DataPayloadBytes);
            m_encodedDataLength = 0;
        });
    }

    if (m_didFail) {
        didFail(WTFMove(m_error));
        return;
    }

    if (m_didFinish)
        didFinish();
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
