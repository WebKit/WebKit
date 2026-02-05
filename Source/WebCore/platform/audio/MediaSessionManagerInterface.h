/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
#include <WebCore/MediaUniqueIdentifier.h>
#include <WebCore/NowPlayingInfo.h>
#include <WebCore/NowPlayingMetadataObserver.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PlatformMediaSessionTypes.h>
#include <wtf/AggregateLogger.h>
#include <wtf/CancellableTask.h>
#include <wtf/LoggerHelper.h>
#include <wtf/ProcessID.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Page;
class PlatformMediaSessionInterface;
struct NowPlayingMetadata;
struct PlatformMediaConfiguration;

class WEBCORE_EXPORT MediaSessionManagerInterface
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaSessionManagerInterface>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(MediaSessionManagerInterface);
public:

    virtual ~MediaSessionManagerInterface();

    virtual WeakPtr<PlatformMediaSessionInterface> bestEligibleSessionForRemoteControls(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&, PlatformMediaSessionPlaybackControlsPurpose) = 0;
    virtual void forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& matchingCallback) = 0;

    virtual void setCurrentSession(PlatformMediaSessionInterface&) = 0;
    virtual RefPtr<PlatformMediaSessionInterface> currentSession() const = 0;

    virtual void addSession(PlatformMediaSessionInterface&);
    virtual void removeSession(PlatformMediaSessionInterface&);
    virtual bool hasNoSession() const;

    virtual bool activeAudioSessionRequired() const;
    virtual bool hasActiveAudioSession() const;
    virtual bool canProduceAudio() const;

    virtual void setShouldDeactivateAudioSession(bool should) { m_shouldDeactivateAudioSession = should; };
    virtual bool shouldDeactivateAudioSession() { return m_shouldDeactivateAudioSession; };

    virtual void updateNowPlayingInfoIfNecessary();
    virtual void updateNowPlayingInfo();
    virtual void setNowPlayingUpdateInterval(double);
    virtual double nowPlayingUpdateInterval();
    virtual void updateAudioSessionCategoryIfNecessary();

    virtual std::optional<NowPlayingInfo> nowPlayingInfo() const { return { }; }
    virtual bool hasActiveNowPlayingSession() const { return false; }
    virtual String lastUpdatedNowPlayingTitle() const { return emptyString(); }
    virtual double lastUpdatedNowPlayingDuration() const { return NAN; }
    virtual double lastUpdatedNowPlayingElapsedTime() const { return NAN; }
    virtual std::optional<MediaUniqueIdentifier> lastUpdatedNowPlayingInfoUniqueIdentifier() const { return std::nullopt; }
    virtual void addNowPlayingMetadataObserver(const NowPlayingMetadataObserver&);
    virtual void removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver&);
    virtual bool hasActiveNowPlayingSessionInGroup(std::optional<MediaSessionGroupIdentifier>);
    virtual bool registeredAsNowPlayingApplication() const { return false; }
    virtual bool haveEverRegisteredAsNowPlayingApplication() const { return false; }
    virtual void resetHaveEverRegisteredAsNowPlayingApplicationForTesting() { };

    virtual bool willIgnoreSystemInterruptions() const { return m_willIgnoreSystemInterruptions; }
    virtual void setWillIgnoreSystemInterruptions(bool ignore) { m_willIgnoreSystemInterruptions = ignore; }

    virtual void beginInterruption(PlatformMediaSessionInterruptionType);
    virtual void endInterruption(PlatformMediaSessionEndInterruptionFlags);

    virtual void applicationWillEnterForeground(bool);
    virtual void applicationDidEnterBackground(bool);
    virtual void applicationWillBecomeInactive();
    virtual void applicationDidBecomeActive();
    virtual void processWillSuspend();
    virtual void processDidResume();

    virtual void stopAllMediaPlaybackForProcess();
    virtual bool mediaPlaybackIsPaused(std::optional<MediaSessionGroupIdentifier>);
    virtual void pauseAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>);
    virtual void suspendAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>);
    virtual void resumeAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier>);
    virtual void suspendAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier>);
    virtual void resumeAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier>);

    virtual void addRestriction(PlatformMediaSessionMediaType, MediaSessionRestrictions);
    virtual void removeRestriction(PlatformMediaSessionMediaType, MediaSessionRestrictions);
    virtual MediaSessionRestrictions restrictions(PlatformMediaSessionMediaType);
    virtual void resetRestrictions();

    virtual void sessionWillBeginPlayback(PlatformMediaSessionInterface&, CompletionHandler<void(bool)>&&);
    virtual void sessionWillEndPlayback(PlatformMediaSessionInterface&, DelayCallingUpdateNowPlaying);
    virtual void sessionStateChanged(PlatformMediaSessionInterface&);
    virtual void sessionDidEndRemoteScrubbing(PlatformMediaSessionInterface&) { }
    virtual void sessionCanProduceAudioChanged();
    virtual void clientCharacteristicsChanged(PlatformMediaSessionInterface&, bool) { }

    virtual void configureWirelessTargetMonitoring() { }
    virtual bool hasWirelessTargetsAvailable() { return false; }
    virtual bool isMonitoringWirelessTargets() const { return false; }
    virtual void sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSessionInterface&);

    virtual void setIsPlayingToAutomotiveHeadUnit(bool);
    virtual bool isPlayingToAutomotiveHeadUnit() const { return m_isPlayingToAutomotiveHeadUnit; };

    virtual void setSupportsSpatialAudioPlayback(bool);
    virtual std::optional<bool> supportsSpatialAudioPlaybackForConfiguration(const PlatformMediaConfiguration&) { return m_supportsSpatialAudioPlayback; }

    virtual void addAudioCaptureSource(AudioCaptureSource&);
    virtual void removeAudioCaptureSource(AudioCaptureSource&);
    virtual void audioCaptureSourceStateChanged() { updateSessionState(); }
    virtual size_t audioCaptureSourceCount() const { return m_audioCaptureSources.computeSize(); }

    virtual void processDidReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&);
    virtual bool processIsSuspended() const { return m_processIsSuspended; };
    virtual void processSystemWillSleep();
    virtual void processSystemDidWake();

    virtual bool isApplicationInBackground() const { return m_isApplicationInBackground; }
    virtual bool isInterrupted() const { return !!m_currentInterruption; }

    virtual void addSupportedCommand(PlatformMediaSessionRemoteControlCommandType) { };
    virtual void removeSupportedCommand(PlatformMediaSessionRemoteControlCommandType) { };
    virtual PlatformMediaSessionRemoteCommandsSet supportedCommands() const { return { }; };

    virtual void scheduleSessionStatusUpdate() { }
    virtual void resetSessionState() { };

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
#endif

protected:
    explicit MediaSessionManagerInterface(PageIdentifier);

    virtual WeakListHashSet<PlatformMediaSessionInterface>& sessions() const = 0;
    virtual Vector<WeakPtr<PlatformMediaSessionInterface>> copySessionsToVector() const = 0;

    void forEachSession(NOESCAPE const Function<void(PlatformMediaSessionInterface&)>&);
    void forEachSessionInGroup(std::optional<MediaSessionGroupIdentifier>, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>&);
    bool anyOfSessions(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;
    Vector<WeakPtr<PlatformMediaSessionInterface>> sessionsMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;
    WeakPtr<PlatformMediaSessionInterface> firstSessionMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>&) const;

    void maybeDeactivateAudioSession();
    bool maybeActivateAudioSession();

    void nowPlayingMetadataChanged(const NowPlayingMetadata&);
    void enqueueTaskOnMainThread(Function<void()>&&);

    int countActiveAudioCaptureSources();

    std::optional<bool> supportsSpatialAudioPlayback() { return m_supportsSpatialAudioPlayback; }

    bool computeSupportsSeeking() const;

    void scheduleUpdateSessionState();
    virtual void updateSessionState() { }

    PageIdentifier pageIdentifier() const { return m_pageIdentifier; }

#if !RELEASE_LOG_DISABLED
    void scheduleStateLog();
    void dumpSessionStates();

    uint64_t logIdentifier() const final { return 0; }
    ASCIILiteral logClassName() const override { return "MediaSessionManagerInterface"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    bool willLog(WTFLogLevel) const;

private:
    bool has(PlatformMediaSessionMediaType) const;

    std::array<MediaSessionRestrictions, static_cast<unsigned>(PlatformMediaSessionMediaType::DOMMediaSession) + 1> m_restrictions;

    std::optional<bool> m_supportsSpatialAudioPlayback;
    std::optional<PlatformMediaSessionInterruptionType> m_currentInterruption;

    WeakHashSet<AudioCaptureSource> m_audioCaptureSources;

    WeakHashSet<NowPlayingMetadataObserver> m_nowPlayingMetadataObservers;
    TaskCancellationGroup m_taskGroup;

    PageIdentifier m_pageIdentifier;
#if !RELEASE_LOG_DISABLED
    UniqueRef<Timer> m_stateLogTimer;
    const Ref<AggregateLogger> m_logger;
#endif

    bool m_shouldDeactivateAudioSession { false };
    bool m_willIgnoreSystemInterruptions { false };
    bool m_isPlayingToAutomotiveHeadUnit { false };
    bool m_processIsSuspended { false };
    bool m_alreadyScheduledSessionStatedUpdate { false };
    bool m_hasScheduledSessionStateUpdate { false };
    mutable bool m_isApplicationInBackground { false };
#if USE(AUDIO_SESSION)
    bool m_becameActive { false };
#endif
};

#if !RELEASE_LOG_DISABLED
inline const Logger& MediaSessionManagerInterface::logger() const { return m_logger; }
#endif

} // namespace WebCore
