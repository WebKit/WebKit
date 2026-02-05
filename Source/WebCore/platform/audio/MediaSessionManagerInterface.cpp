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

#include "config.h"
#include "MediaSessionManagerInterface.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "NowPlayingInfo.h"
#include "PlatformMediaSession.h"
#include <algorithm>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

#define MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(formatString, ...) \
if (willLog(WTFLogLevel::Always)) { \
    RELEASE_LOG_FORWARDABLE(Media, MEDIASESSIONMANAGERINTERFACE_##formatString, ##__VA_ARGS__); \
} \

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSessionManagerInterface);

MediaSessionManagerInterface::MediaSessionManagerInterface(PageIdentifier pageIdentifier)
    : m_pageIdentifier(pageIdentifier)
#if !RELEASE_LOG_DISABLED
    , m_stateLogTimer(makeUniqueRef<Timer>(*this, &MediaSessionManagerInterface::dumpSessionStates))
    , m_logger(AggregateLogger::create(this))
#endif
{
}

MediaSessionManagerInterface::~MediaSessionManagerInterface()
{
    m_taskGroup.cancel();
}

static inline unsigned indexFromMediaType(PlatformMediaSession::MediaType type)
{
    return static_cast<unsigned>(type);
}

Vector<WeakPtr<PlatformMediaSessionInterface>> MediaSessionManagerInterface::sessionsMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& filter) const
{
    Vector<WeakPtr<PlatformMediaSessionInterface>> matchingSessions;
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && filter(*session))
            matchingSessions.append(weakSession);
    }
    return matchingSessions;
}

WeakPtr<PlatformMediaSessionInterface> MediaSessionManagerInterface::firstSessionMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && predicate(*session))
            return weakSession;
    }
    return nullptr;
}

void MediaSessionManagerInterface::forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

void MediaSessionManagerInterface::forEachSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    if (!mediaSessionGroupIdentifier)
        return;

    forEachMatchingSession([mediaSessionGroupIdentifier = *mediaSessionGroupIdentifier](auto& session) {
        return session.client().mediaSessionGroupIdentifier() == mediaSessionGroupIdentifier;
    }, [&callback](auto& session) {
        callback(session);
    });
}

void MediaSessionManagerInterface::forEachSession(NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& weakSession : copySessionsToVector()) {
        if (RefPtr session = weakSession.get())
            callback(*session);
    }
}

bool MediaSessionManagerInterface::anyOfSessions(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && predicate(*session))
            return true;
    }

    return false;
}

void MediaSessionManagerInterface::resetRestrictions()
{
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Video)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Audio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::VideoAudio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::WebAudio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::DOMMediaSession)] = MediaSessionRestriction::NoRestrictions;
}

bool MediaSessionManagerInterface::has(PlatformMediaSession::MediaType type) const
{
    return anyOfSessions([type] (auto& session) {
        return session.mediaType() == type;
    });
}

bool MediaSessionManagerInterface::activeAudioSessionRequired() const
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (anyOfSessions([] (auto& session) { return session.activeAudioSessionRequired(); }))
        return true;

    return std::ranges::any_of(m_audioCaptureSources, [](auto& source) {
        return Ref { source }->isCapturingAudio();
    });
#else
    return false;
#endif
}

bool MediaSessionManagerInterface::hasActiveAudioSession() const
{
#if USE(AUDIO_SESSION)
    return m_becameActive;
#else
    return true;
#endif
}

bool MediaSessionManagerInterface::canProduceAudio() const
{
    return anyOfSessions([] (auto& session) {
        return session.canProduceAudio();
    });
}

void MediaSessionManagerInterface::updateNowPlayingInfoIfNecessary()
{
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerInterface::updateNowPlayingInfo()
{
    updateNowPlayingInfoIfNecessary();
}

void MediaSessionManagerInterface::setNowPlayingUpdateInterval(double)
{
}

double MediaSessionManagerInterface::nowPlayingUpdateInterval()
{
    return 0;
}

void MediaSessionManagerInterface::updateAudioSessionCategoryIfNecessary()
{
    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::addNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(!m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.add(observer);
    observer(nowPlayingInfo().value_or(NowPlayingInfo { }).metadata);
}

void MediaSessionManagerInterface::removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.remove(observer);
}

void MediaSessionManagerInterface::nowPlayingMetadataChanged(const NowPlayingMetadata& metadata)
{
    m_nowPlayingMetadataObservers.forEach([&] (auto& observer) {
        observer(metadata);
    });
}

bool MediaSessionManagerInterface::hasActiveNowPlayingSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    bool hasActiveNowPlayingSession = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&](auto& session) {
        hasActiveNowPlayingSession |= session.isActiveNowPlayingSession();
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif

    return hasActiveNowPlayingSession;
}

void MediaSessionManagerInterface::enqueueTaskOnMainThread(Function<void()>&& task)
{
    callOnMainThread(CancellableTask(m_taskGroup, [task = WTF::move(task)] () mutable {
        task();
    }));
}

void MediaSessionManagerInterface::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = type;
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([type] (auto& session) {
        session.beginInterruption(type);
    });
#endif
    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = { };
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([flags] (auto& session) {
        session.endInterruption(flags);
    });
#else
    UNUSED_PARAM(flags);
#endif
}

void MediaSessionManagerInterface::applicationWillEnterForeground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, suspendedUnderLock);

    if (!m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return (suspendedUnderLock && restrictions(session.mediaType()).contains( MediaSessionRestriction::SuspendedUnderLockPlaybackRestricted)) || restrictions(session.mediaType()).contains( MediaSessionRestriction::BackgroundProcessPlaybackRestricted);
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::applicationDidEnterBackground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, suspendedUnderLock);

    if (m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = true;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        if (suspendedUnderLock && restrictions(session.mediaType()).contains(MediaSessionRestriction::SuspendedUnderLockPlaybackRestricted))
            session.beginInterruption(PlatformMediaSession::InterruptionType::SuspendedUnderLock);
        else if (restrictions(session.mediaType()).contains(MediaSessionRestriction::BackgroundProcessPlaybackRestricted))
            session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
    });
#endif
}

void MediaSessionManagerInterface::applicationWillBecomeInactive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()).contains(MediaSessionRestriction::InactiveProcessPlaybackRestricted);
    }, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::ProcessInactive);
    });
#endif
}

void MediaSessionManagerInterface::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()).contains(MediaSessionRestriction::InactiveProcessPlaybackRestricted);
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::processWillSuspend()
{
    if (m_processIsSuspended)
        return;
    m_processIsSuspended = true;

    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });
#endif

#if USE(AUDIO_SESSION)
    maybeDeactivateAudioSession();
#endif
}

void MediaSessionManagerInterface::processDidResume()
{
    if (!m_processIsSuspended)
        return;
    m_processIsSuspended = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });
#endif

#if USE(AUDIO_SESSION)
    if (!m_becameActive)
        maybeActivateAudioSession();
#endif
}

void MediaSessionManagerInterface::stopAllMediaPlaybackForProcess()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.stopSession();
    });
#endif
}

bool MediaSessionManagerInterface::mediaPlaybackIsPaused(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    bool mediaPlaybackIsPaused = false;
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&mediaPlaybackIsPaused](auto& session) {
        if (session.state() == PlatformMediaSession::State::Paused)
            mediaPlaybackIsPaused = true;
    });
    return mediaPlaybackIsPaused;
}

void MediaSessionManagerInterface::pauseAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.pauseSession();
    });
}

void MediaSessionManagerInterface::suspendAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void MediaSessionManagerInterface::resumeAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void MediaSessionManagerInterface::suspendAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.suspendBuffering();
    });
}

void MediaSessionManagerInterface::resumeAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.resumeBuffering();
    });
}

void MediaSessionManagerInterface::addRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].add(restriction);
}

void MediaSessionManagerInterface::removeRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].remove(restriction);
}

MediaSessionRestrictions MediaSessionManagerInterface::restrictions(PlatformMediaSession::MediaType type)
{
    return m_restrictions[indexFromMediaType(type)];
}

void MediaSessionManagerInterface::sessionWillBeginPlayback(PlatformMediaSessionInterface& session, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    setCurrentSession(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto sessionType = session.mediaType();
    auto restrictions = this->restrictions(sessionType);
    if (session.state() == PlatformMediaSession::State::Interrupted && restrictions & MediaSessionRestriction::InterruptedPlaybackNotPermitted) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false because session.state() is Interrupted, and InterruptedPlaybackNotPermitted");
        completionHandler(false);
        return;
    }

    if (!maybeActivateAudioSession()) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false, failed to activate AudioSession");
        completionHandler(false);
        return;
    }

    if (m_currentInterruption)
        endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

    if (restrictions.contains(MediaSessionRestriction::ConcurrentPlaybackNotPermitted)) {
        forEachMatchingSession([&session](auto& otherSession) {
            if (&otherSession == &session)
                return false;

            if (otherSession.state() != PlatformMediaSession::State::Playing)
                return false;

            return !otherSession.canPlayConcurrently(session);
        }, [](auto& oneSession) {
            oneSession.pauseSession();
        });
    }
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning true");
    completionHandler(true);
#else
    completionHandler(false);
#endif
}

void MediaSessionManagerInterface::sessionWillEndPlayback(PlatformMediaSessionInterface& pausingSession, DelayCallingUpdateNowPlaying)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(SESSIONWILLENDPLAYBACK, pausingSession.logIdentifier());
#endif

    auto sessions = this->sessions();
    auto sessionCount = sessions.computeSize();
    if (sessionCount < 2)
        return;

    RefPtr<PlatformMediaSessionInterface> firstPausedSession;
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        RefPtr session = *it.get();
        if (&pausingSession == session.get() || session->state() == PlatformMediaSession::State::Playing)
            continue;

        firstPausedSession = session.get();
        break;
    }

    if (firstPausedSession) {
        sessions.remove(pausingSession);
        sessions.insertBefore(*firstPausedSession, pausingSession);
    } else
        sessions.appendOrMoveToLast(pausingSession);
}

void MediaSessionManagerInterface::sessionStateChanged(PlatformMediaSessionInterface& session)
{
    // Call updateSessionState() synchronously if the new state is Playing to ensure
    // the audio session is active and has the correct category before playback starts.
    if (session.state() == PlatformMediaSession::State::Playing)
        updateSessionState();
    else
        scheduleUpdateSessionState();

#if !RELEASE_LOG_DISABLED
    scheduleStateLog();
#endif
}

void MediaSessionManagerInterface::sessionCanProduceAudioChanged()
{
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(SESSIONCANPRODUCEAUDIOCHANGED);

    if (m_alreadyScheduledSessionStatedUpdate)
        return;

    m_alreadyScheduledSessionStatedUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { *this }] {
        m_alreadyScheduledSessionStatedUpdate = false;
        maybeActivateAudioSession();
        updateSessionState();
    });
}

void MediaSessionManagerInterface::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSessionInterface& session)
{
    if (!m_isApplicationInBackground || !(restrictions(session.mediaType()).contains(MediaSessionRestriction::BackgroundProcessPlaybackRestricted)))
        return;

    if (session.state() != PlatformMediaSession::State::Interrupted)
        session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
}

void MediaSessionManagerInterface::setIsPlayingToAutomotiveHeadUnit(bool isPlayingToAutomotiveHeadUnit)
{
    if (isPlayingToAutomotiveHeadUnit == m_isPlayingToAutomotiveHeadUnit)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isPlayingToAutomotiveHeadUnit);
    m_isPlayingToAutomotiveHeadUnit = isPlayingToAutomotiveHeadUnit;
}

void MediaSessionManagerInterface::setSupportsSpatialAudioPlayback(bool supportsSpatialAudioPlayback)
{
    if (supportsSpatialAudioPlayback == m_supportsSpatialAudioPlayback)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, supportsSpatialAudioPlayback);
    m_supportsSpatialAudioPlayback = supportsSpatialAudioPlayback;
}

void MediaSessionManagerInterface::addAudioCaptureSource(AudioCaptureSource& source)
{
    ASSERT(!m_audioCaptureSources.contains(source));
    m_audioCaptureSources.add(source);
    updateSessionState();
}


void MediaSessionManagerInterface::removeAudioCaptureSource(AudioCaptureSource& source)
{
    m_audioCaptureSources.remove(source);
    scheduleUpdateSessionState();
}

int MediaSessionManagerInterface::countActiveAudioCaptureSources()
{
    int count = 0;
    for (Ref source : m_audioCaptureSources) {
        if (source->wantsToCaptureAudio())
            ++count;
    }
    return count;
}

void MediaSessionManagerInterface::processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
#if ENABLE(VIDEO) || ENABLE(audio)
    RefPtr<PlatformMediaSessionInterface> activeSession;
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (!session || !session->canReceiveRemoteControlCommands())
            continue;

        if (session->isNowPlayingEligible()) {
            activeSession = WTF::move(session);
            break;
        }
        if (!activeSession)
            activeSession = WTF::move(session);
    }

    if (activeSession)
        activeSession->didReceiveRemoteControlCommand(command, argument);
#else
    UNUSED_PARAM(command);
    UNUSED_PARAM(argument);
#endif
}

void MediaSessionManagerInterface::processSystemWillSleep()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::SystemSleep);
    });
#endif
}

void MediaSessionManagerInterface::processSystemDidWake()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::addSession(PlatformMediaSessionInterface& session)
{
#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->addLogger(session.protectedLogger());
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(ADDSESSION, session.logIdentifier());
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (m_currentInterruption)
        session.beginInterruption(*m_currentInterruption);
#endif

    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::removeSession(PlatformMediaSessionInterface& session)
{
    UNUSED_PARAM(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(REMOVESESSION, session.logIdentifier());
#endif

    if (hasNoSession() && !activeAudioSessionRequired())
        maybeDeactivateAudioSession();

#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->removeLogger(session.protectedLogger());
#endif

    scheduleUpdateSessionState();
}

bool MediaSessionManagerInterface::hasNoSession() const
{
    return sessions().isEmptyIgnoringNullReferences();
}

bool MediaSessionManagerInterface::computeSupportsSeeking() const
{
    if (RefPtr activeSession = currentSession())
        return activeSession->supportsSeeking();

    return false;
}

void MediaSessionManagerInterface::maybeDeactivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!m_becameActive || !shouldDeactivateAudioSession())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "tried to set inactive AudioSession");
    AudioSession::singleton().tryToSetActive(false);
    m_becameActive = false;
#endif
}

bool MediaSessionManagerInterface::maybeActivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!activeAudioSessionRequired()) {
        MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(MAYBEACTIVATEAUDIOSESSION_ACTIVE_SESSION_NOT_REQUIRED);
        return true;
    }

    m_becameActive = AudioSession::singleton().tryToSetActive(true);
    ALWAYS_LOG(LOGIDENTIFIER, m_becameActive ? "successfully activated" : "failed to activate", " AudioSession");
    return m_becameActive;
#else
    return true;
#endif
}

void MediaSessionManagerInterface::scheduleUpdateSessionState()
{
    if (m_hasScheduledSessionStateUpdate)
        return;

    m_hasScheduledSessionStateUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { * this }] {
        updateSessionState();
        m_hasScheduledSessionStateUpdate = false;
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSessionManagerInterface::logChannel() const
{
    return LogMedia;
}

void MediaSessionManagerInterface::scheduleStateLog()
{
    if (m_stateLogTimer->isActive())
        return;

    static constexpr Seconds StateLogDelay { 5_s };
    m_stateLogTimer->startOneShot(StateLogDelay);
}

void MediaSessionManagerInterface::dumpSessionStates()
{
    StringBuilder builder;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&](auto& session) {
        builder.append('(', hex(session.logIdentifier()), "): "_s, session.description(), "\n"_s);
    });
#endif

    ALWAYS_LOG(LOGIDENTIFIER, " Sessions:\n", builder.toString());
}
#endif

bool MediaSessionManagerInterface::willLog(WTFLogLevel level) const
{
#if !RELEASE_LOG_DISABLED
    return m_logger->willLog(logChannel(), level);
#else
    UNUSED_PARAM(level);
    return false;
#endif
}


} // namespace WebCore
