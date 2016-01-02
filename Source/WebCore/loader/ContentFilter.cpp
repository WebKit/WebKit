/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ContentFilter.h"

#if ENABLE(CONTENT_FILTERING)

#include "CachedRawResource.h"
#include "ContentFilterUnblockHandler.h"
#include "DocumentLoader.h"
#include "Logging.h"
#include "NetworkExtensionContentFilter.h"
#include "ParentalControlsContentFilter.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if !LOG_DISABLED
#include <wtf/text/CString.h>
#endif

namespace WebCore {

Vector<ContentFilter::Type>& ContentFilter::types()
{
    static NeverDestroyed<Vector<ContentFilter::Type>> types {
        Vector<ContentFilter::Type> {
#if HAVE(PARENTAL_CONTROLS)
            type<ParentalControlsContentFilter>(),
#endif
#if HAVE(NETWORK_EXTENSION)
            type<NetworkExtensionContentFilter>()
#endif
        }
    };
    return types;
}

std::unique_ptr<ContentFilter> ContentFilter::createIfEnabled(DocumentLoader& documentLoader)
{
    Container filters;
    for (auto& type : types()) {
        if (!type.enabled())
            continue;

        auto filter = type.create();
        ASSERT(filter);
        filters.append(WTFMove(filter));
    }

    if (filters.isEmpty())
        return nullptr;

    return std::make_unique<ContentFilter>(WTFMove(filters), documentLoader);
}

ContentFilter::ContentFilter(Container contentFilters, DocumentLoader& documentLoader)
    : m_contentFilters { WTFMove(contentFilters) }
    , m_documentLoader { documentLoader }
{
    LOG(ContentFiltering, "Creating ContentFilter with %zu platform content filter(s).\n", m_contentFilters.size());
    ASSERT(!m_contentFilters.isEmpty());
}

ContentFilter::~ContentFilter()
{
    LOG(ContentFiltering, "Destroying ContentFilter.\n");
    if (!m_mainResource)
        return;
    ASSERT(m_mainResource->hasClient(this));
    m_mainResource->removeClient(this);
}

void ContentFilter::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    LOG(ContentFiltering, "ContentFilter received request for <%s> with redirect response from <%s>.\n", request.url().string().ascii().data(), redirectResponse.url().string().ascii().data());
    ResourceRequest requestCopy { request };
    ASSERT(m_state == State::Initialized || m_state == State::Filtering);
    forEachContentFilterUntilBlocked([&requestCopy, &redirectResponse](PlatformContentFilter& contentFilter) {
        contentFilter.willSendRequest(requestCopy, redirectResponse);
        if (contentFilter.didBlockData())
            requestCopy = ResourceRequest();
    });
#if !LOG_DISABLED
    if (request != requestCopy)
        LOG(ContentFiltering, "ContentFilter changed request url to <%s>.\n", requestCopy.url().string().ascii().data());
#endif
    request = requestCopy;
}

void ContentFilter::startFilteringMainResource(CachedRawResource& resource)
{
    LOG(ContentFiltering, "ContentFilter will start filtering main resource at <%s>.\n", resource.url().string().ascii().data());
    ASSERT(m_state == State::Initialized);
    m_state = State::Filtering;
    ASSERT(!m_mainResource);
    m_mainResource = &resource;
    ASSERT(!m_mainResource->hasClient(this));
    m_mainResource->addClient(this);
}

void ContentFilter::stopFilteringMainResource()
{
    m_state = State::Stopped;
    if (!m_mainResource)
        return;

    ASSERT(m_mainResource->hasClient(this));
    m_mainResource->removeClient(this);
    m_mainResource = nullptr;
}

ContentFilterUnblockHandler ContentFilter::unblockHandler() const
{
    ASSERT(m_state == State::Blocked);
    ASSERT(m_blockingContentFilter);
    ASSERT(m_blockingContentFilter->didBlockData());
    return m_blockingContentFilter->unblockHandler();
}

Ref<SharedBuffer> ContentFilter::replacementData() const
{
    ASSERT(m_state == State::Blocked);
    ASSERT(m_blockingContentFilter);
    ASSERT(m_blockingContentFilter->didBlockData());
    return m_blockingContentFilter->replacementData();
}

String ContentFilter::unblockRequestDeniedScript() const
{
    ASSERT(m_state == State::Blocked);
    ASSERT(m_blockingContentFilter);
    ASSERT(m_blockingContentFilter->didBlockData());
    return m_blockingContentFilter->unblockRequestDeniedScript();
}

void ContentFilter::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT(resource);
    ASSERT(resource == m_mainResource);
    ASSERT(m_state != State::Initialized);
    LOG(ContentFiltering, "ContentFilter received response from <%s>.\n", response.url().string().ascii().data());

    if (m_state == State::Filtering) {
        forEachContentFilterUntilBlocked([&response](PlatformContentFilter& contentFilter) {
            contentFilter.responseReceived(response);
        });
    }

    if (m_state != State::Blocked)
        m_documentLoader.responseReceived(resource, response);
}

void ContentFilter::dataReceived(CachedResource* resource, const char* data, int length)
{
    ASSERT(resource);
    ASSERT(resource == m_mainResource);
    ASSERT(m_state != State::Initialized);
    LOG(ContentFiltering, "ContentFilter received %d bytes of data from <%s>.\n", length, resource->url().string().ascii().data());

    if (m_state == State::Filtering) {
        forEachContentFilterUntilBlocked([data, length](PlatformContentFilter& contentFilter) {
            contentFilter.addData(data, length);
        });

        if (m_state == State::Allowed)
            deliverResourceData(*resource);
        return;
    }

    if (m_state == State::Allowed)
        m_documentLoader.dataReceived(resource, data, length);
}

void ContentFilter::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(resource);
    ASSERT(resource == m_mainResource);
    ASSERT(m_state != State::Initialized);

    if (m_state == State::Filtering)
        willSendRequest(request, redirectResponse);

    if (m_state != State::Blocked)
        m_documentLoader.redirectReceived(resource, request, redirectResponse);
}

void ContentFilter::notifyFinished(CachedResource* resource)
{
    ASSERT(resource);
    ASSERT(resource == m_mainResource);
    ASSERT(m_state != State::Initialized);
    LOG(ContentFiltering, "ContentFilter will finish filtering main resource at <%s>.\n", resource->url().string().ascii().data());

    if (resource->errorOccurred()) {
        m_documentLoader.notifyFinished(resource);
        return;
    }

    if (m_state == State::Filtering) {
        forEachContentFilterUntilBlocked([](PlatformContentFilter& contentFilter) {
            contentFilter.finishedAddingData();
        });

        if (m_state != State::Blocked)
            deliverResourceData(*resource);
        
        if (m_state == State::Stopped)
            return;
    }

    if (m_state != State::Blocked)
        m_documentLoader.notifyFinished(resource);
}

void ContentFilter::forEachContentFilterUntilBlocked(std::function<void(PlatformContentFilter&)> function)
{
    bool allFiltersAllowedLoad { true };
    for (auto& contentFilter : m_contentFilters) {
        if (!contentFilter->needsMoreData()) {
            ASSERT(!contentFilter->didBlockData());
            continue;
        }

        function(*contentFilter);

        if (contentFilter->didBlockData()) {
            ASSERT(!m_blockingContentFilter);
            m_blockingContentFilter = contentFilter.get();
            didDecide(State::Blocked);
            return;
        } else if (contentFilter->needsMoreData())
            allFiltersAllowedLoad = false;
    }

    if (allFiltersAllowedLoad)
        didDecide(State::Allowed);
}

void ContentFilter::didDecide(State state)
{
    ASSERT(m_state != State::Allowed);
    ASSERT(m_state != State::Blocked);
    ASSERT(state == State::Allowed || state == State::Blocked);
    LOG(ContentFiltering, "ContentFilter decided load should be %s for main resource at <%s>.\n", state == State::Allowed ? "allowed" : "blocked", m_mainResource ? m_mainResource->url().string().ascii().data() : "");
    m_state = state;
    m_documentLoader.contentFilterDidDecide();
}

void ContentFilter::deliverResourceData(CachedResource& resource)
{
    ASSERT(resource.dataBufferingPolicy() == BufferData);
    if (auto* resourceBuffer = resource.resourceBuffer())
        m_documentLoader.dataReceived(&resource, resourceBuffer->data(), resourceBuffer->size());
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
