/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "NetworkExtensionContentFilter.h"
#include "ParentalControlsContentFilter.h"
#include "ScriptController.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

#if HAVE(WEBCONTENTRESTRICTIONS)
#include "ParentalControlsURLFilter.h"
#endif

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

RefPtr<ContentFilter> ContentFilter::create(ContentFilterClient& client)
{
    PlatformContentFilter::FilterParameters params {
#if HAVE(WEBCONTENTRESTRICTIONS)
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
        client.webContentRestrictionsConfigurationPath(),
#endif
        client.mainDocumentURL(),
#endif
    };
    auto filters = types().map([params](auto& type) {
        return type.create(params);
    });

    if (filters.isEmpty())
        return nullptr;

    return adoptRef(*new ContentFilter(WTF::move(filters), client));
}

ContentFilter::ContentFilter(Container&& contentFilters, ContentFilterClient& client)
    : m_contentFilters(WTF::move(contentFilters))
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
    RefPtr protectedClient = m_client.get();
    if (!protectedClient)
        return false;

    LOG(ContentFiltering, "ContentFilter received request for <%{sensitive}s> with redirect response from <%{sensitive}s>.\n", request.url().string().utf8().data(), redirectResponse.url().string().utf8().data());
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
        LOG(ContentFiltering, "ContentFilter changed request url to <%{sensitive}s>.\n", originalRequest.url().string().ascii().data());
#endif
    return !request.isNull();
}

ContentFilter::ContentFilterCallbackAggregator::~ContentFilterCallbackAggregator()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_callback);

    if (m_isBlocked) {
        if (RefPtr contentFilter = m_contentFilter)
            contentFilter->didDecide(State::Blocked);
        m_callback(ResourceRequest());
        return;
    }

    if (RefPtr contentFilter = m_contentFilter; contentFilter && m_numberOfFiltersAllowed == contentFilter->m_contentFilters.size())
        contentFilter->didDecide(State::Allowed);

    m_contentFilter = nullptr;

    m_callback(WTF::move(m_request));
}

void ContentFilter::ContentFilterCallbackAggregator::didReceivePlatformContentFilterDecision(PlatformContentFilter& platformContentFilter, String&& urlString)
{
    ASSERT(RunLoop::isMain());

    if (platformContentFilter.isAllowed())
        ++m_numberOfFiltersAllowed;

    if (platformContentFilter.didBlockData() && !m_isBlocked) {
        m_isBlocked = true;
        RefPtr contentFilter = m_contentFilter;
        contentFilter->m_blockingContentFilter = platformContentFilter;
        return;
    }

    URL url { urlString };
    if (url.isValid())
        m_request.setURL(WTF::move(url));
}

ContentFilter::ContentFilterCallbackAggregator::ContentFilterCallbackAggregator(ContentFilter& contentFilter, const ResourceRequest& request, CompletionHandler<void(ResourceRequest&&)>&& callback)
    : m_contentFilter(contentFilter)
    , m_request(request)
    , m_callback(WTF::move(callback))
{
    ASSERT(RunLoop::isMain());
}

void ContentFilter::continueAfterWillSendRequest(ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    LOG(ContentFiltering, "ContentFilter received request for <%{sensitive}s> with redirect response from %s" SENSITIVE_LOG_STRING, request.url().string().utf8().data(), redirectResponse.url().string().utf8().data());
    ASSERT(m_state == State::Stopped || m_state == State::Filtering);

    Ref contentFilterCallbackAggregator = ContentFilterCallbackAggregator::create(*this, request, WTF::move(completionHandler));

    for (Ref platformContentFilter : m_contentFilters) {
        CompletionHandler<void(String&&)> completion = [contentFilterCallbackAggregator, platformContentFilter] (String&& urlString) mutable {
            ASSERT(RunLoop::isMain());
            contentFilterCallbackAggregator->didReceivePlatformContentFilterDecision(platformContentFilter, WTF::move(urlString));
        };

        ASSERT(platformContentFilter->needsMoreData());
        platformContentFilter->willSendRequest(ResourceRequest { request }, redirectResponse, WTF::move(completion));
    }
}

void ContentFilter::startFilteringMainResource(const URL& url)
{
    if (m_state != State::Stopped)
        return;

    LOG(ContentFiltering, "ContentFilter will start filtering main resource at <%{sensitive}s>.\n", url.string().ascii().data());
    m_state = State::Filtering;
    ASSERT(m_mainResourceURL.isEmpty());
    m_mainResourceURL = url;
}

void ContentFilter::startFilteringMainResource(CachedRawResource& resource)
{
    if (m_state != State::Stopped)
        return;

    LOG(ContentFiltering, "ContentFilter will start filtering main resource at <%{sensitive}s>.\n", resource.url().string().ascii().data());
    m_state = State::Filtering;
    ASSERT(!m_mainResource);
    m_mainResource = resource;
}

void ContentFilter::stopFilteringMainResource()
{
    if (m_state != State::Blocked)
        m_state = State::Stopped;
    m_mainResourceURL = URL();
}

bool ContentFilter::continueAfterResponseReceived(const ResourceResponse& response)
{
    RefPtr protectedClient = m_client.get();
    if (!protectedClient)
        return false;

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter received response from <%{sensitive}s>.\n", response.url().string().ascii().data());
        forEachContentFilterUntilBlocked([&response](PlatformContentFilter& contentFilter) {
            contentFilter.responseReceived(response);
        });
    }

    m_responseReceived = true;

    return m_state != State::Blocked;
}

bool ContentFilter::continueAfterDataReceived(const SharedBuffer& data, FromDocumentLoader fromDocumentLoader)
{
    RefPtr protectedClient = m_client.get();
    if (!protectedClient)
        return false;

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter received %zu bytes of data from <%{sensitive}s>.\n", data.size(), url().string().ascii().data());

        forEachContentFilterUntilBlocked([data = Ref { data }](auto& contentFilter) {
            contentFilter.addData(data);
        });
        if (fromDocumentLoader == FromDocumentLoader::Yes) {
            if (m_state == State::Allowed) {
                ASSERT(m_mainResource->dataBufferingPolicy() == DataBufferingPolicy::BufferData);
                if (RefPtr buffer = m_mainResource->resourceBuffer())
                    deliverResourceData(buffer->makeContiguous());
            }
        } else {
            if (m_state == State::Allowed) {
                deliverStoredResourceData();
                deliverResourceData(data);
            } else
                m_buffers.append(ResourceDataItem { RefPtr { &data } });
        }
        return false;
    }

    return m_state != State::Blocked;
}

bool ContentFilter::continueAfterNotifyFinished(const URL& resourceURL)
{
    RefPtr protectedClient = m_client.get();
    if (!protectedClient)
        return false;
    ASSERT_UNUSED(resourceURL, resourceURL == m_mainResourceURL);

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter will finish filtering main resource at <%{sensitive}s>.\n", url().string().ascii().data());
        forEachContentFilterUntilBlocked([](PlatformContentFilter& contentFilter) {
            contentFilter.finishedAddingData();
        });

        if (m_state != State::Blocked) {
            m_state = State::Allowed;
            deliverStoredResourceData();
        }

        if (m_state == State::Stopped)
            return false;
    }

    return m_state != State::Blocked;
}

bool ContentFilter::continueAfterNotifyFinished(CachedResource& resource)
{
    RefPtr protectedClient = m_client.get();
    if (!protectedClient)
        return true;
    ASSERT_UNUSED(resource, &resource == m_mainResource);
    if (m_mainResource->errorOccurred())
        return true;

    if (m_state == State::Filtering) {
        LOG(ContentFiltering, "ContentFilter will finish filtering main resource at <%{sensitive}s>.\n", url().string().ascii().data());
        forEachContentFilterUntilBlocked([](PlatformContentFilter& contentFilter) {
            contentFilter.finishedAddingData();
        });

        if (m_state != State::Blocked) {
            m_state = State::Allowed;
            if (RefPtr buffer = m_mainResource->resourceBuffer()) {
                ASSERT(m_mainResource->dataBufferingPolicy() == DataBufferingPolicy::BufferData);
                deliverResourceData(buffer->makeContiguous());
            }
        }

        if (m_state == State::Stopped)
            return false;
    }

    return m_state != State::Blocked;
}

template <typename Function>
void ContentFilter::forEachContentFilterUntilBlocked(Function&& getData)
{
    unsigned allowedCount = 0;
    for (Ref contentFilter : m_contentFilters) {
        if (contentFilter->needsMoreData())
            getData(contentFilter.get());

        // Still need more data for decision.
        if (contentFilter->needsMoreData())
            continue;

        if (!contentFilter->didBlockData()) {
            ++allowedCount;
            continue;
        }

        ASSERT(!m_blockingContentFilter.get());
        m_blockingContentFilter = contentFilter.get();
        didDecide(State::Blocked);
        return;
    }

    if (m_contentFilters.size() == allowedCount)
        didDecide(State::Allowed);
}

void ContentFilter::didDecide(State state)
{
    ASSERT(m_state != State::Allowed);
    ASSERT(m_state != State::Blocked);
    ASSERT(state == State::Allowed || state == State::Blocked);
#if !LOG_DISABLED
    LOG(ContentFiltering, "ContentFilter decided load should be %s for main resource at <%{sensitive}s>.\n", state == State::Allowed ? "allowed" : "blocked", url().string().ascii().data());
#endif // !LOG_DISABLED
    m_state = state;
    if (m_state != State::Blocked)
        return;

    RefPtr client = m_client.get();
    if (!client)
        return;
    RefPtr blockingContentFilter = m_blockingContentFilter.get();
    ASSERT(blockingContentFilter);
    m_blockedError = client->contentFilterDidBlock(blockingContentFilter->unblockHandler(), blockingContentFilter->unblockRequestDeniedScript());
    client->cancelMainResourceLoadForContentFilter(m_blockedError);
}

void ContentFilter::deliverResourceData(const SharedBuffer& buffer)
{
    ASSERT(m_state == State::Allowed);
    if (RefPtr protectedClient = m_client.get())
        protectedClient->dataReceivedThroughContentFilter(buffer);
}

URL ContentFilter::url()
{
    if (m_mainResource)
        return m_mainResource->url();
    return m_mainResourceURL;
}

const URL& ContentFilter::blockedPageURL()
{
    static NeverDestroyed blockedPageURL = [] () -> URL {
        RetainPtr webCoreBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebCore"));
        return adoptCF(CFBundleCopyResourceURL(webCoreBundle.get(), CFSTR("ContentFilterBlockedPage"), CFSTR("html"), nullptr)).get();
    }();
    return blockedPageURL;
}

bool ContentFilter::continueAfterSubstituteDataRequest(const DocumentLoader& activeLoader, const SubstituteData& substituteData)
{
    if (RefPtr contentFilter = activeLoader.contentFilter()) {
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

    RefPtr blockingContentFilter = m_blockingContentFilter.get();
    ASSERT(blockingContentFilter);
    RefPtr replacementData { blockingContentFilter->replacementData() };
    ResourceResponse response { URL(), "text/html"_s, static_cast<long long>(replacementData->size()), "UTF-8"_s };
    SubstituteData substituteData { WTF::move(replacementData), URL { error.failingURL() }, WTF::move(response), SubstituteData::SessionHistoryVisibility::Hidden };
    SetForScope loadingBlockedPage { m_isLoadingBlockedPage, true };
    if (RefPtr protectedClient = m_client.get())
        protectedClient->handleProvisionalLoadFailureFromContentFilter(blockedPageURL(), WTF::move(substituteData));
}

void ContentFilter::deliverStoredResourceData()
{
    for (auto& buffer : m_buffers)
        deliverResourceData(Ref { *buffer.buffer });
    m_buffers.clear();
}

#if HAVE(AUDIT_TOKEN)
void ContentFilter::setHostProcessAuditToken(const std::optional<audit_token_t>& token)
{
    for (auto& contentFilter : m_contentFilters)
        contentFilter->setHostProcessAuditToken(token);
}
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)

bool ContentFilter::isWebContentRestrictionsUnblockURL(const URL& url)
{
#if PLATFORM(MAC)
    // FIXME: Remove this when rdar://145714903 is fixed.
    if (url.host() == "127.0.0.1"_s && url.path() == "/webcontentfilter.override.local"_s)
        return true;
#endif

    return url.protocolIs(ContentFilter::urlScheme()) && equalIgnoringASCIICase(url.host(), "unblock"_s);
}

#endif

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
