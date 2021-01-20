/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "ContentFilterClient.h"
#include "ContentFilterUnblockHandler.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "Logging.h"
#include "NetworkExtensionContentFilter.h"
#include "ParentalControlsContentFilter.h"
#include "ScriptController.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

#if !LOG_DISABLED
#include <wtf/text/CString.h>
#endif

namespace WebCore {

Vector<ContentFilter::Type>& ContentFilter::types()
{
    static NeverDestroyed<Vector<ContentFilter::Type>> types {
        Vector<ContentFilter::Type>::from(
#if HAVE(PARENTAL_CONTROLS)
            type<ParentalControlsContentFilter>(),
#endif
            type<NetworkExtensionContentFilter>()
        )
    };
    return types;
}

std::unique_ptr<ContentFilter> ContentFilter::create(ContentFilterClient& client)
{
    Container filters;
    for (auto& type : types()) {
        auto filter = type.create();
        filters.append(WTFMove(filter));
    }

    if (filters.isEmpty())
        return nullptr;

    return makeUnique<ContentFilter>(WTFMove(filters), client);
}

ContentFilter::ContentFilter(Container&& contentFilters, ContentFilterClient& client)
    : m_contentFilters(WTFMove(contentFilters))
    , m_client(client)
{
    LOG(ContentFiltering, "Creating ContentFilter with %zu platform content filter(s).\n", m_contentFilters.size());
    ASSERT(!m_contentFilters.isEmpty());
}

ContentFilter::~ContentFilter()
{
    LOG(ContentFiltering, "Destroying ContentFilter.\n");
}

bool ContentFilter::continueAfterWillSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    Ref<ContentFilterClient> protectedClient { m_client };

    LOG(ContentFiltering, "ContentFilter received request for <%s> with redirect response from <%s>.\n", request.url().string().ascii().data(), redirectResponse.url().string().ascii().data());
#if !LOG_DISABLED
    ResourceRequest originalRequest { request };
#endif
    ASSERT(m_state == State::Stopped || m_state == State::Filtering);
    forEachContentFilterUntilBlocked([&request, &redirectResponse](PlatformContentFilter& contentFilter) {
        contentFilter.willSendRequest(request, redirectResponse);
    });
    if (m_state == State::Blocked)
        request = ResourceRequest();
#if !LOG_DISABLED
    if (request != originalRequest)
        LOG(ContentFiltering, "ContentFilter changed request url to <%s>.\n", originalRequest.url().string().ascii().data());
#endif
    return !request.isNull();
}

void ContentFilter::startFilteringMainResource(CachedRawResource& resource)
{
    if (m_state != State::Stopped)
        return;

    LOG(ContentFiltering, "ContentFilter will start filtering main resource at <%s>.\n", resource.url().string().ascii().data());
    m_state = State::Filtering;
    ASSERT(!m_mainResource);
    m_mainResource = &resource;
}

void ContentFilter::stopFilteringMainResource()
{
    if (m_state != State::Blocked)
        m_state = State::Stopped;
    m_mainResource = nullptr;
}

bool ContentFilter::continueAfterResponseReceived(const ResourceResponse& response)
{
    Ref<ContentFilterClient> protectedClient { m_client };

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter received response from <%s>.\n", response.url().string().ascii().data());
        forEachContentFilterUntilBlocked([&response](PlatformContentFilter& contentFilter) {
            contentFilter.responseReceived(response);
        });
    }

    return m_state != State::Blocked;
}

bool ContentFilter::continueAfterDataReceived(const char* data, int length)
{
    Ref<ContentFilterClient> protectedClient { m_client };

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter received %d bytes of data from <%s>.\n", length, m_mainResource->url().string().ascii().data());
        forEachContentFilterUntilBlocked([data, length](PlatformContentFilter& contentFilter) {
            contentFilter.addData(data, length);
        });

        if (m_state == State::Allowed)
            deliverResourceData(*m_mainResource);
        return false;
    }

    return m_state != State::Blocked;
}

bool ContentFilter::continueAfterNotifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_mainResource);
    Ref<ContentFilterClient> protectedClient { m_client };

    if (m_mainResource->errorOccurred())
        return true;

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter will finish filtering main resource at <%s>.\n", m_mainResource->url().string().ascii().data());
        forEachContentFilterUntilBlocked([](PlatformContentFilter& contentFilter) {
            contentFilter.finishedAddingData();
        });

        if (m_state != State::Blocked) {
            m_state = State::Allowed;
            deliverResourceData(*m_mainResource);
        }

        if (m_state == State::Stopped)
            return false;
    }

    return m_state != State::Blocked;
}

template <typename Function>
inline void ContentFilter::forEachContentFilterUntilBlocked(Function&& function)
{
    bool allFiltersAllowedLoad { true };
    for (auto& contentFilter : m_contentFilters) {
        if (!contentFilter->needsMoreData()) {
            ASSERT(!contentFilter->didBlockData());
            continue;
        }

        function(contentFilter.get());

        if (contentFilter->didBlockData()) {
            ASSERT(!m_blockingContentFilter);
            m_blockingContentFilter = &contentFilter;
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
    if (m_state != State::Blocked)
        return;

    m_blockedError = m_client.contentFilterDidBlock(m_blockingContentFilter->unblockHandler(), m_blockingContentFilter->unblockRequestDeniedScript());
    m_client.cancelMainResourceLoadForContentFilter(m_blockedError);
}

void ContentFilter::deliverResourceData(CachedResource& resource)
{
    ASSERT(m_state == State::Allowed);
    ASSERT(resource.dataBufferingPolicy() == DataBufferingPolicy::BufferData);
    if (auto* resourceBuffer = resource.resourceBuffer())
        m_client.dataReceivedThroughContentFilter(resourceBuffer->data(), resourceBuffer->size());
}

static const URL& blockedPageURL()
{
    static const auto blockedPageURL = makeNeverDestroyed([] () -> URL {
        auto webCoreBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebCore"));
        return adoptCF(CFBundleCopyResourceURL(webCoreBundle, CFSTR("ContentFilterBlockedPage"), CFSTR("html"), nullptr)).get();
    }());
    return blockedPageURL;
}

bool ContentFilter::continueAfterSubstituteDataRequest(const DocumentLoader& activeLoader, const SubstituteData& substituteData)
{
    if (auto contentFilter = activeLoader.contentFilter()) {
        if (contentFilter->m_state == State::Blocked && !contentFilter->m_isLoadingBlockedPage)
            return contentFilter->m_blockedError.failingURL() != substituteData.failingURL();
    }

    if (activeLoader.request().url() == blockedPageURL()) {
        ASSERT(activeLoader.substituteData().isValid());
        return activeLoader.substituteData().failingURL() != substituteData.failingURL();
    }

    return true;
}

bool ContentFilter::willHandleProvisionalLoadFailure(const ResourceError& error) const
{
    if (m_state != State::Blocked)
        return false;

    if (m_blockedError.errorCode() != error.errorCode() || m_blockedError.domain() != error.domain())
        return false;

    ASSERT(m_blockedError.failingURL() == error.failingURL());
    return true;
}

void ContentFilter::handleProvisionalLoadFailure(const ResourceError& error)
{
    ASSERT(willHandleProvisionalLoadFailure(error));

    RefPtr<SharedBuffer> replacementData { m_blockingContentFilter->replacementData() };
    ResourceResponse response { URL(), "text/html"_s, static_cast<long long>(replacementData->size()), "UTF-8"_s };
    SubstituteData substituteData { WTFMove(replacementData), error.failingURL(), response, SubstituteData::SessionHistoryVisibility::Hidden };
    SetForScope<bool> loadingBlockedPage { m_isLoadingBlockedPage, true };
    m_client.handleProvisionalLoadFailureFromContentFilter(blockedPageURL(), substituteData);
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
