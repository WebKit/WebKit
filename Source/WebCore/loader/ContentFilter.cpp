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

std::unique_ptr<ContentFilter> ContentFilter::createIfNeeded(DecisionFunction decisionFunction)
{
    Container filters;
    for (auto& type : types()) {
        if (!type.enabled())
            continue;

        auto filter = type.create();
        ASSERT(filter);
        filters.append(WTF::move(filter));
    }

    if (filters.isEmpty())
        return nullptr;

    return std::make_unique<ContentFilter>(WTF::move(filters), WTF::move(decisionFunction));
}

ContentFilter::ContentFilter(Container contentFilters, DecisionFunction decisionFunction)
    : m_contentFilters { WTF::move(contentFilters) }
    , m_decisionFunction { WTF::move(decisionFunction) }
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
    LOG(ContentFiltering, "ContentFilter received response from <%s>.\n", response.url().string().ascii().data());
    ASSERT(m_state == State::Filtering);
    ASSERT_UNUSED(resource, resource == m_mainResource.get());
    forEachContentFilterUntilBlocked([&response](PlatformContentFilter& contentFilter) {
        contentFilter.responseReceived(response);
    });
}

void ContentFilter::dataReceived(CachedResource* resource, const char* data, int length)
{
    LOG(ContentFiltering, "ContentFilter received %d bytes of data from <%s>.\n", length, resource->url().string().ascii().data());
    ASSERT(m_state == State::Filtering);
    ASSERT_UNUSED(resource, resource == m_mainResource.get());
    forEachContentFilterUntilBlocked([data, length](PlatformContentFilter& contentFilter) {
        contentFilter.addData(data, length);
    });
}

void ContentFilter::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_state == State::Filtering);
    ASSERT_UNUSED(resource, resource == m_mainResource.get());
    willSendRequest(request, redirectResponse);
}

void ContentFilter::notifyFinished(CachedResource* resource)
{
    LOG(ContentFiltering, "ContentFilter will finish filtering main resource at <%s>.\n", resource->url().string().ascii().data());
    ASSERT(m_state == State::Filtering);
    ASSERT_UNUSED(resource, resource == m_mainResource.get());
    forEachContentFilterUntilBlocked([](PlatformContentFilter& contentFilter) {
        contentFilter.finishedAddingData();
    });
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

    // Calling m_decisionFunction might delete |this|.
    if (m_decisionFunction)
        m_decisionFunction();
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
