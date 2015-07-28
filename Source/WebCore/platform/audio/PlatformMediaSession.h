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

#ifndef PlatformMediaSession_h
#define PlatformMediaSession_h

#include "MediaProducer.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetClient.h"
#endif

namespace WebCore {

class Document;
class MediaPlaybackTarget;
class PlatformMediaSessionClient;

class PlatformMediaSession
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    : public MediaPlaybackTargetClient
#endif
{
public:
    static std::unique_ptr<PlatformMediaSession> create(PlatformMediaSessionClient&);

    PlatformMediaSession(PlatformMediaSessionClient&);
    virtual ~PlatformMediaSession();

    enum MediaType {
        None = 0,
        Video,
        Audio,
        WebAudio,
    };
    MediaType mediaType() const;
    MediaType presentationType() const;

    enum State {
        Idle,
        Playing,
        Paused,
        Interrupted,
    };
    State state() const { return m_state; }
    void setState(State);

    enum InterruptionType {
        SystemSleep,
        EnteringBackground,
        SystemInterruption,
    };
    enum EndInterruptionFlags {
        NoFlags = 0,
        MayResumePlaying = 1 << 0,
    };

    void doInterruption();
    bool shouldDoInterruption(InterruptionType);
    void beginInterruption(InterruptionType);
    void forceInterruption(InterruptionType);
    void endInterruption(EndInterruptionFlags);

    void applicationWillEnterForeground() const;
    void applicationWillEnterBackground() const;
    void applicationDidEnterBackground(bool isSuspendedUnderLock) const;

    bool clientWillBeginPlayback();
    bool clientWillPausePlayback();

    void pauseSession();
    
    void visibilityChanged();

    String title() const;
    double duration() const;
    double currentTime() const;

    enum RemoteControlCommandType {
        NoCommand,
        PlayCommand,
        PauseCommand,
        StopCommand,
        TogglePlayPauseCommand,
        BeginSeekingBackwardCommand,
        EndSeekingBackwardCommand,
        BeginSeekingForwardCommand,
        EndSeekingForwardCommand,
    };
    bool canReceiveRemoteControlCommands() const;
    void didReceiveRemoteControlCommand(RemoteControlCommandType);

    enum DisplayType {
        Normal,
        Fullscreen,
        Optimized,
    };
    DisplayType displayType() const;

    bool isHidden() const;

    virtual bool canPlayToWirelessPlaybackTarget() const { return false; }
    virtual bool isPlayingToWirelessPlaybackTarget() const { return false; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // MediaPlaybackTargetClient
    virtual void setPlaybackTarget(Ref<MediaPlaybackTarget>&&) override { }
    virtual void externalOutputDeviceAvailableDidChange(bool) override { }
    virtual void setShouldPlayToPlaybackTarget(bool) override { }
#endif

#if PLATFORM(IOS)
    virtual bool requiresPlaybackTargetRouteMonitoring() const { return false; }
#endif

protected:
    PlatformMediaSessionClient& client() const { return m_client; }

private:
    void clientDataBufferingTimerFired();
    void updateClientDataBuffering();

    PlatformMediaSessionClient& m_client;
    Timer m_clientDataBufferingTimer;
    State m_state;
    State m_stateToRestore;
    int m_interruptionCount { 0 };
    bool m_notifyingClient;

    friend class PlatformMediaSessionManager;
};

class PlatformMediaSessionClient {
    WTF_MAKE_NONCOPYABLE(PlatformMediaSessionClient);
public:
    PlatformMediaSessionClient() { }
    
    virtual PlatformMediaSession::MediaType mediaType() const = 0;
    virtual PlatformMediaSession::MediaType presentationType() const = 0;
    virtual PlatformMediaSession::DisplayType displayType() const { return PlatformMediaSession::Normal; }

    virtual void mayResumePlayback(bool shouldResume) = 0;
    virtual void suspendPlayback() = 0;

    virtual String mediaSessionTitle() const;
    virtual double mediaSessionDuration() const;
    virtual double mediaSessionCurrentTime() const;
    
    virtual bool canReceiveRemoteControlCommands() const = 0;
    virtual void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType) = 0;

    virtual void setShouldBufferData(bool) { }
    virtual bool elementIsHidden() const { return false; }

    virtual bool overrideBackgroundPlaybackRestriction() const = 0;

    virtual void wirelessRoutesAvailableDidChange() { }
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) { }
    virtual bool canPlayToWirelessPlaybackTarget() const { return false; }
    virtual bool isPlayingToWirelessPlaybackTarget() const { return false; }
    virtual void setShouldPlayToPlaybackTarget(bool) { }

    virtual const Document* hostingDocument() const = 0;

protected:
    virtual ~PlatformMediaSessionClient() { }
};

}

#endif // PlatformMediaSession_h
