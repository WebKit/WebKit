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

#include "Timer.h"
#include <wtf/LoggerHelper.h>
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
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<PlatformMediaSession> create(PlatformMediaSessionClient&);

    PlatformMediaSession(PlatformMediaSessionClient&);
    virtual ~PlatformMediaSession();

    enum MediaType {
        None = 0,
        Video,
        VideoAudio,
        Audio,
        WebAudio,
        MediaStreamCapturingAudio,
    };
    MediaType mediaType() const;
    MediaType presentationType() const;

    enum State {
        Idle,
        Autoplaying,
        Playing,
        Paused,
        Interrupted,
    };
    State state() const { return m_state; }
    void setState(State);

    enum InterruptionType {
        NoInterruption,
        SystemSleep,
        EnteringBackground,
        SystemInterruption,
        SuspendedUnderLock,
        InvisibleAutoplay,
        ProcessInactive,
    };
    InterruptionType interruptionType() const { return m_interruptionType; }

    enum EndInterruptionFlags {
        NoFlags = 0,
        MayResumePlaying = 1 << 0,
    };

    enum Characteristics {
        HasNothing = 0,
        HasAudio = 1 << 0,
        HasVideo = 1 << 1,
    };
    typedef unsigned CharacteristicsFlags;

    CharacteristicsFlags characteristics() const;
    void clientCharacteristicsChanged();

    void beginInterruption(InterruptionType);
    void endInterruption(EndInterruptionFlags);

    virtual void clientWillBeginAutoplaying();
    virtual bool clientWillBeginPlayback();
    virtual bool clientWillPausePlayback();

    void pauseSession();
    void stopSession();
    
#if ENABLE(VIDEO)
    uint64_t uniqueIdentifier() const;
    String title() const;
    double duration() const;
    double currentTime() const;
#endif

    typedef union {
        double asDouble;
    } RemoteCommandArgument;

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
        SeekToPlaybackPositionCommand,
    };
    bool canReceiveRemoteControlCommands() const;
    void didReceiveRemoteControlCommand(RemoteControlCommandType, const RemoteCommandArgument* argument = nullptr);
    bool supportsSeeking() const;

    enum DisplayType {
        Normal,
        Fullscreen,
        Optimized,
    };
    DisplayType displayType() const;

    bool isHidden() const;
    bool isSuspended() const;

    bool shouldOverrideBackgroundLoadingRestriction() const;

    virtual bool isPlayingToWirelessPlaybackTarget() const { return m_isPlayingToWirelessPlaybackTarget; }
    void isPlayingToWirelessPlaybackTargetChanged(bool);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // MediaPlaybackTargetClient
    void setPlaybackTarget(Ref<MediaPlaybackTarget>&&) override { }
    void externalOutputDeviceAvailableDidChange(bool) override { }
    void setShouldPlayToPlaybackTarget(bool) override { }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual bool requiresPlaybackTargetRouteMonitoring() const { return false; }
#endif

    bool activeAudioSessionRequired();
    bool canProduceAudio() const;
    void canProduceAudioChanged();

    virtual void resetPlaybackSessionState() { }
    String sourceApplicationIdentifier() const;

    virtual bool allowsNowPlayingControlsVisibility() const { return false; }

    bool hasPlayedSinceLastInterruption() const { return m_hasPlayedSinceLastInterruption; }
    void clearHasPlayedSinceLastInterruption() { m_hasPlayedSinceLastInterruption = false; }

protected:
    PlatformMediaSessionClient& client() const { return m_client; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const override { return reinterpret_cast<const void*>(m_logIdentifier); }
    const char* logClassName() const override { return "PlatformMediaSession"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    PlatformMediaSessionClient& m_client;
    State m_state;
    State m_stateToRestore;
    InterruptionType m_interruptionType { NoInterruption };
    int m_interruptionCount { 0 };
    bool m_notifyingClient;
    bool m_isPlayingToWirelessPlaybackTarget { false };
    bool m_hasPlayedSinceLastInterruption { false };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    uint64_t m_logIdentifier;
#endif

    friend class PlatformMediaSessionManager;
};

class PlatformMediaSessionClient {
    WTF_MAKE_NONCOPYABLE(PlatformMediaSessionClient);
public:
    PlatformMediaSessionClient() = default;
    
    virtual PlatformMediaSession::MediaType mediaType() const = 0;
    virtual PlatformMediaSession::MediaType presentationType() const = 0;
    virtual PlatformMediaSession::DisplayType displayType() const { return PlatformMediaSession::Normal; }
    virtual PlatformMediaSession::CharacteristicsFlags characteristics() const = 0;

    virtual void resumeAutoplaying() { }
    virtual void mayResumePlayback(bool shouldResume) = 0;
    virtual void suspendPlayback() = 0;

#if ENABLE(VIDEO)
    virtual uint64_t mediaSessionUniqueIdentifier() const;
    virtual String mediaSessionTitle() const;
    virtual double mediaSessionDuration() const;
    virtual double mediaSessionCurrentTime() const;
#endif
    
    virtual bool canReceiveRemoteControlCommands() const = 0;
    virtual void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*) = 0;
    virtual bool supportsSeeking() const = 0;

    virtual bool canProduceAudio() const { return false; }
    virtual bool isSuspended() const { return false; };

    virtual bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const = 0;
    virtual bool shouldOverrideBackgroundLoadingRestriction() const { return false; }

    virtual void wirelessRoutesAvailableDidChange() { }
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) { }
    virtual bool isPlayingToWirelessPlaybackTarget() const { return false; }
    virtual void setShouldPlayToPlaybackTarget(bool) { }

    virtual bool isPlayingOnSecondScreen() const { return false; }

    virtual Document* hostingDocument() const = 0;
    virtual String sourceApplicationIdentifier() const = 0;

    virtual bool processingUserGestureForMedia() const = 0;

protected:
    virtual ~PlatformMediaSessionClient() = default;
};

String convertEnumerationToString(PlatformMediaSession::State);
String convertEnumerationToString(PlatformMediaSession::InterruptionType);
String convertEnumerationToString(PlatformMediaSession::RemoteControlCommandType);
}

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::PlatformMediaSession::State> {
    static String toString(const WebCore::PlatformMediaSession::State state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::PlatformMediaSession::InterruptionType> {
    static String toString(const WebCore::PlatformMediaSession::InterruptionType state)
    {
        return convertEnumerationToString(state);
    }
};

template <>
struct LogArgument<WebCore::PlatformMediaSession::RemoteControlCommandType> {
    static String toString(const WebCore::PlatformMediaSession::RemoteControlCommandType command)
    {
        return convertEnumerationToString(command);
    }
};

} // namespace WTF

#endif // PlatformMediaSession_h
