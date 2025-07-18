/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "MediaSessionManagerInterface.h"
#include "Timer.h"
#include <wtf/AggregateLogger.h>
#include <wtf/CancellableTask.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class Page;
class PlatformMediaSession;
struct MediaConfiguration;
struct NowPlayingInfo;
struct NowPlayingMetadata;

class PlatformMediaSessionManager : public MediaSessionManagerInterface
{
    WTF_MAKE_TZONE_ALLOCATED(PlatformMediaSessionManager);
public:
    WEBCORE_EXPORT static PlatformMediaSessionManager* singletonIfExists();
    WEBCORE_EXPORT static PlatformMediaSessionManager& singleton();

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

    static void updateNowPlayingInfoIfNecessary();
    static void updateAudioSessionCategoryIfNecessary();

    WEBCORE_EXPORT static void setShouldDeactivateAudioSession(bool);
    WEBCORE_EXPORT static bool shouldDeactivateAudioSession();

    virtual ~PlatformMediaSessionManager();

    void addSession(PlatformMediaSessionInterface&) override;
    void removeSession(PlatformMediaSessionInterface&) override;
    void setCurrentSession(PlatformMediaSessionInterface&) override;
    RefPtr<PlatformMediaSessionInterface> currentSession() const final;

    bool activeAudioSessionRequired() const final;
    bool hasActiveAudioSession() const final;
    bool canProduceAudio() const final;

    bool willIgnoreSystemInterruptions() const final { return m_willIgnoreSystemInterruptions; }
    void setWillIgnoreSystemInterruptions(bool ignore) final { m_willIgnoreSystemInterruptions = ignore; }

    WEBCORE_EXPORT void beginInterruption(PlatformMediaSession::InterruptionType) override;
    WEBCORE_EXPORT void endInterruption(PlatformMediaSession::EndInterruptionFlags) final;

    WEBCORE_EXPORT void applicationWillBecomeInactive() override;
    WEBCORE_EXPORT void applicationDidBecomeActive() override;
    WEBCORE_EXPORT void applicationWillEnterForeground(bool) override;
    WEBCORE_EXPORT void applicationDidEnterBackground(bool) override;
    WEBCORE_EXPORT void processWillSuspend() final;
    WEBCORE_EXPORT void processDidResume() final;

    bool mediaPlaybackIsPaused(std::optional<MediaSessionGroupIdentifier>) final;
    void pauseAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>) final;
    WEBCORE_EXPORT void stopAllMediaPlaybackForProcess() final;

    void suspendAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>) final;
    void resumeAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>) final;
    void suspendAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier>) final;
    void resumeAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier>) final;

    WEBCORE_EXPORT void addRestriction(PlatformMediaSession::MediaType, MediaSessionRestrictions) final;
    WEBCORE_EXPORT void removeRestriction(PlatformMediaSession::MediaType, MediaSessionRestrictions) final;
    WEBCORE_EXPORT MediaSessionRestrictions restrictions(PlatformMediaSession::MediaType) final;
    void resetRestrictions() override;

    bool sessionWillBeginPlayback(PlatformMediaSessionInterface&) override;
    void sessionWillEndPlayback(PlatformMediaSessionInterface&, DelayCallingUpdateNowPlaying) override;
    void sessionStateChanged(PlatformMediaSessionInterface&) override;
    void sessionCanProduceAudioChanged() override;

    void sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSessionInterface&) final;

    WEBCORE_EXPORT void setIsPlayingToAutomotiveHeadUnit(bool) final;
    bool isPlayingToAutomotiveHeadUnit() const final { return m_isPlayingToAutomotiveHeadUnit; }

    WEBCORE_EXPORT void setSupportsSpatialAudioPlayback(bool) final;
    std::optional<bool> supportsSpatialAudioPlaybackForConfiguration(const MediaConfiguration&) override;

    void forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& matchingCallback);

    bool processIsSuspended() const final { return m_processIsSuspended; }

    WEBCORE_EXPORT void addAudioCaptureSource(AudioCaptureSource&) final;
    WEBCORE_EXPORT void removeAudioCaptureSource(AudioCaptureSource&) final;
    void audioCaptureSourceStateChanged()  final { updateSessionState(); }
    size_t audioCaptureSourceCount() const  final { return m_audioCaptureSources.computeSize(); }

    WEBCORE_EXPORT void processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) final;

    bool isInterrupted() const  final { return !!m_currentInterruption; }
    bool hasNoSession() const final;

    WEBCORE_EXPORT void processSystemWillSleep() final;
    WEBCORE_EXPORT void processSystemDidWake() final;

    bool isApplicationInBackground() const final { return m_isApplicationInBackground; }

    WeakPtr<PlatformMediaSessionInterface> bestEligibleSessionForRemoteControls(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&, PlatformMediaSession::PlaybackControlsPurpose) final;

    std::optional<NowPlayingInfo> nowPlayingInfo() const override;
    WEBCORE_EXPORT void addNowPlayingMetadataObserver(const NowPlayingMetadataObserver&) final;
    WEBCORE_EXPORT void removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver&) final;
    bool hasActiveNowPlayingSessionInGroup(std::optional<MediaSessionGroupIdentifier>) final;

protected:
    static const std::unique_ptr<PlatformMediaSessionManager> create();
    PlatformMediaSessionManager();

    void forEachSession(NOESCAPE const Function<void(PlatformMediaSessionInterface&)>&);
    void forEachSessionInGroup(std::optional<MediaSessionGroupIdentifier>, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>&);
    bool anyOfSessions(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;

    void maybeDeactivateAudioSession();
    bool maybeActivateAudioSession();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    uint64_t logIdentifier() const final { return 0; }
    ASCIILiteral logClassName() const override { return "PlatformMediaSessionManager"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    bool willLog(WTFLogLevel) const;

    int countActiveAudioCaptureSources();

    bool computeSupportsSeeking() const;

    std::optional<bool> supportsSpatialAudioPlayback() { return m_supportsSpatialAudioPlayback; }

    void nowPlayingMetadataChanged(const NowPlayingMetadata&);
    void enqueueTaskOnMainThread(Function<void()>&&);

private:
    friend class Internals;

    bool has(PlatformMediaSession::MediaType) const;
    int count(PlatformMediaSession::MediaType) const;

    void scheduleUpdateSessionState();
    virtual void updateSessionState() { }

    Vector<WeakPtr<PlatformMediaSessionInterface>> sessionsMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;
    WeakPtr<PlatformMediaSessionInterface> firstSessionMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;

#if !RELEASE_LOG_DISABLED
    void scheduleStateLog();
    void dumpSessionStates();
#endif

    std::array<MediaSessionRestrictions, static_cast<unsigned>(PlatformMediaSession::MediaType::WebAudio) + 1> m_restrictions;
    mutable Vector<WeakPtr<PlatformMediaSessionInterface>> m_sessions;

    std::optional<PlatformMediaSession::InterruptionType> m_currentInterruption;
    mutable bool m_isApplicationInBackground { false };
    bool m_willIgnoreSystemInterruptions { false };
    bool m_processIsSuspended { false };
    bool m_isPlayingToAutomotiveHeadUnit { false };
    std::optional<bool> m_supportsSpatialAudioPlayback;

    bool m_alreadyScheduledSessionStatedUpdate { false };
#if USE(AUDIO_SESSION)
    bool m_becameActive { false };
#endif

    WeakHashSet<AudioCaptureSource> m_audioCaptureSources;
    bool m_hasScheduledSessionStateUpdate { false };

    WeakHashSet<NowPlayingMetadataObserver> m_nowPlayingMetadataObservers;
    TaskCancellationGroup m_taskGroup;

#if HAVE(SC_CONTENT_SHARING_PICKER)
    static bool s_useSCContentSharingPicker;
#endif

#if ENABLE(VP9)
    static bool m_vp9DecoderEnabled;
    static bool m_swVPDecodersAlwaysEnabled;
#endif

#if !RELEASE_LOG_DISABLED
    UniqueRef<Timer> m_stateLogTimer;
    const Ref<AggregateLogger> m_logger;
#endif
};

} // namespace WebCore
