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
#include "ServiceWorkerSoftUpdateLoader.h"

#if ENABLE(SERVICE_WORKER)

#include "Logging.h"
#include "NetworkCache.h"
#include "NetworkLoad.h"
#include "NetworkSession.h"
#include <WebCore/ServiceWorkerFetchResult.h>
#include <WebCore/ServiceWorkerJob.h>
#include <WebCore/TextResourceDecoder.h>
#include <WebCore/WorkerScriptLoader.h>

namespace WebKit {

using namespace WebCore;

void ServiceWorkerSoftUpdateLoader::start(NetworkSession* session, ServiceWorkerJobData&& jobData, bool shouldRefreshCache, ResourceRequest&& request, Handler&& completionHandler)
{
    if (!session)
        return completionHandler(serviceWorkerFetchError(jobData.identifier(), ServiceWorkerRegistrationKey { jobData.registrationKey() }, ResourceError { ResourceError::Type::Cancellation }));
    auto loader = std::unique_ptr<ServiceWorkerSoftUpdateLoader>(new ServiceWorkerSoftUpdateLoader(*session, WTFMove(jobData), shouldRefreshCache, WTFMove(request), WTFMove(completionHandler)));
    session->addSoftUpdateLoader(WTFMove(loader));
}

ServiceWorkerSoftUpdateLoader::ServiceWorkerSoftUpdateLoader(NetworkSession& session, ServiceWorkerJobData&& jobData, bool shouldRefreshCache, ResourceRequest&& request, Handler&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
    , m_jobData(WTFMove(jobData))
    , m_session(makeWeakPtr(session))
{
    ASSERT(!request.isConditional());

    if (session.cache()) {
        // We set cache policy to disable speculative loading/async revalidation from the cache.
        request.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataDontLoad);
        
        session.cache()->retrieve(request, NetworkCache::GlobalFrameID { }, [this, weakThis = makeWeakPtr(*this), request, shouldRefreshCache](auto&& entry, auto&&) mutable {
            if (!weakThis)
                return;
            if (!m_session) {
                fail(ResourceError { ResourceError::Type::Cancellation });
                return;
            }
            if (!shouldRefreshCache && entry && !entry->needsValidation()) {
                loadWithCacheEntry(*entry);
                return;
            }

            request.setCachePolicy(ResourceRequestCachePolicy::RefreshAnyCacheData);
            if (entry) {
                m_cacheEntry = WTFMove(entry);

                String eTag = m_cacheEntry->response().httpHeaderField(HTTPHeaderName::ETag);
                if (!eTag.isEmpty())
                    request.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);

                String lastModified = m_cacheEntry->response().httpHeaderField(HTTPHeaderName::LastModified);
                if (!lastModified.isEmpty())
                    request.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
            }
            loadFromNetwork(*m_session, WTFMove(request));
        });
        return;
    }
    loadFromNetwork(session, WTFMove(request));
}

ServiceWorkerSoftUpdateLoader::~ServiceWorkerSoftUpdateLoader()
{
    if (m_completionHandler)
        m_completionHandler(serviceWorkerFetchError(m_jobData.identifier(), ServiceWorkerRegistrationKey { m_jobData.registrationKey() }, ResourceError { ResourceError::Type::Cancellation }));
}

void ServiceWorkerSoftUpdateLoader::fail(ResourceError&& error)
{
    if (!m_completionHandler)
        return;

    m_completionHandler(serviceWorkerFetchError(m_jobData.identifier(), ServiceWorkerRegistrationKey { m_jobData.registrationKey() }, WTFMove(error)));
    didComplete();
}

void ServiceWorkerSoftUpdateLoader::loadWithCacheEntry(NetworkCache::Entry& entry)
{
    auto error = processResponse(entry.response());
    if (!error.isNull()) {
        fail(WTFMove(error));
        return;
    }

    if (entry.buffer())
        didReceiveBuffer(makeRef(*entry.buffer()), 0);
    didFinishLoading({ });
}

void ServiceWorkerSoftUpdateLoader::loadFromNetwork(NetworkSession& session, ResourceRequest&& request)
{
    NetworkLoadParameters parameters;
    parameters.storedCredentialsPolicy = StoredCredentialsPolicy::Use;
    parameters.contentSniffingPolicy = ContentSniffingPolicy::DoNotSniffContent;
    parameters.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Sniff;
    parameters.request = WTFMove(request);
    m_networkLoad = makeUnique<NetworkLoad>(*this, nullptr, WTFMove(parameters), session);
}

void ServiceWorkerSoftUpdateLoader::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&&, ResourceResponse&&)
{
    fail(ResourceError { ResourceError::Type::Cancellation });
}

void ServiceWorkerSoftUpdateLoader::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    if (response.httpStatusCode() == 304 && m_cacheEntry) {
        loadWithCacheEntry(*m_cacheEntry);
        completionHandler(PolicyAction::Ignore);
        return;
    }
    auto error = processResponse(response);
    if (!error.isNull()) {
        fail(WTFMove(error));
        completionHandler(PolicyAction::Ignore);
        return;
    }
    completionHandler(PolicyAction::Use);
}

// https://w3c.github.io/ServiceWorker/#update-algorithm, steps 9.7 to 9.17
ResourceError ServiceWorkerSoftUpdateLoader::processResponse(const ResourceResponse& response)
{
    auto error = WorkerScriptLoader::validateWorkerResponse(response, FetchOptions::Destination::Serviceworker);
    if (!error.isNull())
        return error;

    error = ServiceWorkerJob::validateServiceWorkerResponse(m_jobData, response);
    if (!error.isNull())
        return error;

    m_contentSecurityPolicy = ContentSecurityPolicyResponseHeaders { response };
    m_referrerPolicy = response.httpHeaderField(HTTPHeaderName::ReferrerPolicy);
    m_responseEncoding = response.textEncodingName();

    return { };
}

void ServiceWorkerSoftUpdateLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    if (!m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/javascript"_s, m_responseEncoding);
        else
            m_decoder = TextResourceDecoder::create("text/javascript"_s, "UTF-8");
    }

    if (auto size = buffer->size())
        m_script.append(m_decoder->decode(buffer->data(), size));
}

void ServiceWorkerSoftUpdateLoader::didFinishLoading(const WebCore::NetworkLoadMetrics&)
{
    if (m_decoder)
        m_script.append(m_decoder->flush());
    m_completionHandler({ m_jobData.identifier(), m_jobData.registrationKey(), m_script.toString(), m_contentSecurityPolicy, m_referrerPolicy, { } });
    didComplete();
}

void ServiceWorkerSoftUpdateLoader::didFailLoading(const ResourceError& error)
{
    fail(ResourceError(error));
}

void ServiceWorkerSoftUpdateLoader::didComplete()
{
    m_networkLoad = nullptr;
    if (m_session)
        m_session->removeSoftUpdateLoader(this);
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
