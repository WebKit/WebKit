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
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/URL.h>

using namespace WebCore;

namespace WebKit {

void WebResourceLoadStatisticsManager::setPrevalentResource(const String& hostName, bool value)
{
    if (value)
        WebCore::ResourceLoadObserver::sharedObserver().setPrevalentResource(URL(URL(), hostName));
    else
        WebCore::ResourceLoadObserver::sharedObserver().clearPrevalentResource(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::isPrevalentResource(const String& hostName)
{
    return WebCore::ResourceLoadObserver::sharedObserver().isPrevalentResource(URL(URL(), hostName));
}
    
void WebResourceLoadStatisticsManager::setHasHadUserInteraction(const String& hostName, bool value)
{
    if (value)
        WebCore::ResourceLoadObserver::sharedObserver().logUserInteraction(URL(URL(), hostName));
    else
        WebCore::ResourceLoadObserver::sharedObserver().clearUserInteraction(URL(URL(), hostName));
}

bool WebResourceLoadStatisticsManager::hasHadUserInteraction(const String& hostName)
{
    return WebCore::ResourceLoadObserver::sharedObserver().hasHadUserInteraction(URL(URL(), hostName));
}

void WebResourceLoadStatisticsManager::setTimeToLiveUserInteraction(double seconds)
{
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(seconds);
}

void WebResourceLoadStatisticsManager::fireDataModificationHandler()
{
    WebCore::ResourceLoadObserver::sharedObserver().fireDataModificationHandler();
}

void WebResourceLoadStatisticsManager::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(value);
}

void WebResourceLoadStatisticsManager::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(value);
}

void WebResourceLoadStatisticsManager::setMinimumTimeBetweeenDataRecordsRemoval(double seconds)
{
    WebResourceLoadStatisticsStore::setMinimumTimeBetweeenDataRecordsRemoval(seconds);
}

void WebResourceLoadStatisticsManager::resetToConsistentState()
{
    WebCore::ResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(2592000);
    WebResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(false);
    WebResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(true);
    WebResourceLoadStatisticsStore::setMinimumTimeBetweeenDataRecordsRemoval(60);
    auto store = WebCore::ResourceLoadObserver::sharedObserver().statisticsStore();
    if (store)
        store->clear();
}
    
} // namespace WebKit
