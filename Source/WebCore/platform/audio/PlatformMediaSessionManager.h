/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "DocumentIdentifier.h"
#include "MediaSessionIdentifier.h"
#include "PlatformMediaSession.h"
#include "Timer.h"
#include <wtf/AggregateLogger.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class PlatformMediaSession;
class RemoteCommandListener;

class PlatformMediaSessionManager
    : public CanMakeWeakPtr<PlatformMediaSessionManager>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static PlatformMediaSessionManager* sharedManagerIfExists();
    WEBCORE_EXPORT static PlatformMediaSessionManager& sharedManager();
    WEBCORE_EXPORT static std::unique_ptr<PlatformMediaSessionManager> create();

    static void updateNowPlayingInfoIfNecessary();

    WEBCORE_EXPORT static void setShouldDeactivateAudioSession(bool);
    WEBCORE_EXPORT static bool shouldDeactivateAudioSession();

    virtual ~PlatformMediaSessionManager() = default;

    virtual void scheduleSessionStatusUpdate() { }

    bool has(PlatformMediaSession::MediaType) const;
    int count(PlatformMediaSession::MediaType) const;
    bool activeAudioSessionRequired() const;
    bool canProduceAudio() const;

    virtual bool hasActiveNowPlayingSession() const { return false; }
    virtual String lastUpdatedNowPlayingTitle() const { return emptyString(); }
    virtual double lastUpdatedNowPlayingDuration() const { return NAN; }
    virtual double lastUpdatedNowPlayingElapsedTime() const { return NAN; }
    virtual MediaSessionIdentifier lastUpdatedNowPlayingInfoUniqueIdentifier() const { return { }; }
    virtual bool registeredAsNowPlayingApplication() const { return false; }
    virtual bool haveEverRegisteredAsNowPlayingApplication() const { return false; }
    virtual void prepareToSendUserMediaPermissionRequest() { }

    bool willIgnoreSystemInterruptions() const { return m_willIgnoreSystemInterruptions; }
    void setWillIgnoreSystemInterruptions(bool ignore) { m_willIgnoreSystemInterruptions = ignore; }

    WEBCORE_EXPORT virtual void beginInterruption(PlatformMediaSession::InterruptionType);
    WEBCORE_EXPORT void endInterruption(PlatformMediaSession::EndInterruptionFlags);

    WEBCORE_EXPORT void applicationWillBecomeInactive();
    WEBCORE_EXPORT void applicationDidBecomeActive();
    WEBCORE_EXPORT void applicationWillEnterForeground(bool suspendedUnderLock);
    WEBCORE_EXPORT void applicationDidEnterBackground(bool suspendedUnderLock);
    WEBCORE_EXPORT void processWillSuspend();
    WEBCORE_EXPORT void processDidResume();

    void stopAllMediaPlaybackForDocument(DocumentIdentifier);
    WEBCORE_EXPORT void stopAllMediaPlaybackForProcess();

    void suspendAllMediaPlaybackForDocument(DocumentIdentifier);
    void resumeAllMediaPlaybackForDocument(DocumentIdentifier);
    void suspendAllMediaBufferingForDocument(DocumentIdentifier);
    void resumeAllMediaBufferingForDocument(DocumentIdentifier);

    enum SessionRestrictionFlags {
        NoRestrictions = 0,
        ConcurrentPlaybackNotPermitted = 1 << 0,
        BackgroundProcessPlaybackRestricted = 1 << 1,
        BackgroundTabPlaybackRestricted = 1 << 2,
        InterruptedPlaybackNotPermitted = 1 << 3,
        InactiveProcessPlaybackRestricted = 1 << 4,
        SuspendedUnderLockPlaybackRestricted = 1 << 5,
    };
    typedef unsigned SessionRestrictions;

    WEBCORE_EXPORT void addRestriction(PlatformMediaSession::MediaType, SessionRestrictions);
    WEBCORE_EXPORT void removeRestriction(PlatformMediaSession::MediaType, SessionRestrictions);
    WEBCORE_EXPORT SessionRestrictions restrictions(PlatformMediaSession::MediaType);
    virtual void resetRestrictions();

    virtual bool sessionWillBeginPlayback(PlatformMediaSession&);

    virtual void sessionWillEndPlayback(PlatformMediaSession&, DelayCallingUpdateNowPlaying);
    virtual void sessionStateChanged(PlatformMediaSession&);
    virtual void sessionDidEndRemoteScrubbing(PlatformMediaSession&) { };
    virtual void clientCharacteristicsChanged(PlatformMediaSession&) { }
    virtual void sessionCanProduceAudioChanged();

#if PLATFORM(IOS_FAMILY)
    virtual void configureWireLessTargetMonitoring() { }
#endif
    virtual bool hasWirelessTargetsAvailable() { return false; }

    virtual void setCurrentSession(PlatformMediaSession&);
    PlatformMediaSession* currentSession() const;

    void sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSession&);

    WEBCORE_EXPORT void setIsPlayingToAutomotiveHeadUnit(bool);
    bool isPlayingToAutomotiveHeadUnit() const { return m_isPlayingToAutomotiveHeadUnit; }

    void forEachMatchingSession(const Function<bool(const PlatformMediaSession&)>& predicate, const Function<void(PlatformMediaSession&)>& matchingCallback);

    bool processIsSuspended() const { return m_processIsSuspended; }

    WEBCORE_EXPORT void addAudioCaptureSource(PlatformMediaSession::AudioCaptureSource&);
    WEBCORE_EXPORT void removeAudioCaptureSource(PlatformMediaSession::AudioCaptureSource&);

    WEBCORE_EXPORT void processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*);

    bool isInterrupted() const { return m_interrupted; }

protected:
    friend class PlatformMediaSession;
    PlatformMediaSessionManager();

    virtual void addSession(PlatformMediaSession&);
    virtual void removeSession(PlatformMediaSession&);

    void forEachSession(const Function<void(PlatformMediaSession&)>&);
    void forEachDocumentSession(DocumentIdentifier, const Function<void(PlatformMediaSession&)>&);
    bool anyOfSessions(const Function<bool(const PlatformMediaSession&)>&) const;

    bool isApplicationInBackground() const { return m_isApplicationInBackground; }
#if USE(AUDIO_SESSION)
    void maybeDeactivateAudioSession();
#endif

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const void* logIdentifier() const final { return nullptr; }
    const char* logClassName() const override { return "PlatformMediaSessionManager"; }
    WTFLogChannel& logChannel() const final;
#endif

    int countActiveAudioCaptureSources();
    bool hasNoSession() const;

    bool computeSupportsSeeking() const;

    WEBCORE_EXPORT void processSystemWillSleep();
    WEBCORE_EXPORT void processSystemDidWake();

private:
    friend class Internals;

    virtual void updateSessionState() { }

    Vector<WeakPtr<PlatformMediaSession>> sessionsMatching(const Function<bool(const PlatformMediaSession&)>&) const;

    SessionRestrictions m_restrictions[static_cast<unsigned>(PlatformMediaSession::MediaType::WebAudio) + 1];
    mutable Vector<WeakPtr<PlatformMediaSession>> m_sessions;

    bool m_interrupted { false };
    mutable bool m_isApplicationInBackground { false };
    bool m_willIgnoreSystemInterruptions { false };
    bool m_processIsSuspended { false };
    bool m_isPlayingToAutomotiveHeadUnit { false };

#if USE(AUDIO_SESSION)
    bool m_becameActive { false };
#endif

    WeakHashSet<PlatformMediaSession::AudioCaptureSource> m_audioCaptureSources;

#if !RELEASE_LOG_DISABLED
    Ref<AggregateLogger> m_logger;
#endif
};

}

#endif // PlatformMediaSessionManager_h
