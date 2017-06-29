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

#include "Logging.h"
#include "WebResourceLoadObserver.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/URL.h>

using namespace WebCore;

namespace WebKit {

void WebResourceLoadStatisticsManager::setPrevalentResource(const String& hostName, bool value)
{
    if (value)
        WebResourceLoadObserver::sharedObserver().setPrevalentResource(URL(URL(), hostName));
    else
        WebResourceLoadObserver::sharedObserver().clearPrevalentResource(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::isPrevalentResource(const String& hostName)
{
    return WebResourceLoadObserver::sharedObserver().isPrevalentResource(URL(URL(), hostName));
}
    
void WebResourceLoadStatisticsManager::setHasHadUserInteraction(const String& hostName, bool value)
{
    if (value)
        WebResourceLoadObserver::sharedObserver().logUserInteraction(URL(URL(), hostName));
    else
        WebResourceLoadObserver::sharedObserver().clearUserInteraction(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::hasHadUserInteraction(const String& hostName)
{
    return WebResourceLoadObserver::sharedObserver().hasHadUserInteraction(URL(URL(), hostName));
}

void WebResourceLoadStatisticsManager::setGrandfathered(const String& hostName, bool value)
{
    WebResourceLoadObserver::sharedObserver().setGrandfathered(URL(URL(), hostName), value);
}

bool WebResourceLoadStatisticsManager::isGrandfathered(const String& hostName)
{
    return WebResourceLoadObserver::sharedObserver().isGrandfathered(URL(URL(), hostName));
}

void WebResourceLoadStatisticsManager::setSubframeUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName)
{
    WebResourceLoadObserver::sharedObserver().setSubframeUnderTopFrameOrigin(URL(URL(), hostName), URL(URL(), topFrameHostName));
}

void WebResourceLoadStatisticsManager::setSubresourceUnderTopFrameOrigin(const String& hostName, const String& topFrameHostName)
{
    WebResourceLoadObserver::sharedObserver().setSubresourceUnderTopFrameOrigin(URL(URL(), hostName), URL(URL(), topFrameHostName));
}

void WebResourceLoadStatisticsManager::setSubresourceUniqueRedirectTo(const String& hostName, const String& hostNameRedirectedTo)
{
    WebResourceLoadObserver::sharedObserver().setSubresourceUniqueRedirectTo(URL(URL(), hostName), URL(URL(), hostNameRedirectedTo));
}

void WebResourceLoadStatisticsManager::setTimeToLiveUserInteraction(Seconds seconds)
{
    WebResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(seconds);
}

void WebResourceLoadStatisticsManager::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    WebResourceLoadObserver::sharedObserver().setTimeToLiveCookiePartitionFree(seconds);
}

void WebResourceLoadStatisticsManager::setMinimumTimeBetweeenDataRecordsRemoval(Seconds seconds)
{
    WebResourceLoadObserver::sharedObserver().setMinimumTimeBetweeenDataRecordsRemoval(seconds);
}

void WebResourceLoadStatisticsManager::setGrandfatheringTime(Seconds seconds)
{
    WebResourceLoadObserver::sharedObserver().setGrandfatheringTime(seconds);
}

void WebResourceLoadStatisticsManager::fireDataModificationHandler()
{
    WebResourceLoadObserver::sharedObserver().fireDataModificationHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandler()
{
    WebResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler();
}

void WebResourceLoadStatisticsManager::fireShouldPartitionCookiesHandlerForOneDomain(const String& hostName, bool value)
{
    if (value)
        WebResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler({ }, {hostName}, false);
    else
        WebResourceLoadObserver::sharedObserver().fireShouldPartitionCookiesHandler({hostName}, { }, false);
}

void WebResourceLoadStatisticsManager::fireTelemetryHandler()
{
    WebResourceLoadObserver::sharedObserver().fireTelemetryHandler();
}
    
void WebResourceLoadStatisticsManager::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WebResourceLoadStatisticsManager::setNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
}
    
void WebResourceLoadStatisticsManager::setShouldSubmitTelemetry(bool value)
{
    WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(value);
}
    
void WebResourceLoadStatisticsManager::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStore()
{
    WebResourceLoadObserver::sharedObserver().clearInMemoryAndPersistentStore();
}

void WebResourceLoadStatisticsManager::clearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    WebResourceLoadObserver::sharedObserver().clearInMemoryAndPersistentStore(std::chrono::system_clock::now() - std::chrono::hours(hours));
}
    
void WebResourceLoadStatisticsManager::resetToConsistentState()
{
    WebResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(24_h * 30.);
    WebResourceLoadObserver::sharedObserver().setTimeToLiveCookiePartitionFree(24_h);
    WebResourceLoadObserver::sharedObserver().setMinimumTimeBetweeenDataRecordsRemoval(1_h);
    WebResourceLoadObserver::sharedObserver().setGrandfatheringTime(1_h);
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(false);
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(false);
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(true);
    WebResourceLoadObserver::sharedObserver().clearInMemoryStore();
}
    
} // namespace WebKit
