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
#include "WKResourceLoadStatisticsManager.h"

#include "WKAPICast.h"
#include "WebResourceLoadStatisticsManager.h"
#include "WebResourceLoadStatisticsStore.h"
#include <wtf/Seconds.h>

using namespace WebCore;
using namespace WebKit;

void WKResourceLoadStatisticsManagerSetPrevalentResource(WKStringRef hostName, bool value)
{
    if (value)
        WebResourceLoadStatisticsManager::shared().setPrevalentResource(URL(URL(), toWTFString(hostName)));
    else
        WebResourceLoadStatisticsManager::shared().clearPrevalentResource(URL(URL(), toWTFString(hostName)));
}

bool WKResourceLoadStatisticsManagerIsPrevalentResource(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::shared().isPrevalentResource(URL(URL(), toWTFString(hostName)));
}

void WKResourceLoadStatisticsManagerSetHasHadUserInteraction(WKStringRef hostName, bool value)
{
    if (value)
        WebResourceLoadStatisticsManager::shared().logUserInteraction(URL(URL(), toWTFString(hostName)));
    else
        WebResourceLoadStatisticsManager::shared().clearUserInteraction(URL(URL(), toWTFString(hostName)));
}

// FIXME: This API name is wrong.
bool WKResourceLoadStatisticsManagerIsHasHadUserInteraction(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::shared().hasHadUserInteraction(URL(URL(), toWTFString(hostName)));
}

void WKResourceLoadStatisticsManagerSetGrandfathered(WKStringRef hostName, bool value)
{
    WebResourceLoadStatisticsManager::shared().setGrandfathered(URL(URL(), toWTFString(hostName)), value);
}

bool WKResourceLoadStatisticsManagerIsGrandfathered(WKStringRef hostName)
{
    return WebResourceLoadStatisticsManager::shared().isGrandfathered(URL(URL(), toWTFString(hostName)));
}

void WKResourceLoadStatisticsManagerSetSubframeUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    WebResourceLoadStatisticsManager::shared().setSubframeUnderTopFrameOrigin(URL(URL(), toWTFString(hostName)), URL(URL(), toWTFString(topFrameHostName)));
}

void WKResourceLoadStatisticsManagerSetSubresourceUnderTopFrameOrigin(WKStringRef hostName, WKStringRef topFrameHostName)
{
    WebResourceLoadStatisticsManager::shared().setSubresourceUnderTopFrameOrigin(URL(URL(), toWTFString(hostName)), URL(URL(), toWTFString(topFrameHostName)));
}

void WKResourceLoadStatisticsManagerSetSubresourceUniqueRedirectTo(WKStringRef hostName, WKStringRef hostNameRedirectedTo)
{
    WebResourceLoadStatisticsManager::shared().setSubresourceUniqueRedirectTo(URL(URL(), toWTFString(hostName)), URL(URL(), toWTFString(hostNameRedirectedTo)));
}

void WKResourceLoadStatisticsManagerSetTimeToLiveUserInteraction(double seconds)
{
    WebResourceLoadStatisticsManager::shared().setTimeToLiveUserInteraction(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetTimeToLiveCookiePartitionFree(double seconds)
{
    WebResourceLoadStatisticsManager::shared().setTimeToLiveCookiePartitionFree(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetMinimumTimeBetweenDataRecordsRemoval(double seconds)
{
    WebResourceLoadStatisticsManager::shared().setMinimumTimeBetweenDataRecordsRemoval(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerSetGrandfatheringTime(double seconds)
{
    WebResourceLoadStatisticsManager::shared().setGrandfatheringTime(Seconds { seconds });
}

void WKResourceLoadStatisticsManagerFireDataModificationHandler()
{
    WebResourceLoadStatisticsManager::shared().fireDataModificationHandler();
}

void WKResourceLoadStatisticsManagerFireShouldPartitionCookiesHandler()
{
    WebResourceLoadStatisticsManager::shared().fireShouldPartitionCookiesHandler();
}

void WKResourceLoadStatisticsManagerFireShouldPartitionCookiesHandlerForOneDomain(WKStringRef hostName, bool value)
{
    if (value)
        WebResourceLoadStatisticsManager::shared().fireShouldPartitionCookiesHandler({ }, { toWTFString(hostName) }, false);
    else
        WebResourceLoadStatisticsManager::shared().fireShouldPartitionCookiesHandler({ toWTFString(hostName) }, { }, false);
}

void WKResourceLoadStatisticsManagerFireTelemetryHandler()
{
    WebResourceLoadStatisticsManager::shared().fireTelemetryHandler();
}

void WKResourceLoadStatisticsManagerSetNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WKResourceLoadStatisticsManagerSetShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WKResourceLoadStatisticsManagerSetNotifyPagesWhenTelemetryWasCaptured(bool value)
{
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(value);
}

void WKResourceLoadStatisticsManagerSetShouldSubmitTelemetry(bool value)
{
    WebResourceLoadStatisticsStore::setShouldSubmitTelemetry(value);
}

void WKResourceLoadStatisticsManagerClearInMemoryAndPersistentStore()
{
    WebResourceLoadStatisticsManager::shared().clearInMemoryAndPersistentStore();
}

void WKResourceLoadStatisticsManagerClearInMemoryAndPersistentStoreModifiedSinceHours(unsigned hours)
{
    WebResourceLoadStatisticsManager::shared().clearInMemoryAndPersistentStore(std::chrono::system_clock::now() - std::chrono::hours(hours));
}

void WKResourceLoadStatisticsManagerResetToConsistentState()
{
    WebResourceLoadStatisticsManager::shared().setTimeToLiveUserInteraction(24_h * 30.);
    WebResourceLoadStatisticsManager::shared().setTimeToLiveCookiePartitionFree(24_h);
    WebResourceLoadStatisticsManager::shared().setMinimumTimeBetweenDataRecordsRemoval(1_h);
    WebResourceLoadStatisticsManager::shared().setGrandfatheringTime(1_h);
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(false);
    WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(false);
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(true);
    WebResourceLoadStatisticsManager::shared().clearInMemoryStore();
}
