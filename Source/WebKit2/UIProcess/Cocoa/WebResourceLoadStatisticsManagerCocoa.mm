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

#import "config.h"
#import "WebResourceLoadStatisticsManager.h"

#import "WebPreferencesKeys.h"
#import <WebCore/ResourceLoadObserver.h>

using namespace WebCore;

namespace WebKit {
    
void WebResourceLoadStatisticsManager::registerUserDefaultsIfNeeded()
{
    static dispatch_once_t initOnce;
    
    dispatch_once(&initOnce, ^ {
        const size_t hourInSeconds = 3600;
        const size_t dayInSeconds = 24 * hourInSeconds;
        
        double timeToLiveUserInteraction = [[NSUserDefaults standardUserDefaults] doubleForKey: WebPreferencesKey::resourceLoadStatisticsTimeToLiveUserInteractionKey()];
        if (timeToLiveUserInteraction > 0 && timeToLiveUserInteraction <= 30 * dayInSeconds)
            ResourceLoadObserver::sharedObserver().setTimeToLiveUserInteraction(timeToLiveUserInteraction);
        
        double timeToLiveCookiePartitionFree = [[NSUserDefaults standardUserDefaults] doubleForKey: WebPreferencesKey::resourceLoadStatisticsTimeToLiveCookiePartitionFreeKey()];
        if (timeToLiveCookiePartitionFree > 0 && timeToLiveCookiePartitionFree <= dayInSeconds)
            ResourceLoadObserver::sharedObserver().setTimeToLiveCookiePartitionFree(timeToLiveCookiePartitionFree);
        
        double reducedTimestampResolution = [[NSUserDefaults standardUserDefaults] doubleForKey: WebPreferencesKey::resourceLoadStatisticsReducedTimestampResolutionKey()];
        if (reducedTimestampResolution > 0 && reducedTimestampResolution <= hourInSeconds)
            ResourceLoadObserver::sharedObserver().setReducedTimestampResolution(reducedTimestampResolution);

        double grandfatheringTime = [[NSUserDefaults standardUserDefaults] doubleForKey: WebPreferencesKey::resourceLoadStatisticsGrandfatheringTimeKey()];
        if (grandfatheringTime > 0 && grandfatheringTime <= 7 * dayInSeconds)
            ResourceLoadObserver::sharedObserver().setGrandfatheringTime(grandfatheringTime);
    });
}

};
