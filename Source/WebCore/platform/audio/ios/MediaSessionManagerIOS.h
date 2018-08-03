/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(IOS)

#include "MediaSessionManagerCocoa.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS WebMediaSessionHelper;

#if defined(__OBJC__) && __OBJC__
extern NSString* WebUIApplicationWillResignActiveNotification;
extern NSString* WebUIApplicationWillEnterForegroundNotification;
extern NSString* WebUIApplicationDidBecomeActiveNotification;
extern NSString* WebUIApplicationDidEnterBackgroundNotification;
#endif

namespace WebCore {

class MediaSessionManageriOS : public MediaSessionManagerCocoa {
public:
    virtual ~MediaSessionManageriOS();

    void externalOutputDeviceAvailableDidChange();
    bool hasWirelessTargetsAvailable() override;

private:
    friend class PlatformMediaSessionManager;

    MediaSessionManageriOS();

    void removeSession(PlatformMediaSession&) override;

    bool sessionWillBeginPlayback(PlatformMediaSession&) override;
    void sessionWillEndPlayback(PlatformMediaSession&) override;
    void clientCharacteristicsChanged(PlatformMediaSession&) override;

    void updateNowPlayingInfo();
    
    void resetRestrictions() override;

    void configureWireLessTargetMonitoring() override;

    bool hasActiveNowPlayingSession() const final { return m_nowPlayingActive; }
    String lastUpdatedNowPlayingTitle() const final { return m_reportedTitle; }
    double lastUpdatedNowPlayingDuration() const final { return m_reportedDuration; }
    double lastUpdatedNowPlayingElapsedTime() const final { return m_reportedCurrentTime; }
    uint64_t lastUpdatedNowPlayingInfoUniqueIdentifier() const final { return m_lastUpdatedNowPlayingInfoUniqueIdentifier; }
    bool registeredAsNowPlayingApplication() const final { return m_nowPlayingActive; }

    PlatformMediaSession* nowPlayingEligibleSession();
    
    RetainPtr<WebMediaSessionHelper> m_objcObserver;
    double m_reportedRate { 0 };
    double m_reportedDuration { 0 };
    double m_reportedCurrentTime { 0 };
    uint64_t m_lastUpdatedNowPlayingInfoUniqueIdentifier { 0 };
    String m_reportedTitle;
    bool m_nowPlayingActive { false };
};

} // namespace WebCore

#endif // PLATFORM(IOS)
