/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringView.h>

#define RELEASE_LOG_ALWAYS(fmt, ...) RELEASE_LOG(Network, "%p - CachedRawResource::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

CachedRawResource::CachedRawResource(CachedResourceRequest&& request, Type type, PAL::SessionID sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), type, sessionID, cookieJar)
{
    ASSERT(isMainOrMediaOrIconOrRawResource());
}

std::optional<SharedBufferDataView> CachedRawResource::calculateIncrementalDataChunk(const FragmentedSharedBuffer& data) const
{
    size_t previousDataLength = encodedSize();
    if (data.size() <= previousDataLength)
        return std::nullopt;
    return data.getSomeData(previousDataLength);
}

void CachedRawResource::updateBuffer(const FragmentedSharedBuffer& data)
{
    // Skip any updateBuffers triggered from nested runloops. We'll have the complete buffer in finishLoading.
    if (m_inIncrementalDataNotify)
        return;

    // We need to keep a strong reference to both the SharedBuffer and the current CachedRawResource instance
    // as notifyClientsDataWasReceived call may delete both.
    CachedResourceHandle protectedThis(this);
    Ref protectedData { data };

    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::BufferData);
    // While m_data is immutable, we need to drop the const, this will be removed in bug 236736.
    m_data = const_cast<FragmentedSharedBuffer*>(&data);

    // Notify clients only of the newly appended content since the last run.
    auto previousDataSize = encodedSize();
    while (data.size() > previousDataSize) {
        auto incrementalData = data.getSomeData(previousDataSize);
        previousDataSize += incrementalData.size();

        SetForScope notifyScope(m_inIncrementalDataNotify, true);
        notifyClientsDataWasReceived(incrementalData.createSharedBuffer());
    }
    setEncodedSize(data.size());

    if (dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData) {
        if (RefPtr loader = m_loader)
            loader->setDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
        clear();
    } else
        CachedResource::updateBuffer(data);

    if (m_delayedFinishLoading) {
        auto delayedFinishLoading = std::exchange(m_delayedFinishLoading, std::nullopt);
        finishLoading(delayedFinishLoading->buffer.get(), { });
    }
}

void CachedRawResource::updateData(const SharedBuffer& buffer)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData);
    notifyClientsDataWasReceived(buffer);
    CachedResource::updateData(buffer);
}

void CachedRawResource::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (m_inIncrementalDataNotify) {
        // We may get here synchronously from updateBuffer() if the callback there ends up spinning a runloop.
        // In that case delay the call.
        m_delayedFinishLoading = std::make_optional(DelayedFinishLoading { data });
        return;
    };
    CachedResourceHandle protectedThis { this };
    DataBufferingPolicy dataBufferingPolicy = this->dataBufferingPolicy();
    if (dataBufferingPolicy == DataBufferingPolicy::BufferData) {
        m_data = const_cast<FragmentedSharedBuffer*>(data);
        if (data) {
            if (auto incrementalData = calculateIncrementalDataChunk(*data)) {
                setEncodedSize(data->size());
                notifyClientsDataWasReceived(incrementalData->createSharedBuffer());
            }
        }
    }

#if USE(QUICK_LOOK)
    m_allowEncodedDataReplacement = m_loader && !m_loader->isQuickLookResource();
#endif

    CachedResource::finishLoading(data, metrics);
    if (dataBufferingPolicy == DataBufferingPolicy::BufferData && this->dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData) {
        if (RefPtr loader = m_loader)
            loader->setDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
        clear();
    }
}

void CachedRawResource::notifyClientsDataWasReceived(const SharedBuffer& buffer)
{
    if (buffer.isEmpty())
        return;

    CachedResourceHandle protectedThis { this };
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (RefPtr client = walker.next())
        client->dataReceived(*this, buffer);
}

static void iterateRedirects(CachedResourceHandle<CachedRawResource>&& handle, CachedRawResourceClient& client, Vector<std::pair<ResourceRequest, ResourceResponse>>&& redirectsInReverseOrder, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    if (!handle->hasClient(client) || redirectsInReverseOrder.isEmpty())
        return completionHandler({ });
    auto redirectPair = redirectsInReverseOrder.takeLast();
    client.redirectReceived(*handle, WTFMove(redirectPair.first), WTFMove(redirectPair.second), [handle = WTFMove(handle), client = WeakPtr { client }, redirectsInReverseOrder = WTFMove(redirectsInReverseOrder), completionHandler = WTFMove(completionHandler)] (ResourceRequest&&) mutable {
        // Ignore the new request because we can't do anything with it.
        // We're just replying a redirect chain that has already happened.
        if (!client)
            return completionHandler({ });
        iterateRedirects(WTFMove(handle), *client, WTFMove(redirectsInReverseOrder), WTFMove(completionHandler));
    });
}

void CachedRawResource::didAddClient(CachedResourceClient& c)
{
    auto& client = downcast<CachedRawResourceClient>(c);
    size_t redirectCount = m_redirectChain.size();
    Vector<std::pair<ResourceRequest, ResourceResponse>> redirectsInReverseOrder(redirectCount, [&](size_t i) {
        const auto& pair = m_redirectChain[redirectCount - i - 1];
        return std::pair<ResourceRequest, ResourceResponse> { pair.m_request, pair.m_redirectResponse };
    });

    iterateRedirects(CachedResourceHandle { this }, client, WTFMove(redirectsInReverseOrder), [this, protectedThis = CachedResourceHandle { this }, client = WeakPtr { client }] (ResourceRequest&&) mutable {
        if (!client || !hasClient(*client))
            return;
        auto responseProcessedHandler = [this, protectedThis = WTFMove(protectedThis), client] {
            if (!client || !hasClient(*client))
                return;
            if (RefPtr data = m_data) {
                data->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
                    if (!client || hasClient(*client))
                        client->dataReceived(*this, buffer);
                });
            }
            if (!client || !hasClient(*client))
                return;
            CachedResource::didAddClient(*client);
        };

        if (!response().isNull()) {
            ResourceResponse response(CachedResource::response());
            if (validationCompleting())
                response.setSource(ResourceResponse::Source::MemoryCacheAfterValidation);
            else {
                ASSERT(!validationInProgress());
                response.setSource(ResourceResponse::Source::MemoryCache);
            }
            client->responseReceived(*this, WTFMove(response), WTFMove(responseProcessedHandler));
        } else
            responseProcessedHandler();
    });
}

void CachedRawResource::allClientsRemoved()
{
    if (RefPtr loader = m_loader)
        loader->cancelIfNotFinishing();
}

static void iterateClients(CachedResourceClientWalker<CachedRawResourceClient>&& walker, CachedResourceHandle<CachedRawResource>&& handle, ResourceRequest&& request, std::unique_ptr<ResourceResponse>&& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    RefPtr client = walker.next();
    if (!client)
        return completionHandler(WTFMove(request));
    const ResourceResponse& responseReference = *response;
    client->redirectReceived(*handle, WTFMove(request), responseReference, [walker = WTFMove(walker), handle = WTFMove(handle), response = WTFMove(response), completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request) mutable {
        iterateClients(WTFMove(walker), WTFMove(handle), WTFMove(request), WTFMove(response), WTFMove(completionHandler));
    });
}

void CachedRawResource::redirectReceived(ResourceRequest&& request, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    RELEASE_LOG_ALWAYS("redirectReceived:");
    if (response.isNull())
        CachedResource::redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
    else {
        m_redirectChain.append(RedirectPair(request, response));
        iterateClients(CachedResourceClientWalker<CachedRawResourceClient>(*this), CachedResourceHandle { this }, WTFMove(request), makeUnique<ResourceResponse>(response), [this, protectedThis = CachedResourceHandle { this }, completionHandler = WTFMove(completionHandler), response] (ResourceRequest&& request) mutable {
            CachedResource::redirectReceived(WTFMove(request), response, WTFMove(completionHandler));
        });
    }
}

void CachedRawResource::responseReceived(ResourceResponse&& newResponse)
{
    CachedResourceHandle protectedThis { this };
    if (!m_resourceLoaderIdentifier)
        m_resourceLoaderIdentifier = m_loader->identifier();
    CachedResource::responseReceived(WTFMove(newResponse));
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (RefPtr client = walker.next())
        client->responseReceived(*this, response(), nullptr);
}

bool CachedRawResource::shouldCacheResponse(const ResourceResponse& response)
{
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (RefPtr client = walker.next()) {
        if (!client->shouldCacheResponse(*this, response))
            return false;
    }
    return true;
}

void CachedRawResource::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (RefPtr client = walker.next())
        client->dataSent(*this, bytesSent, totalBytesToBeSent);
}

void CachedRawResource::finishedTimingForWorkerLoad(ResourceTiming&& resourceTiming)
{
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (RefPtr client = walker.next())
        client->finishedTimingForWorkerLoad(*this, resourceTiming);
}

void CachedRawResource::switchClientsToRevalidatedResource()
{
    ASSERT(m_loader);
    // If we're in the middle of a successful revalidation, responseReceived() hasn't been called, so we haven't set m_identifier.
    ASSERT(!m_resourceLoaderIdentifier);
    downcast<CachedRawResource>(*resourceToRevalidate()).m_resourceLoaderIdentifier = m_loader->identifier();
    CachedResource::switchClientsToRevalidatedResource();
}

void CachedRawResource::setDefersLoading(bool defers)
{
    if (RefPtr loader = m_loader)
        loader->setDefersLoading(defers);
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
    case HTTPHeaderName::Referer:
    case HTTPHeaderName::SecPurpose:
    case HTTPHeaderName::SecSpeculationTags:
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
    if (RefPtr loader = m_loader)
        loader->clearResourceData();
}

#if USE(QUICK_LOOK)
void CachedRawResource::previewResponseReceived(ResourceResponse&& newResponse)
{
    CachedResourceHandle protectedThis { this };
    CachedResource::previewResponseReceived(WTFMove(newResponse));
    CachedResourceClientWalker<CachedRawResourceClient> walker(*this);
    while (CachedRawResourceClient* c = walker.next())
        c->previewResponseReceived(*this, response());
}

#endif

} // namespace WebCore
