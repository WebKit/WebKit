/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef MediaSessionManageriOS_h
#define MediaSessionManageriOS_h

#if PLATFORM(IOS)

#include "PlatformMediaSessionManager.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS WebMediaSessionHelper;

#if defined(__OBJC__) && __OBJC__
extern NSString* WebUIApplicationWillResignActiveNotification;
extern NSString* WebUIApplicationWillEnterForegroundNotification;
extern NSString* WebUIApplicationDidBecomeActiveNotification;
extern NSString* WebUIApplicationDidEnterBackgroundNotification;
#endif

namespace WebCore {

class MediaSessionManageriOS : public PlatformMediaSessionManager {
public:
    virtual ~MediaSessionManageriOS();

    void externalOutputDeviceAvailableDidChange();
    bool hasWirelessTargetsAvailable() override;
    void applicationDidEnterBackground(bool isSuspendedUnderLock);
    void applicationWillEnterForeground(bool isSuspendedUnderLock);

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

    bool sessionCanLoadMedia(const PlatformMediaSession&) const override;

    PlatformMediaSession* nowPlayingEligibleSession();
    
    RetainPtr<WebMediaSessionHelper> m_objcObserver;
    RetainPtr<NSMutableDictionary> m_nowPlayingInfo;
    bool m_isInBackground { false };
};

} // namespace WebCore

#endif // MediaSessionManageriOS_h

#endif // PLATFORM(IOS)
