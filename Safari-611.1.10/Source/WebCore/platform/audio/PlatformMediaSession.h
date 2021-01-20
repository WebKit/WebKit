/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "DocumentIdentifier.h"
#include "MediaSessionIdentifier.h"
#include "Timer.h"
#include <wtf/LoggerHelper.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetClient.h"
#endif

namespace WebCore {

class Document;
class MediaPlaybackTarget;
class PlatformMediaSessionClient;
class PlatformMediaSessionManager;
enum class DelayCallingUpdateNowPlaying { No, Yes };
struct NowPlayingInfo;

class PlatformMediaSession
    : public CanMakeWeakPtr<PlatformMediaSession>
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , public MediaPlaybackTargetClient
#endif
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<PlatformMediaSession> create(PlatformMediaSessionManager&, PlatformMediaSessionClient&);

    virtual ~PlatformMediaSession();

    enum class MediaType : uint8_t {
        None = 0,
        Video,
        VideoAudio,
        Audio,
        WebAudio,
    };
    MediaType mediaType() const;
    MediaType presentationType() const;

    enum State : uint8_t {
        Idle,
        Autoplaying,
        Playing,
        Paused,
        Interrupted,
    };
    State state() const { return m_state; }
    void setState(State);

    enum InterruptionType : uint8_t {
        NoInterruption,
        SystemSleep,
        EnteringBackground,
        SystemInterruption,
        SuspendedUnderLock,
        InvisibleAutoplay,
        ProcessInactive,
        PlaybackSuspended,
    };
    InterruptionType interruptionType() const { return m_interruptionType; }

    enum EndInterruptionFlags : uint8_t {
        NoFlags = 0,
        MayResumePlaying = 1 << 0,
    };

    void clientCharacteristicsChanged();

    void beginInterruption(InterruptionType);
    void endInterruption(EndInterruptionFlags);

    virtual void clientWillBeginAutoplaying();
    virtual bool clientWillBeginPlayback();
    virtual bool clientWillPausePlayback();

    void clientWillBeDOMSuspended();

    void pauseSession();
    void stopSession();

    virtual void suspendBuffering() { }
    virtual void resumeBuffering() { }
    
    typedef union {
        double asDouble;
    } RemoteCommandArgument;

    enum RemoteControlCommandType : uint8_t {
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

    enum DisplayType : uint8_t {
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
    void playbackTargetPickerWasDismissed() override { }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual bool requiresPlaybackTargetRouteMonitoring() const { return false; }
#endif

    bool activeAudioSessionRequired() const;
    bool canProduceAudio() const;
    void canProduceAudioChanged();

    virtual void resetPlaybackSessionState() { }
    String sourceApplicationIdentifier() const;

    bool hasPlayedSinceLastInterruption() const { return m_hasPlayedSinceLastInterruption; }
    void clearHasPlayedSinceLastInterruption() { m_hasPlayedSinceLastInterruption = false; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const override { return m_logIdentifier; }
    const char* logClassName() const override { return "PlatformMediaSession"; }
    WTFLogChannel& logChannel() const final;
#endif

    bool canPlayConcurrently(const PlatformMediaSession&) const;
    bool shouldOverridePauseDuringRouteChange() const;

    class AudioCaptureSource : public CanMakeWeakPtr<AudioCaptureSource> {
    public:
        virtual ~AudioCaptureSource() = default;
        virtual bool isCapturingAudio() const = 0;
    };

    virtual Optional<NowPlayingInfo> nowPlayingInfo() const;
    virtual void updateMediaUsageIfChanged() { }

    MediaSessionIdentifier mediaSessionIdentifier() const { return m_mediaSessionIdentifier; }

protected:
    PlatformMediaSession(PlatformMediaSessionManager&, PlatformMediaSessionClient&);
    PlatformMediaSessionClient& client() const { return m_client; }

    PlatformMediaSessionManager& manager();

private:
    bool processClientWillPausePlayback(DelayCallingUpdateNowPlaying);

    WeakPtr<PlatformMediaSessionManager> m_manager;
    PlatformMediaSessionClient& m_client;
    MediaSessionIdentifier m_mediaSessionIdentifier;
    State m_state { Idle };
    State m_stateToRestore { Idle };
    InterruptionType m_interruptionType { NoInterruption };
    int m_interruptionCount { 0 };
    bool m_notifyingClient { false };
    bool m_isPlayingToWirelessPlaybackTarget { false };
    bool m_hasPlayedSinceLastInterruption { false };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
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

    virtual void resumeAutoplaying() { }
    virtual void mayResumePlayback(bool shouldResume) = 0;
    virtual void suspendPlayback() = 0;

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
    virtual void playbackTargetPickerWasDismissed() { }

    virtual bool isPlayingOnSecondScreen() const { return false; }

    virtual DocumentIdentifier hostingDocumentIdentifier() const = 0;

    virtual bool hasMediaStreamSource() const { return false; }

    virtual void processIsSuspendedChanged() { }

    virtual bool shouldOverridePauseDuringRouteChange() const { return false; }

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const = 0;
#endif

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

template <> struct EnumTraits<WebCore::PlatformMediaSession::MediaType> {
    using values = EnumValues <
    WebCore::PlatformMediaSession::MediaType,
    WebCore::PlatformMediaSession::MediaType::None,
    WebCore::PlatformMediaSession::MediaType::Video,
    WebCore::PlatformMediaSession::MediaType::VideoAudio,
    WebCore::PlatformMediaSession::MediaType::Audio,
    WebCore::PlatformMediaSession::MediaType::WebAudio
    >;
};

template <> struct EnumTraits<WebCore::PlatformMediaSession::State> {
    using values = EnumValues <
    WebCore::PlatformMediaSession::State,
    WebCore::PlatformMediaSession::State::Idle,
    WebCore::PlatformMediaSession::State::Autoplaying,
    WebCore::PlatformMediaSession::State::Playing,
    WebCore::PlatformMediaSession::State::Paused,
    WebCore::PlatformMediaSession::State::Interrupted
    >;
};

template <> struct EnumTraits<WebCore::PlatformMediaSession::InterruptionType> {
    using values = EnumValues <
    WebCore::PlatformMediaSession::InterruptionType,
    WebCore::PlatformMediaSession::InterruptionType::NoInterruption,
    WebCore::PlatformMediaSession::InterruptionType::SystemSleep,
    WebCore::PlatformMediaSession::InterruptionType::EnteringBackground,
    WebCore::PlatformMediaSession::InterruptionType::SystemInterruption,
    WebCore::PlatformMediaSession::InterruptionType::SuspendedUnderLock,
    WebCore::PlatformMediaSession::InterruptionType::InvisibleAutoplay,
    WebCore::PlatformMediaSession::InterruptionType::ProcessInactive,
    WebCore::PlatformMediaSession::InterruptionType::PlaybackSuspended
    >;
};

template <> struct EnumTraits<WebCore::PlatformMediaSession::EndInterruptionFlags> {
    using values = EnumValues <
    WebCore::PlatformMediaSession::EndInterruptionFlags,
    WebCore::PlatformMediaSession::EndInterruptionFlags::NoFlags,
    WebCore::PlatformMediaSession::EndInterruptionFlags::MayResumePlaying
    >;
};

template <> struct EnumTraits<WebCore::PlatformMediaSession::RemoteControlCommandType> {
    using values = EnumValues <
    WebCore::PlatformMediaSession::RemoteControlCommandType,
    WebCore::PlatformMediaSession::RemoteControlCommandType::NoCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::PlayCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::PauseCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::StopCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::BeginSeekingBackwardCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::EndSeekingBackwardCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::BeginSeekingForwardCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::EndSeekingForwardCommand,
    WebCore::PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand
    >;
};

} // namespace WTF
