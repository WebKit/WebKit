/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef PlatformMediaSessionManager_h
#define PlatformMediaSessionManager_h

#include "AudioHardwareListener.h"
#include "PlatformMediaSession.h"
#include "RemoteCommandListener.h"
#include "Settings.h"
#include "SystemSleepListener.h"
#include <map>
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class HTMLMediaElement;
class PlatformMediaSession;
class RemoteCommandListener;

class PlatformMediaSessionManager : private RemoteCommandListenerClient, private SystemSleepListener::Client, private AudioHardwareListener::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static PlatformMediaSessionManager* sharedManagerIfExists();
    WEBCORE_EXPORT static PlatformMediaSessionManager& sharedManager();
    virtual ~PlatformMediaSessionManager() { }

    bool has(PlatformMediaSession::MediaType) const;
    int count(PlatformMediaSession::MediaType) const;
    bool activeAudioSessionRequired() const;
    bool canProduceAudio() const;

    bool willIgnoreSystemInterruptions() const { return m_willIgnoreSystemInterruptions; }
    void setWillIgnoreSystemInterruptions(bool ignore) { m_willIgnoreSystemInterruptions = ignore; }

    WEBCORE_EXPORT void beginInterruption(PlatformMediaSession::InterruptionType);
    WEBCORE_EXPORT void endInterruption(PlatformMediaSession::EndInterruptionFlags);

    WEBCORE_EXPORT void applicationDidEnterForeground() const;
    WEBCORE_EXPORT void applicationWillEnterBackground() const;

    void stopAllMediaPlaybackForDocument(const Document*);
    WEBCORE_EXPORT void stopAllMediaPlaybackForProcess();

    enum SessionRestrictionFlags {
        NoRestrictions = 0,
        ConcurrentPlaybackNotPermitted = 1 << 0,
        BackgroundProcessPlaybackRestricted = 1 << 1,
        BackgroundTabPlaybackRestricted = 1 << 2,
        InterruptedPlaybackNotPermitted = 1 << 3,
    };
    typedef unsigned SessionRestrictions;

    WEBCORE_EXPORT void addRestriction(PlatformMediaSession::MediaType, SessionRestrictions);
    WEBCORE_EXPORT void removeRestriction(PlatformMediaSession::MediaType, SessionRestrictions);
    WEBCORE_EXPORT SessionRestrictions restrictions(PlatformMediaSession::MediaType);
    virtual void resetRestrictions();

    virtual bool sessionWillBeginPlayback(PlatformMediaSession&);
    virtual void sessionWillEndPlayback(PlatformMediaSession&);
    virtual bool sessionCanLoadMedia(const PlatformMediaSession&) const;
    virtual void clientCharacteristicsChanged(PlatformMediaSession&) { }

#if PLATFORM(IOS)
    virtual void configureWireLessTargetMonitoring() { }
    virtual bool hasWirelessTargetsAvailable() { return false; }
#endif

    void setCurrentSession(PlatformMediaSession&);
    PlatformMediaSession* currentSession();

    void sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSession&);
    void sessionCanProduceAudioChanged(PlatformMediaSession&);

protected:
    friend class PlatformMediaSession;
    explicit PlatformMediaSessionManager();

    void addSession(PlatformMediaSession&);
    virtual void removeSession(PlatformMediaSession&);

    Vector<PlatformMediaSession*> sessions() { return m_sessions; }

private:
    friend class Internals;

    void updateSessionState();

    // RemoteCommandListenerClient
    WEBCORE_EXPORT void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType) override;

    // AudioHardwareListenerClient
    void audioHardwareDidBecomeActive() override { }
    void audioHardwareDidBecomeInactive() override { }
    void audioOutputDeviceChanged() override;

    // SystemSleepListener
    void systemWillSleep() override;
    void systemDidWake() override;

    SessionRestrictions m_restrictions[PlatformMediaSession::WebAudio + 1];
    Vector<PlatformMediaSession*> m_sessions;
    std::unique_ptr<RemoteCommandListener> m_remoteCommandListener;
    std::unique_ptr<SystemSleepListener> m_systemSleepListener;
    RefPtr<AudioHardwareListener> m_audioHardwareListener;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_canPlayToTarget { false };
#endif

    bool m_interrupted { false };
    mutable bool m_isApplicationInBackground { false };
    bool m_willIgnoreSystemInterruptions { false };
};

}

#endif // PlatformMediaSessionManager_h
