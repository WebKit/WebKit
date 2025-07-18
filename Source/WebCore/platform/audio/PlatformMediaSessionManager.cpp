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

#include "config.h"
#include "PlatformMediaSessionManager.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "NowPlayingInfo.h"
#include "PlatformMediaSession.h"
#include <algorithm>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "VP9UtilitiesCocoa.h"
#endif

#define PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(formatString, ...) \
if (willLog(WTFLogLevel::Always)) { \
    RELEASE_LOG_FORWARDABLE(Media, PLATFORMMEDIASESSIONMANAGER_##formatString, ##__VA_ARGS__); \
} \

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformMediaSessionManager);

#if ENABLE(VP9)
bool PlatformMediaSessionManager::m_vp9DecoderEnabled;
bool PlatformMediaSessionManager::m_swVPDecodersAlwaysEnabled;
#endif

static const std::unique_ptr<PlatformMediaSessionManager>& sharedPlatformMediaSessionManager()
{
    static NeverDestroyed<const std::unique_ptr<PlatformMediaSessionManager>> platformMediaSessionManager;
    return platformMediaSessionManager.get();
}

PlatformMediaSessionManager& PlatformMediaSessionManager::singleton()
{
    auto& manager = sharedPlatformMediaSessionManager();
    if (!manager) {
        lazyInitialize(manager, PlatformMediaSessionManager::create());
        manager->resetRestrictions();
    }
    return *manager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::singletonIfExists()
{
    return sharedPlatformMediaSessionManager().get();
}

#if !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))
const std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return std::unique_ptr<PlatformMediaSessionManager>(new PlatformMediaSessionManager);
}
#endif // !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{
    if (RefPtr existingManager = PlatformMediaSessionManager::singletonIfExists())
        existingManager->scheduleSessionStatusUpdate();
}

void PlatformMediaSessionManager::updateAudioSessionCategoryIfNecessary()
{
    if (RefPtr existingManager = PlatformMediaSessionManager::singletonIfExists())
        existingManager->scheduleUpdateSessionState();
}

PlatformMediaSessionManager::PlatformMediaSessionManager()
#if !RELEASE_LOG_DISABLED
    : m_stateLogTimer(makeUniqueRef<Timer>(*this, &PlatformMediaSessionManager::dumpSessionStates))
    , m_logger(AggregateLogger::create(this))
#endif
{
}

PlatformMediaSessionManager::~PlatformMediaSessionManager()
{
    m_taskGroup.cancel();
}

static inline unsigned indexFromMediaType(PlatformMediaSession::MediaType type)
{
    return static_cast<unsigned>(type);
}

void PlatformMediaSessionManager::resetRestrictions()
{
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Video)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Audio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::VideoAudio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::WebAudio)] = MediaSessionRestriction::NoRestrictions;
}

bool PlatformMediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    return anyOfSessions([type] (auto& session) {
        return session.mediaType() == type;
    });
}

bool PlatformMediaSessionManager::activeAudioSessionRequired() const
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (anyOfSessions([] (auto& session) { return session.activeAudioSessionRequired(); }))
        return true;

    return std::ranges::any_of(m_audioCaptureSources, [](auto& source) {
        return source.isCapturingAudio();
    });
#else
    return false;
#endif
}

bool PlatformMediaSessionManager::hasActiveAudioSession() const
{
#if USE(AUDIO_SESSION)
    return m_becameActive;
#else
    return true;
#endif
}

bool PlatformMediaSessionManager::canProduceAudio() const
{
    return anyOfSessions([] (auto& session) {
        return session.canProduceAudio();
    });
}

std::optional<NowPlayingInfo> PlatformMediaSessionManager::nowPlayingInfo() const
{
    return { };
}

int PlatformMediaSessionManager::count(PlatformMediaSession::MediaType type) const
{
    int count = 0;
    for (const auto& session : m_sessions) {
        if (session->mediaType() == type)
            ++count;
    }

    return count;
}

int PlatformMediaSessionManager::countActiveAudioCaptureSources()
{
    int count = 0;
    for (const auto& source : m_audioCaptureSources) {
        if (source.wantsToCaptureAudio())
            ++count;
    }
    return count;
}

void PlatformMediaSessionManager::beginInterruption(PlatformMediaSession::InterruptionType type)
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

void PlatformMediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
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

void PlatformMediaSessionManager::addSession(PlatformMediaSessionInterface& session)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(ADDSESSION, session.logIdentifier());
#endif

    m_sessions.append(session);
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (m_currentInterruption)
        session.beginInterruption(*m_currentInterruption);
#endif

#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->addLogger(session.protectedLogger());
#endif

    scheduleUpdateSessionState();
}

bool PlatformMediaSessionManager::hasNoSession() const
{
    return m_sessions.isEmpty() || std::ranges::all_of(m_sessions, std::logical_not<void>());
}

void PlatformMediaSessionManager::removeSession(PlatformMediaSessionInterface& session)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(REMOVESESSION, session.logIdentifier());
#endif

    size_t index = m_sessions.find(&session);
    if (index == notFound)
        return;

    m_sessions.removeAt(index);

    if (hasNoSession() && !activeAudioSessionRequired())
        maybeDeactivateAudioSession();

#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->removeLogger(session.protectedLogger());
#endif

    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].add(restriction);
}

void PlatformMediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].remove(restriction);
}

MediaSessionRestrictions PlatformMediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    return m_restrictions[indexFromMediaType(type)];
}

bool PlatformMediaSessionManager::sessionWillBeginPlayback(PlatformMediaSessionInterface& session)
{
    setCurrentSession(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto sessionType = session.mediaType();
    auto restrictions = this->restrictions(sessionType);
    if (session.state() == PlatformMediaSession::State::Interrupted && restrictions & MediaSessionRestriction::InterruptedPlaybackNotPermitted) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false because session.state() is Interrupted, and InterruptedPlaybackNotPermitted");
        return false;
    }

    if (!maybeActivateAudioSession()) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false, failed to activate AudioSession");
        return false;
    }

    if (m_currentInterruption)
        endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

    if (restrictions.contains(MediaSessionRestriction::ConcurrentPlaybackNotPermitted)) {
        forEachMatchingSession([&session](auto& oneSession) {
            return &oneSession != &session
                && oneSession.state() == PlatformMediaSession::State::Playing
                && !oneSession.canPlayConcurrently(session);
        }, [](auto& oneSession) {
            oneSession.pauseSession();
        });
    }
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning true");
    return true;
#else
    return false;
#endif
}
    
void PlatformMediaSessionManager::sessionWillEndPlayback(PlatformMediaSessionInterface& session, DelayCallingUpdateNowPlaying)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(SESSIONWILLENDPLAYBACK, session.logIdentifier());
#endif

    if (m_sessions.size() < 2)
        return;
    
    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    for (size_t i = 0, size = m_sessions.size(); i < size; ++i) {
        const auto& oneSession = *m_sessions[i];
        if (&oneSession == &session)
            pausingSessionIndex = i;
        else if (oneSession.state() == PlatformMediaSession::State::Playing)
            lastPlayingSessionIndex = i;
        else
            break;
    }

    if (lastPlayingSessionIndex == notFound || pausingSessionIndex == notFound)
        return;

    if (pausingSessionIndex > lastPlayingSessionIndex)
        return;

    // Removing the pausing session from m_sessions will decrement the index of the
    // last playing session, and re-inserting that pausing session at the
    // lastPlayingSessionIndex effectively places the pausing session immediately
    // after the last playing session.
    m_sessions.removeAt(pausingSessionIndex);
    m_sessions.insert(lastPlayingSessionIndex, session);

    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", pausingSessionIndex, " to ", lastPlayingSessionIndex);
}

void PlatformMediaSessionManager::sessionStateChanged(PlatformMediaSessionInterface& session)
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

void PlatformMediaSessionManager::setCurrentSession(PlatformMediaSessionInterface& session)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
#endif
    
    if (m_sessions.size() < 2)
        return;
    
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (!index || index == notFound)
        return;

    m_sessions.removeAt(index);
    m_sessions.insert(0, session);
    
    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", index, " to 0");
}
    
RefPtr<PlatformMediaSessionInterface> PlatformMediaSessionManager::currentSession() const
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0].get();
}

void PlatformMediaSessionManager::applicationWillBecomeInactive()
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

void PlatformMediaSessionManager::applicationDidBecomeActive()
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

void PlatformMediaSessionManager::applicationDidEnterBackground(bool suspendedUnderLock)
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

void PlatformMediaSessionManager::applicationWillEnterForeground(bool suspendedUnderLock)
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

void PlatformMediaSessionManager::processWillSuspend()
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

void PlatformMediaSessionManager::processDidResume()
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

void PlatformMediaSessionManager::setIsPlayingToAutomotiveHeadUnit(bool isPlayingToAutomotiveHeadUnit)
{
    if (isPlayingToAutomotiveHeadUnit == m_isPlayingToAutomotiveHeadUnit)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isPlayingToAutomotiveHeadUnit);
    m_isPlayingToAutomotiveHeadUnit = isPlayingToAutomotiveHeadUnit;
}

void PlatformMediaSessionManager::setSupportsSpatialAudioPlayback(bool supportsSpatialAudioPlayback)
{
    if (supportsSpatialAudioPlayback == m_supportsSpatialAudioPlayback)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, supportsSpatialAudioPlayback);
    m_supportsSpatialAudioPlayback = supportsSpatialAudioPlayback;
}

std::optional<bool> PlatformMediaSessionManager::supportsSpatialAudioPlaybackForConfiguration(const MediaConfiguration&)
{
    return m_supportsSpatialAudioPlayback;
}

void PlatformMediaSessionManager::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSessionInterface& session)
{
    if (!m_isApplicationInBackground || !(restrictions(session.mediaType()).contains(MediaSessionRestriction::BackgroundProcessPlaybackRestricted)))
        return;

    if (session.state() != PlatformMediaSession::State::Interrupted)
        session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
}

void PlatformMediaSessionManager::sessionCanProduceAudioChanged()
{
    PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(SESSIONCANPRODUCEAUDIOCHANGED);

    if (m_alreadyScheduledSessionStatedUpdate)
        return;

    m_alreadyScheduledSessionStatedUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { *this }] {
        m_alreadyScheduledSessionStatedUpdate = false;
        maybeActivateAudioSession();
        updateSessionState();
    });
}

void PlatformMediaSessionManager::processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
#if ENABLE(VIDEO) || ENABLE(audio)
    auto activeSession = firstSessionMatching([](auto& session) {
        return session.canReceiveRemoteControlCommands();
    });

    if (activeSession)
        activeSession->didReceiveRemoteControlCommand(command, argument);
#else
    UNUSED_PARAM(command);
    UNUSED_PARAM(argument);
#endif
}

bool PlatformMediaSessionManager::computeSupportsSeeking() const
{
    if (RefPtr activeSession = currentSession())
        return activeSession->supportsSeeking();

    return false;
}

void PlatformMediaSessionManager::processSystemWillSleep()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::SystemSleep);
    });
#endif
}

void PlatformMediaSessionManager::processSystemDidWake()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void PlatformMediaSessionManager::pauseAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.pauseSession();
    });
}


bool PlatformMediaSessionManager::mediaPlaybackIsPaused(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    bool mediaPlaybackIsPaused = false;
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&mediaPlaybackIsPaused](auto& session) {
        if (session.state() == PlatformMediaSession::State::Paused)
            mediaPlaybackIsPaused = true;
    });
    return mediaPlaybackIsPaused;
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForProcess()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.stopSession();
    });
#endif
}

void PlatformMediaSessionManager::suspendAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void PlatformMediaSessionManager::resumeAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void PlatformMediaSessionManager::suspendAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.suspendBuffering();
    });
}

void PlatformMediaSessionManager::resumeAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.resumeBuffering();
    });
}

Vector<WeakPtr<PlatformMediaSessionInterface>> PlatformMediaSessionManager::sessionsMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& filter) const
{
    Vector<WeakPtr<PlatformMediaSessionInterface>> matchingSessions;
    for (auto& session : m_sessions) {
        if (filter(*session))
            matchingSessions.append(session);
    }
    return matchingSessions;
}

WeakPtr<PlatformMediaSessionInterface> PlatformMediaSessionManager::firstSessionMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    for (auto& session : m_sessions) {
        if (session && predicate(*session))
            return session;
    }
    return nullptr;
}

void PlatformMediaSessionManager::forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

void PlatformMediaSessionManager::forEachSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    if (!mediaSessionGroupIdentifier)
        return;

    forEachMatchingSession([mediaSessionGroupIdentifier = *mediaSessionGroupIdentifier](auto& session) {
        return session.client().mediaSessionGroupIdentifier() == mediaSessionGroupIdentifier;
    }, [&callback](auto& session) {
        callback(session);
    });
}

void PlatformMediaSessionManager::forEachSession(NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    auto sessions = m_sessions;
    for (auto& session : sessions) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

bool PlatformMediaSessionManager::anyOfSessions(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    return std::ranges::any_of(m_sessions, [&predicate](auto& session) {
        return predicate(*session);
    });
}

void PlatformMediaSessionManager::addAudioCaptureSource(AudioCaptureSource& source)
{
    ASSERT(!m_audioCaptureSources.contains(source));
    m_audioCaptureSources.add(source);
    updateSessionState();
}


void PlatformMediaSessionManager::removeAudioCaptureSource(AudioCaptureSource& source)
{
    m_audioCaptureSources.remove(source);
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::scheduleUpdateSessionState()
{
    if (m_hasScheduledSessionStateUpdate)
        return;

    m_hasScheduledSessionStateUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { * this }] {
        updateSessionState();
        m_hasScheduledSessionStateUpdate = false;
    });
}

void PlatformMediaSessionManager::maybeDeactivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!m_becameActive || !shouldDeactivateAudioSession())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "tried to set inactive AudioSession");
    AudioSession::singleton().tryToSetActive(false);
    m_becameActive = false;
#endif
}

bool PlatformMediaSessionManager::maybeActivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!activeAudioSessionRequired()) {
        PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(MAYBEACTIVATEAUDIOSESSION_ACTIVE_SESSION_NOT_REQUIRED);
        return true;
    }

    m_becameActive = AudioSession::singleton().tryToSetActive(true);
    ALWAYS_LOG(LOGIDENTIFIER, m_becameActive ? "successfully activated" : "failed to activate", " AudioSession");
    return m_becameActive;
#else
    return true;
#endif
}
static bool& deactivateAudioSession()
{
    static bool deactivate;
    return deactivate;
}

bool PlatformMediaSessionManager::shouldDeactivateAudioSession()
{
    return deactivateAudioSession();
}

void PlatformMediaSessionManager::setShouldDeactivateAudioSession(bool deactivate)
{
    deactivateAudioSession() = deactivate;
}

WeakPtr<PlatformMediaSessionInterface> PlatformMediaSessionManager::bestEligibleSessionForRemoteControls(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& filterFunction, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    Vector<WeakPtr<PlatformMediaSessionInterface>> eligibleAudioVideoSessions;
    Vector<WeakPtr<PlatformMediaSessionInterface>> eligibleWebAudioSessions;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession(filterFunction, [&](auto& session) {
        if (session.presentationType() == PlatformMediaSession::MediaType::WebAudio) {
            if (eligibleAudioVideoSessions.isEmpty())
                eligibleWebAudioSessions.append(session);
        } else
            eligibleAudioVideoSessions.append(session);
    });
#else
    UNUSED_PARAM(filterFunction);
#endif

    if (eligibleAudioVideoSessions.isEmpty()) {
        if (eligibleWebAudioSessions.isEmpty())
            return nullptr;
        return eligibleWebAudioSessions[0]->selectBestMediaSession(eligibleWebAudioSessions, purpose);
    }

    return eligibleAudioVideoSessions[0]->selectBestMediaSession(eligibleAudioVideoSessions, purpose);
}

void PlatformMediaSessionManager::addNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(!m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.add(observer);
    observer(nowPlayingInfo().value_or(NowPlayingInfo { }).metadata);
}

void PlatformMediaSessionManager::removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.remove(observer);
}

void PlatformMediaSessionManager::nowPlayingMetadataChanged(const NowPlayingMetadata& metadata)
{
    m_nowPlayingMetadataObservers.forEach([&] (auto& observer) {
        observer(metadata);
    });
}

bool PlatformMediaSessionManager::hasActiveNowPlayingSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
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

void PlatformMediaSessionManager::enqueueTaskOnMainThread(Function<void()>&& task)
{
    callOnMainThread(CancellableTask(m_taskGroup, [task = WTFMove(task)] () mutable {
        task();
    }));
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PlatformMediaSessionManager::logChannel() const
{
    return LogMedia;
}

void PlatformMediaSessionManager::scheduleStateLog()
{
    if (m_stateLogTimer->isActive())
        return;

    static constexpr Seconds StateLogDelay { 5_s };
    m_stateLogTimer->startOneShot(StateLogDelay);
}

void PlatformMediaSessionManager::dumpSessionStates()
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

bool PlatformMediaSessionManager::willLog(WTFLogLevel level) const
{
#if !RELEASE_LOG_DISABLED
    return m_logger->willLog(logChannel(), level);
#else
    UNUSED_PARAM(level);
    return false;
#endif
}

} // namespace WebCore
