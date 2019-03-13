/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include "GenericTaskQueue.h"
#include "PlatformMediaSessionManager.h"

namespace WebCore {

class MediaSessionManagerCocoa : public PlatformMediaSessionManager {
public:
    virtual ~MediaSessionManagerCocoa() = default;
    
    void updateSessionState() override;
    void beginInterruption(PlatformMediaSession::InterruptionType) override;

    bool hasActiveNowPlayingSession() const final { return m_nowPlayingActive; }
    String lastUpdatedNowPlayingTitle() const final { return m_lastUpdatedNowPlayingTitle; }
    double lastUpdatedNowPlayingDuration() const final { return m_lastUpdatedNowPlayingDuration; }
    double lastUpdatedNowPlayingElapsedTime() const final { return m_lastUpdatedNowPlayingElapsedTime; }
    uint64_t lastUpdatedNowPlayingInfoUniqueIdentifier() const final { return m_lastUpdatedNowPlayingInfoUniqueIdentifier; }
    bool registeredAsNowPlayingApplication() const final { return m_registeredAsNowPlayingApplication; }
    void prepareToSendUserMediaPermissionRequest() final;

protected:
    void scheduleUpdateNowPlayingInfo() override;
    void updateNowPlayingInfo();
    void removeSession(PlatformMediaSession&) override;
    
    bool sessionWillBeginPlayback(PlatformMediaSession&) override;
    void sessionWillEndPlayback(PlatformMediaSession&) override;
    void sessionDidEndRemoteScrubbing(const PlatformMediaSession&) override;
    void clientCharacteristicsChanged(PlatformMediaSession&) override;
    void sessionCanProduceAudioChanged(PlatformMediaSession&) override;

    virtual void providePresentingApplicationPIDIfNecessary() { }

    PlatformMediaSession* nowPlayingEligibleSession();

private:
#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "MediaSessionManagerCocoa"; }
#endif

    bool m_nowPlayingActive { false };
    bool m_registeredAsNowPlayingApplication { false };

    // For testing purposes only.
    String m_lastUpdatedNowPlayingTitle;
    double m_lastUpdatedNowPlayingDuration { NAN };
    double m_lastUpdatedNowPlayingElapsedTime { NAN };
    uint64_t m_lastUpdatedNowPlayingInfoUniqueIdentifier { 0 };

    GenericTaskQueue<Timer> m_nowPlayingUpdateTaskQueue;
};

}

#endif // PLATFORM(COCOA)
