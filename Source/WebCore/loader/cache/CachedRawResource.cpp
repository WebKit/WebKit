/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
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
#include "CachedRawResource.h"

#include "CachedRawResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "HTTPHeaderNames.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringView.h>

namespace WebCore {

CachedRawResource::CachedRawResource(CachedResourceRequest&& request, Type type, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), type, sessionID, cookieJar)
    , m_identifier(0)
    , m_allowEncodedDataReplacement(true)
{
    ASSERT(isMainOrMediaOrIconOrRawResource());
}

Optional<SharedBufferDataView> CachedRawResource::calculateIncrementalDataChunk(const SharedBuffer* data) const
{
    size_t previousDataLength = encodedSize();
    if (!data || data->size() <= previousDataLength)
        return WTF::nullopt;
    return data->getSomeData(previousDataLength);
}

void CachedRawResource::updateBuffer(SharedBuffer& data)
{
    // Skip any updateBuffers triggered from nested runloops. We'll have the complete buffer in finishLoading.
    if (m_inIncrementalDataNotify)
        return;

    CachedResourceHandle<CachedRawResource> protectedThis(this);
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::BufferData);
    m_data = &data;

    auto previousDataSize = encodedSize();
    while (data.size() > previousDataSize) {
        auto incrementalData = data.getSomeData(previousDataSize);
        previousDataSize += incrementalData.size();

        SetForScope<bool> notifyScope(m_inIncrementalDataNotify, true);
        notifyClientsDataWasReceived(incrementalData.data(), incrementalData.size());
    }
    setEncodedSize(data.size());

    if (dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData) {
        if (m_loader)
            m_loader->setDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
        clear();
    } else
        CachedResource::updateBuffer(data);

    if (m_delayedFinishLoading) {
        auto delayedFinishLoading = std::exchange(m_delayedFinishLoading, WTF::nullopt);
        finishLoading(delayedFinishLoading->buffer.get());
    }
}

void CachedRawResource::updateData(const char* data, unsigned length)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData);
    notifyClientsDataWasReceived(data, length);
    CachedResource::updateData(data, length);
}

void CachedRawResource::finishLoading(SharedBuffer* data)
{
    if (m_inIncrementalDataNotify) {
        // We may get here synchronously from updateBuffer() if the callback there ends up spinning a runloop.
        // In that case delay the call.
        m_delayedFinishLoading = makeOptional(DelayedFinishLoading { data });
        return;
    };
    CachedResourceHandle<CachedRawResource> protectedThis(this);
    DataBufferingPolicy dataBufferingPolicy = this->dataBufferingPolicy();
    if (dataBufferingPolicy == DataBufferingPolicy::BufferData) {
        m_data = data;

        if (auto incrementalData = calculateIncrementalDataChunk(data)) {
            setEncodedSize(data->size());
            notifyClientsDataWasReceived(incrementalData->data(), incrementalData->size());
        }
    }

#if USE(QUICK_LOOK)
    m_allowEncodedDataReplacement = m_loader && !m_loader->isQuickLookResource();
#endif

    CachedResource::finishLoading(data);
    if (dataBufferingPolicy == DataBufferingPolicy::BufferData && this->dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData) {
        if (m_loader)
            m_loader->setDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
        clear();
    }
}

void CachedRawResource::notifyClientsDataWasReceived(const char* data, unsigned length)
{
    if (!length)
        return;

    CachedResourceHandle<CachedRawResource> protectedThis(this);
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->dataReceived(*this, data, length);
}

static void iterateRedirects(CachedResourceHandle<CachedRawResource>&& handle, CachedRawResourceClient& client, Vector<std::pair<ResourceRequest, ResourceResponse>>&& redirectsInReverseOrder, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    if (!handle->hasClient(client) || redirectsInReverseOrder.isEmpty())
        return completionHandler({ });
    auto redirectPair = redirectsInReverseOrder.takeLast();
    client.redirectReceived(*handle, WTFMove(redirectPair.first), WTFMove(redirectPair.second), [handle = WTFMove(handle), client, redirectsInReverseOrder = WTFMove(redirectsInReverseOrder), completionHandler = WTFMove(completionHandler)] (ResourceRequest&&) mutable {
        // Ignore the new request because we can't do anything with it.
        // We're just replying a redirect chain that has already happened.
        iterateRedirects(WTFMove(handle), client, WTFMove(redirectsInReverseOrder), WTFMove(completionHandler));
    });
}

void CachedRawResource::didAddClient(CachedResourceClient& c)
{
    CachedRawResourceClient& client = static_cast<CachedRawResourceClient&>(c);
    size_t redirectCount = m_redirectChain.size();
    Vector<std::pair<ResourceRequest, ResourceResponse>> redirectsInReverseOrder;
    redirectsInReverseOrder.reserveInitialCapacity(redirectCount);
    for (size_t i = 0; i < redirectCount; ++i) {
        const auto& pair = m_redirectChain[redirectCount - i - 1];
        redirectsInReverseOrder.uncheckedAppend(std::make_pair(pair.m_request, pair.m_redirectResponse));
    }
    iterateRedirects(CachedResourceHandle<CachedRawResource>(this), client, WTFMove(redirectsInReverseOrder), [this, protectedThis = CachedResourceHandle<CachedRawResource>(this), client = &client] (ResourceRequest&&) mutable {
        if (!hasClient(*client))
            return;
        auto responseProcessedHandler = [this, protectedThis = WTFMove(protectedThis), client] {
            if (!hasClient(*client))
                return;
            if (m_data)
                client->dataReceived(*this, m_data->data(), m_data->size());
            if (!hasClient(*client))
                return;
            CachedResource::didAddClient(*client);
        };

        if (!m_response.isNull()) {
            ResourceResponse response(m_response);
            if (validationCompleting())
                response.setSource(ResourceResponse::Source::MemoryCacheAfterValidation);
            else {
                ASSERT(!validationInProgress());
                response.setSource(ResourceResponse::Source::MemoryCache);
            }
            client->responseReceived(*this, response, WTFMove(responseProcessedHandler));
        } else
            responseProcessedHandler();
    });
}

void CachedRawResource::allClientsRemoved()
{
    if (m_loader)
        m_loader->cancelIfNotFinishing();
}

static void iterateClients(CachedResourceClientWalker<CachedRawResourceClient>&& walker, CachedResourceHandle<CachedRawResource>&& handle, ResourceRequest&& request, std::unique_ptr<ResourceResponse>&& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    auto client = walker.next();
    if (!client)
        return completionHandler(WTFMove(request));
    const ResourceResponse& responseReference = *response;
    client->redirectReceived(*handle, WTFMove(request), responseReference, [walker = WTFMove(walker), handle = WTFMove(handle), response = WTFMove(response), completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request) mutable {
        iterateClients(WTFMove(walker), WTFMove(handle), WTFMove(request), WTFMove(response), WTFMove(completionHandler));
    });
}

void CachedRawResource::redirectReceived(ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    if (response.isNull())
        CachedResource::redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
    else {
        m_redirectChain.append(RedirectPair(request, response));
        iterateClients(CachedResourceClientWalker<CachedRawResourceClient>(m_clients), CachedResourceHandle<CachedRawResource>(this), WTFMove(request), makeUnique<ResourceResponse>(response), [this, protectedThis = CachedResourceHandle<CachedRawResource>(this), completionHandler = WTFMove(completionHandler), response] (ResourceRequest&& request) mutable {
            CachedResource::redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
        });
    }
}

void CachedRawResource::responseReceived(const ResourceResponse& response)
{
    CachedResourceHandle<CachedRawResource> protectedThis(this);
    if (!m_identifier)
        m_identifier = m_loader->identifier();
    CachedResource::responseReceived(response);
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->responseReceived(*this, m_response, nullptr);
}

bool CachedRawResource::shouldCacheResponse(const ResourceResponse& response)
{
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next()) {
        if (!c->shouldCacheResponse(*this, response))
            return false;
    }
    return true;
}

void CachedRawResource::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void CachedRawResource::finishedTimingForWorkerLoad(ResourceTiming&& resourceTiming)
{
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->finishedTimingForWorkerLoad(*this, resourceTiming);
}

void CachedRawResource::switchClientsToRevalidatedResource()
{
    ASSERT(m_loader);
    // If we're in the middle of a successful revalidation, responseReceived() hasn't been called, so we haven't set m_identifier.
    ASSERT(!m_identifier);
    downcast<CachedRawResource>(*resourceToRevalidate()).m_identifier = m_loader->identifier();
    CachedResource::switchClientsToRevalidatedResource();
}

void CachedRawResource::setDefersLoading(bool defers)
{
    if (m_loader)
        m_loader->setDefersLoading(defers);
}

void CachedRawResource::setDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    m_options.dataBufferingPolicy = dataBufferingPolicy;
}

static bool shouldIgnoreHeaderForCacheReuse(HTTPHeaderName name)
{
    switch (name) {
    // FIXME: This list of headers that don't affect cache policy almost certainly isn't complete.
    case HTTPHeaderName::Accept:
    case HTTPHeaderName::CacheControl:
    case HTTPHeaderName::Pragma:
    case HTTPHeaderName::Purpose:
    case HTTPHeaderName::Referer:
    case HTTPHeaderName::UserAgent:
        return true;

    default:
        return false;
    }
}

bool CachedRawResource::canReuse(const ResourceRequest& newRequest) const
{
    if (dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData)
        return false;

    if (m_resourceRequest.httpMethod() != newRequest.httpMethod())
        return false;

    if (m_resourceRequest.httpBody() != newRequest.httpBody())
        return false;

    if (m_resourceRequest.allowCookies() != newRequest.allowCookies())
        return false;

    if (newRequest.isConditional())
        return false;

    // Ensure most headers match the existing headers before continuing.
    // Note that the list of ignored headers includes some headers explicitly related to caching.
    // A more detailed check of caching policy will be performed later, this is simply a list of
    // headers that we might permit to be different and still reuse the existing CachedResource.
    const HTTPHeaderMap& newHeaders = newRequest.httpHeaderFields();
    const HTTPHeaderMap& oldHeaders = m_resourceRequest.httpHeaderFields();

    for (const auto& header : newHeaders) {
        if (header.keyAsHTTPHeaderName) {
            if (!shouldIgnoreHeaderForCacheReuse(header.keyAsHTTPHeaderName.value())
                && header.value != oldHeaders.get(header.keyAsHTTPHeaderName.value()))
                return false;
        } else if (header.value != oldHeaders.get(header.key))
            return false;
    }

    // For this second loop, we don't actually need to compare values, checking that the
    // key is contained in newHeaders is sufficient due to the previous loop.
    for (const auto& header : oldHeaders) {
        if (header.keyAsHTTPHeaderName) {
            if (!shouldIgnoreHeaderForCacheReuse(header.keyAsHTTPHeaderName.value())
                && !newHeaders.contains(header.keyAsHTTPHeaderName.value()))
                return false;
        } else if (!newHeaders.contains(header.key))
            return false;
    }

    return true;
}

void CachedRawResource::clear()
{
    m_data = nullptr;
    setEncodedSize(0);
    if (m_loader)
        m_loader->clearResourceData();
}

} // namespace WebCore
