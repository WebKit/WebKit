/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "WebPage.h"

#import "LoadParameters.h"
#import "WebPageProxyMessages.h"
#import <WebCore/PlatformMediaSessionManager.h>

#if PLATFORM(COCOA)

namespace WebKit {
using namespace WebCore;

void WebPage::platformDidReceiveLoadParameters(const LoadParameters& loadParameters)
{
    m_dataDetectionContext = loadParameters.dataDetectionContext;
}

void WebPage::requestActiveNowPlayingSessionInfo(CallbackID callbackID)
{
    bool hasActiveSession = false;
    String title = emptyString();
    double duration = NAN;
    double elapsedTime = NAN;
    uint64_t uniqueIdentifier = 0;
    bool registeredAsNowPlayingApplication = false;
    if (auto* sharedManager = WebCore::PlatformMediaSessionManager::sharedManagerIfExists()) {
        hasActiveSession = sharedManager->hasActiveNowPlayingSession();
        title = sharedManager->lastUpdatedNowPlayingTitle();
        duration = sharedManager->lastUpdatedNowPlayingDuration();
        elapsedTime = sharedManager->lastUpdatedNowPlayingElapsedTime();
        uniqueIdentifier = sharedManager->lastUpdatedNowPlayingInfoUniqueIdentifier();
        registeredAsNowPlayingApplication = sharedManager->registeredAsNowPlayingApplication();
    }

    send(Messages::WebPageProxy::NowPlayingInfoCallback(hasActiveSession, registeredAsNowPlayingApplication, title, duration, elapsedTime, uniqueIdentifier, callbackID));
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
