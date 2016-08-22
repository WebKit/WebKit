/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ResourceLoadObserver.h"

#include "Document.h"
#include "Frame.h"
#include "Logging.h"
#include "MainFrame.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "ResourceLoadStatistics.h"
#include "ResourceLoadStatisticsStore.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

ResourceLoadObserver& ResourceLoadObserver::sharedObserver()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}

void ResourceLoadObserver::setStatisticsStore(Ref<ResourceLoadStatisticsStore>&& store)
{
    m_store = WTFMove(store);
}

void ResourceLoadObserver::logFrameNavigation(const Frame& frame, const Frame& topFrame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    if (!m_store)
        return;

    ASSERT(frame.document());
    ASSERT(topFrame.document());
    ASSERT(topFrame.page());

    bool needPrivacy = topFrame.page() ? topFrame.page()->usesEphemeralSession() : false;
    if (needPrivacy)
        return;

    bool isRedirect = !redirectResponse.isNull();
    bool isMainFrame = frame.isMainFrame();
    const URL& sourceURL = frame.document()->url();
    const URL& targetURL = newRequest.url();
    const URL& mainFrameURL = topFrame.document()->url();
    
    if (!targetURL.isValid() || !mainFrameURL.isValid())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || targetPrimaryDomain == sourcePrimaryDomain)
        return;
    
    auto targetOrigin = SecurityOrigin::create(targetURL);
    auto targetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
    
    if (isMainFrame)
        targetStatistics.topFrameHasBeenNavigatedToBefore = true;
    else {
        targetStatistics.subframeHasBeenLoadedBefore = true;

        auto mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
        targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain);
    }
    
    if (isRedirect) {
        auto& redirectingOriginResourceStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        
        if (m_store->isPrevalentResource(targetPrimaryDomain))
            redirectingOriginResourceStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);
        
        if (isMainFrame) {
            ++targetStatistics.topFrameHasBeenRedirectedTo;
            ++redirectingOriginResourceStatistics.topFrameHasBeenRedirectedFrom;
        } else {
            ++targetStatistics.subframeHasBeenRedirectedTo;
            ++redirectingOriginResourceStatistics.subframeHasBeenRedirectedFrom;
            redirectingOriginResourceStatistics.subframeUniqueRedirectsTo.add(targetPrimaryDomain);
            
            ++targetStatistics.subframeSubResourceCount;
        }
    } else {
        if (sourcePrimaryDomain.isNull() || sourcePrimaryDomain.isEmpty() || sourcePrimaryDomain == "nullOrigin") {
            if (isMainFrame)
                ++targetStatistics.topFrameInitialLoadCount;
            else
                ++targetStatistics.subframeSubResourceCount;
        } else {
            auto& sourceOriginResourceStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

            if (isMainFrame) {
                ++sourceOriginResourceStatistics.topFrameHasBeenNavigatedFrom;
                ++targetStatistics.topFrameHasBeenNavigatedTo;
            } else {
                ++sourceOriginResourceStatistics.subframeHasBeenNavigatedFrom;
                ++targetStatistics.subframeHasBeenNavigatedTo;
            }
        }
    }

    m_store->setResourceStatisticsForPrimaryDomain(targetPrimaryDomain, WTFMove(targetStatistics));
    m_store->fireDataModificationHandler();
}
    
void ResourceLoadObserver::logSubresourceLoading(const Frame* frame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    if (!m_store)
        return;

    bool needPrivacy = (frame && frame->page()) ? frame->page()->usesEphemeralSession() : false;
    if (needPrivacy)
        return;
    
    bool isRedirect = !redirectResponse.isNull();
    const URL& sourceURL = redirectResponse.url();
    const URL& targetURL = newRequest.url();
    const URL& mainFrameURL = frame ? frame->mainFrame().document()->url() : URL();
    
    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || targetPrimaryDomain == sourcePrimaryDomain)
        return;

    auto& targetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

    auto mainFrameOrigin = SecurityOrigin::create(mainFrameURL);
    targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);

    if (isRedirect) {
        auto& redirectingOriginStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        
        // We just inserted to the store, so we need to reget 'targetStatistics'
        auto& updatedTargetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

        if (m_store->isPrevalentResource(targetPrimaryDomain))
            redirectingOriginStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);
        
        ++redirectingOriginStatistics.subresourceHasBeenRedirectedFrom;
        ++updatedTargetStatistics.subresourceHasBeenRedirectedTo;

        redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain);

        ++updatedTargetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
        
        updatedTargetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(updatedTargetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    } else {
        ++targetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
        
        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    }
    
    m_store->fireDataModificationHandler();
}
    
void ResourceLoadObserver::logUserInteraction(const Document& document)
{
    if (!Settings::resourceLoadStatisticsEnabled())
        return;

    if (!m_store)
        return;

    bool needPrivacy = document.page() ? document.page()->usesEphemeralSession() : false;
    if (needPrivacy)
        return;

    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(document.url()));
    statistics.hadUserInteraction = true;
    m_store->fireDataModificationHandler();
}
    
String ResourceLoadObserver::primaryDomain(const URL& url)
{
    String host = url.host();
    Vector<String> hostSplitOnDot;
    
    host.split('.', false, hostSplitOnDot);

    String primaryDomain;
    if (host.isNull())
        primaryDomain = "nullOrigin";
    else if (hostSplitOnDot.size() < 3)
        primaryDomain = host;
    else {
        // Skip TLD and then up to two domains smaller than 4 characters
        int primaryDomainCutOffIndex = hostSplitOnDot.size() - 2;

        // Start with TLD as a given part
        size_t numberOfParts = 1;
        for (; primaryDomainCutOffIndex >= 0; --primaryDomainCutOffIndex) {
            ++numberOfParts;

            // We have either a domain part that's 4 chars or longer, or 3 domain parts including TLD
            if (hostSplitOnDot.at(primaryDomainCutOffIndex).length() >= 4 || numberOfParts >= 3)
                break;
        }

        if (primaryDomainCutOffIndex < 0)
            primaryDomain = host;
        else {
            StringBuilder builder;
            builder.append(hostSplitOnDot.at(primaryDomainCutOffIndex));
            for (size_t j = primaryDomainCutOffIndex + 1; j < hostSplitOnDot.size(); ++j) {
                builder.append('.');
                builder.append(hostSplitOnDot[j]);
            }
            primaryDomain = builder.toString();
        }
    }

    return primaryDomain;
}

String ResourceLoadObserver::statisticsForOrigin(const String& origin)
{
    return m_store ? m_store->statisticsForOrigin(origin) : emptyString();
}

}
