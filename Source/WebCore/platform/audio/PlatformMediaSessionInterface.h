/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/MediaSessionGroupIdentifier.h>
#include <WebCore/MediaSessionIdentifier.h>
#include <WebCore/MediaSessionManagerInterface.h>
#include <WebCore/NowPlayingInfo.h>
#include <WebCore/PlatformMediaSessionTypes.h>
#include <WebCore/Timer.h>
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Markable.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>
#include <wtf/ProcessID.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/MediaPlaybackTargetClient.h>
#endif

namespace WebCore {

class AudioCaptureSource;
class Document;
class MediaPlaybackTarget;
class PlatformMediaSession;
class PlatformMediaSessionInterface;
class PlatformMediaSessionManager;

class WEBCORE_EXPORT PlatformMediaSessionClient : public CanMakeCheckedPtr<PlatformMediaSessionClient> {
    WTF_MAKE_NONCOPYABLE(PlatformMediaSessionClient);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PlatformMediaSessionClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlatformMediaSessionClient);
public:
    PlatformMediaSessionClient() = default;

    virtual RefPtr<MediaSessionManagerInterface> sessionManager() const = 0;

    virtual PlatformMediaSessionMediaType mediaType() const = 0;
    virtual PlatformMediaSessionMediaType presentationType() const = 0;
    virtual PlatformMediaSessionDisplayType displayType() const { return PlatformMediaSessionDisplayType::Normal; }

    virtual void resumeAutoplaying() { }
    virtual void mayResumePlayback(bool shouldResume) = 0;
    virtual void suspendPlayback() = 0;

    virtual bool canReceiveRemoteControlCommands() const = 0;
    virtual void didReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&) = 0;
    virtual bool supportsSeeking() const = 0;

    virtual bool canProduceAudio() const { return false; }
    virtual bool isSuspended() const { return false; }
    virtual bool isPlaying() const { return false; }
    virtual bool isAudible() const { return false; }
    virtual bool isEnded() const { return false; }
    virtual MediaTime mediaSessionDuration() const { return MediaTime::invalidTime(); }

    virtual bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSessionInterruptionType) const = 0;
    virtual bool shouldOverrideBackgroundLoadingRestriction() const { return false; }

    virtual void wirelessRoutesAvailableDidChange() { }
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) { }
    virtual bool isPlayingToWirelessPlaybackTarget() const { return false; }
    virtual void setShouldPlayToPlaybackTarget(bool) { }
    virtual void playbackTargetPickerWasDismissed() { }

    virtual bool isPlayingOnSecondScreen() const { return false; }

    virtual std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const = 0;

    virtual bool hasMediaStreamSource() const { return false; }

    virtual void processIsSuspendedChanged() { }

    virtual bool shouldOverridePauseDuringRouteChange() const { return false; }

    virtual bool isNowPlayingEligible() const { return false; }
    virtual std::optional<NowPlayingInfo> nowPlayingInfo() const { return { }; }
    virtual WeakPtr<PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>&, PlatformMediaSessionPlaybackControlsPurpose) { return nullptr; }

    virtual bool isRemoteSessionClientProxy() const;

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const = 0;
    virtual uint64_t logIdentifier() const = 0;
#endif

protected:
    virtual ~PlatformMediaSessionClient();
};

inline bool PlatformMediaSessionClient::isRemoteSessionClientProxy() const { return false; }

WEBCORE_EXPORT PlatformMediaSessionClient& NODELETE emptyPlatformMediaSessionClient();

class WEBCORE_EXPORT PlatformMediaSessionInterface
    : public RefCountedAndCanMakeWeakPtr<PlatformMediaSessionInterface>
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , public MediaPlaybackTargetClient
#endif
{
public:
    virtual ~PlatformMediaSessionInterface() = default;

    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<PlatformMediaSessionInterface>);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void ref() const final { RefCountedAndCanMakeWeakPtr::ref(); }
    void deref() const final { RefCountedAndCanMakeWeakPtr::deref(); }
#endif

    virtual void setActive(bool) = 0;

    using MediaType = PlatformMediaSessionMediaType;
    virtual MediaType mediaType() const { return protect(client())->mediaType(); }
    virtual MediaType presentationType() const { return protect(client())->presentationType(); }

    using State = PlatformMediaSessionState;
    virtual State state() const = 0;
    virtual void setState(State) = 0;
    virtual State stateToRestore() const = 0;

    using InterruptionType = PlatformMediaSessionInterruptionType;
    virtual InterruptionType interruptionType() const = 0;

    using EndInterruptionFlags = PlatformMediaSessionEndInterruptionFlags;
    virtual void beginInterruption(InterruptionType) = 0;
    virtual void endInterruption(OptionSet<EndInterruptionFlags>) = 0;

    virtual void clientCharacteristicsChanged(bool) = 0;

    virtual void clientWillBeginAutoplaying() = 0;
    virtual Ref<GenericPromise> clientWillBeginPlayback() = 0;
    // Called by the manager, inside the serialized admission region, once activation has succeeded and
    // immediately before enforceConcurrentPlaybackRestriction(). stateAtStart is state() as captured
    // synchronously when this admission began.
    // Returns true if the session actually transitioned to Playing (so enforcing exclusivity on its behalf
    // makes sense); false if it stayed paused (e.g. a pause() raced the admission), in which case the
    // caller must not evict other sessions.
    virtual bool commitPlaybackAdmission(State stateAtStart) = 0;
    virtual bool clientWillPausePlayback() = 0;

    virtual void clientWillBeDOMSuspended() = 0;

    virtual void pauseSession() = 0;
    virtual void stopSession() = 0;

    virtual void suspendBuffering() { }
    virtual void resumeBuffering() { }

    using RemoteCommandArgument = PlatformMediaSessionRemoteCommandArgument;
    using RemoteControlCommandType = PlatformMediaSessionRemoteControlCommandType;
    bool canReceiveRemoteControlCommands() const { return protect(client())->canReceiveRemoteControlCommands(); }
    virtual void didReceiveRemoteControlCommand(RemoteControlCommandType, const RemoteCommandArgument&) = 0;

    using DisplayType = PlatformMediaSessionDisplayType;
    virtual DisplayType displayType() const { return protect(client())->displayType(); }

    virtual bool supportsSeeking() const { return protect(client())->supportsSeeking(); }
    virtual bool isSuspended() const { return protect(client())->isSuspended(); }
    virtual bool isPlaying() const { return protect(client())->isPlaying(); }
    virtual bool isAudible() const { return protect(client())->isAudible(); }
    virtual bool isEnded() const { return protect(client())->isEnded(); }
    virtual MediaTime duration() const { return protect(client())->mediaSessionDuration(); }

    virtual bool shouldOverrideBackgroundLoadingRestriction() const { return protect(client())->shouldOverrideBackgroundLoadingRestriction(); }

    virtual bool isPlayingToWirelessPlaybackTarget() const { return false; }
    virtual void isPlayingToWirelessPlaybackTargetChanged(bool) = 0;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    // MediaPlaybackTargetClient
    virtual void setPlaybackTarget(Ref<MediaPlaybackTarget>&&) { }
    virtual void externalOutputDeviceAvailableDidChange(bool) { }
    virtual void setShouldPlayToPlaybackTarget(bool) { }
    virtual void playbackTargetPickerWasDismissed() { }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual bool requiresPlaybackTargetRouteMonitoring() const { return false; }
#endif

    virtual bool blockedBySystemInterruption() const = 0;
    virtual bool activeAudioSessionRequired() const = 0;

    // Whether the client is allowed to begin playing right now. A session can be Playing and audible
    // while its client is no longer allowed to start.
    virtual bool playbackPermitted() const;

    virtual bool canProduceAudio() const { return protect(client())->canProduceAudio(); }
    virtual bool hasMediaStreamSource() const { return protect(client())->hasMediaStreamSource(); }
    virtual void canProduceAudioChanged() = 0;

    virtual void resetPlaybackSessionState() { }

    virtual bool hasPlayedAudiblySinceLastInterruption() const { return m_hasPlayedAudiblySinceLastInterruption; }
    virtual void setHasPlayedAudiblySinceLastInterruption(bool hasPlayed) { m_hasPlayedAudiblySinceLastInterruption = hasPlayed; }

    virtual bool preparingToPlay() const = 0;

    // Whether a queued admission for this session, once its turn comes up in
    // MediaSessionManagerInterface::sessionWillBeginPlayback()'s serialization chain, should still
    // proceed. Defaults to preparingToPlay(): a local session may have had pause() called on it while
    // queued, in which case it no longer wants to play. Overridden by RemoteMediaSessionProxy, whose
    // preparingToPlay() mirrors a snapshot the WebContent process takes *after* it already committed the
    // admission locally — always false for that message, which is not evidence of a race here; any real
    // invalidation after commit arrives via a separate, later-ordered IPC message instead.
    virtual bool admissionStillValid() const;

    // A session whose own admission is in flight has not reached Playing yet but intends to; treat it
    // as playing for the purpose of not disturbing it. Do not use this in enforceConcurrentPlaybackRestriction()'s
    // eviction predicate — that one deliberately checks state() alone, since a merely-queued or
    // in-flight session there is not a legitimate eviction target (see MediaSessionManagerInterface.cpp).
    bool isPlayingOrPreparingToPlay() const;

    virtual bool canPlayConcurrently(const PlatformMediaSessionInterface&) const = 0;
    virtual bool shouldOverridePauseDuringRouteChange() const { return protect(client())->shouldOverridePauseDuringRouteChange(); }

    virtual std::optional<NowPlayingInfo> nowPlayingInfo() const { return protect(client())->nowPlayingInfo(); }
    virtual bool isNowPlayingEligible() const { return protect(client())->isNowPlayingEligible(); }

    using PlaybackControlsPurpose = PlatformMediaSessionPlaybackControlsPurpose;
    virtual WeakPtr<PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>&, PlaybackControlsPurpose) = 0;

    virtual void updateMediaUsageIfChanged() { }

    virtual bool isLongEnoughForMainContent() const { return false; }
    virtual bool isLargeEnoughForMainContent() const;
    virtual Markable<MonotonicTime> mostRecentUserInteractionTime() const;

    void setMediaSessionIdentifier(MediaSessionIdentifier);
    virtual MediaSessionIdentifier mediaSessionIdentifier() const;

    virtual bool isActiveNowPlayingSession() const = 0;
    virtual void setActiveNowPlayingSession(bool) = 0;

    virtual void audioSessionCategoryChanged(AudioSessionCategory, AudioSessionMode, RouteSharingPolicy) { }

#if !RELEASE_LOG_DISABLED
    virtual String description() const = 0;
#endif

    virtual std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const { return protect(client())->mediaSessionGroupIdentifier(); }
    virtual bool isPlayingOnSecondScreen() const { return protect(client())->isPlayingOnSecondScreen(); }

    virtual bool isRemoteSessionProxy() const;

    void invalidateClient() { m_client = emptyPlatformMediaSessionClient(); }
    PlatformMediaSessionClient& client() const;

#if !RELEASE_LOG_DISABLED
    virtual const Logger& logger() const = 0;
    virtual uint64_t logIdentifier() const = 0;
    virtual ASCIILiteral logClassName() const = 0;
    virtual WTFLogChannel& logChannel() const = 0;
#endif

protected:
    PlatformMediaSessionInterface(PlatformMediaSessionClient& client)
        : m_client(client)
        , m_mediaSessionIdentifier(MediaSessionIdentifier::generate())
    {
    }

    RefPtr<MediaSessionManagerInterface> sessionManager() const { return protect(client())->sessionManager(); }

private:
    CheckedRef<PlatformMediaSessionClient> m_client;
    MediaSessionIdentifier m_mediaSessionIdentifier;
    bool m_hasPlayedAudiblySinceLastInterruption { false };
};

inline void PlatformMediaSessionInterface::setMediaSessionIdentifier(MediaSessionIdentifier identifier) { m_mediaSessionIdentifier = identifier; }
inline PlatformMediaSessionClient& PlatformMediaSessionInterface::client() const { return m_client; }
inline bool PlatformMediaSessionInterface::isRemoteSessionProxy() const { return false; }
inline bool PlatformMediaSessionInterface::playbackPermitted() const { return true; }
inline bool PlatformMediaSessionInterface::admissionStillValid() const { return preparingToPlay(); }
inline bool PlatformMediaSessionInterface::isPlayingOrPreparingToPlay() const { return state() == State::Playing || preparingToPlay(); }
inline MediaSessionIdentifier PlatformMediaSessionInterface::mediaSessionIdentifier() const { return m_mediaSessionIdentifier; }

} // namespace WebCore
