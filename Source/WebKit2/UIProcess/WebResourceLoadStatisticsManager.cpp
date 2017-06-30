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
#include "WebResourceLoadStatisticsManager.h"

#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/URL.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

WebResourceLoadStatisticsManager& WebResourceLoadStatisticsManager::shared()
{
    static NeverDestroyed<WebResourceLoadStatisticsManager> webResourceLoadStatisticsManager;
    return webResourceLoadStatisticsManager;
}

WebResourceLoadStatisticsManager::WebResourceLoadStatisticsManager()
{
}

WebResourceLoadStatisticsManager::~WebResourceLoadStatisticsManager()
{
}

void WebResourceLoadStatisticsManager::setStatisticsStore(Ref<WebResourceLoadStatisticsStore>&& store)
{
    m_store = WTFMove(store);
}

void WebResourceLoadStatisticsManager::clearInMemoryStore()
{
    if (!m_store)
        return;

    m_store->clearInMemory();
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStore()
{
    if (!m_store)
        return;

    m_store->clearInMemoryAndPersistent();
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStore(std::chrono::system_clock::time_point modifiedSince)
{
    if (!m_store)
        return;

    m_store->clearInMemoryAndPersistent(modifiedSince);
}

void WebResourceLoadStatisticsManager::logUserInteraction(const URL& url)
{
    if (!m_store)
        return;

    m_store->logUserInteraction(url);
}

void WebResourceLoadStatisticsManager::clearUserInteraction(const URL& url)
{
    if (!m_store)
        return;

    m_store->clearUserInteraction(url);
}

bool WebResourceLoadStatisticsManager::hasHadUserInteraction(const URL& url)
{
    if (!m_store)
        return false;

    return m_store->hasHadUserInteraction(url);
}

void WebResourceLoadStatisticsManager::setPrevalentResource(const URL& url)
{
    if (!m_store)
        return;

    m_store->setPrevalentResource(url);
}

bool WebResourceLoadStatisticsManager::isPrevalentResource(const URL& url)
{
    if (!m_store)
        return false;

    return m_store->isPrevalentResource(url);
}

void WebResourceLoadStatisticsManager::clearPrevalentResource(const URL& url)
{
    if (!m_store)
        return;

    m_store->clearPrevalentResource(url);
}

void WebResourceLoadStatisticsManager::setGrandfathered(const URL& url, bool value)
{
    if (!m_store)
        return;

    m_store->setGrandfathered(url, value);
}

bool WebResourceLoadStatisticsManager::isGrandfathered(const URL& url)
{
    if (!m_store)
        return false;

    return m_store->isGrandfathered(url);
}

void WebResourceLoadStatisticsManager::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    if (!m_store)
        return;

    m_store->setSubframeUnderTopFrameOrigin(subframe, topFrame);
}

void WebResourceLoadStatisticsManager::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    if (!m_store)
        return;

    m_store->setSubresourceUnderTopFrameOrigin(subresource, topFrame);
}

void WebResourceLoadStatisticsManager::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    if (!m_store)
        return;

    m_store->setSubresourceUniqueRedirectTo(subresource, hostNameRedirectedTo);
}

void WebResourceLoadStatisticsManager::setTimeToLiveUserInteraction(Seconds seconds)
{
    if (!m_store)
        return;

    m_store->setTimeToLiveUserInteraction(seconds);
}

void WebResourceLoadStatisticsManager::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    if (!m_store)
        return;

    m_store->setTimeToLiveCookiePartitionFree(seconds);
}

void WebResourceLoadStatisticsManager::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    if (!m_store)
        return;

    m_store->setMinimumTimeBetweenDataRecordsRemoval(seconds);
}

void WebResourceLoadStatisticsManager::setGrandfatheringTime(Seconds seconds)
{
    if (!m_store)
        return;

    m_store->setGrandfatheringTime(seconds);
}

void WebResourceLoadStatisticsManager::fireDataModificationHandler()
{
    if (!m_store)
        return;

    m_store->fireDataModificationHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandler()
{
    if (!m_store)
        return;

    m_store->fireShouldPartitionCookiesHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)
{
    if (!m_store)
        return;

    m_store->fireShouldPartitionCookiesHandler(domainsToRemove, domainsToAdd, clearFirst);
}

void WebResourceLoadStatisticsManager::fireTelemetryHandler()
{
    if (!m_store)
        return;

    m_store->fireTelemetryHandler();
}

} // namespace WebKit
