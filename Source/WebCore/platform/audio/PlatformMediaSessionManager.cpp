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

#include "config.h"
#include "PlatformMediaSessionManager.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "PlatformMediaSession.h"

namespace WebCore {

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#if ENABLE(WEBM_FORMAT_READER)
bool PlatformMediaSessionManager::m_webMFormatReaderEnabled;
#endif

#if ENABLE(VORBIS) && PLATFORM(MAC)
bool PlatformMediaSessionManager::m_vorbisDecoderEnabled;
#endif

#if ENABLE(OPUS) && PLATFORM(MAC)
bool PlatformMediaSessionManager::m_opusDecoderEnabled;
#endif

static std::unique_ptr<PlatformMediaSessionManager>& sharedPlatformMediaSessionManager()
{
    static NeverDestroyed<std::unique_ptr<PlatformMediaSessionManager>> platformMediaSessionManager;
    return platformMediaSessionManager.get();
}

PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    auto& manager = sharedPlatformMediaSessionManager();
    if (!manager) {
        manager = PlatformMediaSessionManager::create();
        manager->resetRestrictions();
    }
    return *manager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::sharedManagerIfExists()
{
    return sharedPlatformMediaSessionManager().get();
}

#if !PLATFORM(COCOA)
std::unique_ptr<PlatformMediaSessionManager> PlatformMediaSessionManager::create()
{
    return std::unique_ptr<PlatformMediaSessionManager>(new PlatformMediaSessionManager);
}
#endif // !PLATFORM(COCOA)

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{
    if (auto existingManager = PlatformMediaSessionManager::sharedManagerIfExists())
        existingManager->scheduleSessionStatusUpdate();
}

PlatformMediaSessionManager::PlatformMediaSessionManager()
#if !RELEASE_LOG_DISABLED
    : m_logger(AggregateLogger::create(this))
#endif
{
}

static inline unsigned indexFromMediaType(PlatformMediaSession::MediaType type)
{
    return static_cast<unsigned>(type);
}

void PlatformMediaSessionManager::resetRestrictions()
{
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Video)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Audio)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::VideoAudio)] = NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::WebAudio)] = NoRestrictions;
}

bool PlatformMediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    return anyOfSessions([type] (auto& session) {
        return session.mediaType() == type;
    });
}

bool PlatformMediaSessionManager::activeAudioSessionRequired() const
{
    return anyOfSessions([] (auto& session) {
        return session.activeAudioSessionRequired();
    });
}

bool PlatformMediaSessionManager::canProduceAudio() const
{
    return anyOfSessions([] (auto& session) {
        return session.canProduceAudio();
    });
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
        if (source.isCapturingAudio())
            ++count;
    }
    return count;
}

void PlatformMediaSessionManager::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_interrupted = true;
    forEachSession([type] (auto& session) {
        session.beginInterruption(type);
    });
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_interrupted = false;
    forEachSession([flags] (auto& session) {
        session.endInterruption(flags);
    });
}

void PlatformMediaSessionManager::addSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    m_sessions.append(makeWeakPtr(session));
    if (m_interrupted)
        session.setState(PlatformMediaSession::Interrupted);

#if !RELEASE_LOG_DISABLED
    m_logger->addLogger(session.logger());
#endif

    scheduleUpdateSessionState();
}

bool PlatformMediaSessionManager::hasNoSession() const
{
    return m_sessions.isEmpty() || std::all_of(m_sessions.begin(), m_sessions.end(), std::logical_not<void>());
}

void PlatformMediaSessionManager::removeSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    size_t index = m_sessions.find(&session);
    if (index == notFound)
        return;

    m_sessions.remove(index);

#if USE(AUDIO_SESSION)
    if (hasNoSession())
        maybeDeactivateAudioSession();
#endif

#if !RELEASE_LOG_DISABLED
    m_logger->removeLogger(session.logger());
#endif

    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)] |= restriction;
}

void PlatformMediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)] &= ~restriction;
}

PlatformMediaSessionManager::SessionRestrictions PlatformMediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    return m_restrictions[indexFromMediaType(type)];
}

bool PlatformMediaSessionManager::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    setCurrentSession(session);

    auto sessionType = session.mediaType();
    auto restrictions = this->restrictions(sessionType);
    if (session.state() == PlatformMediaSession::Interrupted && restrictions & InterruptedPlaybackNotPermitted) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false because session.state() is Interrupted, and InterruptedPlaybackNotPermitted");
        return false;
    }

#if USE(AUDIO_SESSION)
    if (activeAudioSessionRequired()) {
        if (!AudioSession::sharedSession().tryToSetActive(true)) {
            ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false failed to set active AudioSession");
            return false;
        }

        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " sucessfully activated AudioSession");
        m_becameActive = true;
    }
#endif

    if (m_interrupted)
        endInterruption(PlatformMediaSession::NoFlags);

    if (restrictions & ConcurrentPlaybackNotPermitted) {
        forEachMatchingSession([&session](auto& oneSession) {
            return &oneSession != &session
                && oneSession.state() == PlatformMediaSession::Playing
                && !oneSession.canPlayConcurrently(session);
        }, [](auto& oneSession) {
            oneSession.pauseSession();
        });
    }
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning true");
    return true;
}
    
void PlatformMediaSessionManager::sessionWillEndPlayback(PlatformMediaSession& session, DelayCallingUpdateNowPlaying)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    if (m_sessions.size() < 2)
        return;
    
    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    for (size_t i = 0, size = m_sessions.size(); i < size; ++i) {
        const auto& oneSession = *m_sessions[i];
        if (&oneSession == &session)
            pausingSessionIndex = i;
        else if (oneSession.state() == PlatformMediaSession::Playing)
            lastPlayingSessionIndex = i;
        else
            break;
    }

    if (lastPlayingSessionIndex == notFound || pausingSessionIndex == notFound)
        return;

    if (pausingSessionIndex > lastPlayingSessionIndex)
        return;

    m_sessions.remove(pausingSessionIndex);
    m_sessions.append(makeWeakPtr(session));

    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", pausingSessionIndex, " to ", lastPlayingSessionIndex);
}

void PlatformMediaSessionManager::sessionStateChanged(PlatformMediaSession&)
{
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::setCurrentSession(PlatformMediaSession& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());
    
    if (m_sessions.size() < 2)
        return;
    
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (!index || index == notFound)
        return;

    m_sessions.remove(index);
    m_sessions.insert(0, makeWeakPtr(session));
    
    ALWAYS_LOG(LOGIDENTIFIER, "session moved from index ", index, " to 0");
}
    
PlatformMediaSession* PlatformMediaSessionManager::currentSession() const
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0].get();
}

void PlatformMediaSessionManager::applicationWillBecomeInactive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()) & InactiveProcessPlaybackRestricted;
    }, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::ProcessInactive);
    });
}

void PlatformMediaSessionManager::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()) & InactiveProcessPlaybackRestricted;
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::applicationDidEnterBackground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, "suspendedUnderLock: ", suspendedUnderLock);

    if (m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = true;

    forEachSession([&] (auto& session) {
        if (suspendedUnderLock && restrictions(session.mediaType()) & SuspendedUnderLockPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::SuspendedUnderLock);
        else if (restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::EnteringBackground);
    });
}

void PlatformMediaSessionManager::applicationWillEnterForeground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, "suspendedUnderLock: ", suspendedUnderLock);

    if (!m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = false;

    forEachMatchingSession([&](auto& session) {
        return (suspendedUnderLock && restrictions(session.mediaType()) & SuspendedUnderLockPlaybackRestricted) || restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted;
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::processWillSuspend()
{
    if (m_processIsSuspended)
        return;
    m_processIsSuspended = true;

    ALWAYS_LOG(LOGIDENTIFIER);

    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });

#if USE(AUDIO_SESSION)
    maybeDeactivateAudioSession();
#endif
}

void PlatformMediaSessionManager::processDidResume()
{
    if (!m_processIsSuspended)
        return;
    m_processIsSuspended = false;

    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });

#if USE(AUDIO_SESSION)
    if (!m_becameActive && activeAudioSessionRequired()) {
        m_becameActive = AudioSession::sharedSession().tryToSetActive(true);
        ALWAYS_LOG(LOGIDENTIFIER, "tried to set active AudioSession, ", m_becameActive ? "succeeded" : "failed");
    }
#endif
}

void PlatformMediaSessionManager::setIsPlayingToAutomotiveHeadUnit(bool isPlayingToAutomotiveHeadUnit)
{
    if (isPlayingToAutomotiveHeadUnit == m_isPlayingToAutomotiveHeadUnit)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isPlayingToAutomotiveHeadUnit);
    m_isPlayingToAutomotiveHeadUnit = isPlayingToAutomotiveHeadUnit;
}

void PlatformMediaSessionManager::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSession& session)
{
    if (!m_isApplicationInBackground || !(restrictions(session.mediaType()) & BackgroundProcessPlaybackRestricted))
        return;

    if (session.state() != PlatformMediaSession::Interrupted)
        session.beginInterruption(PlatformMediaSession::EnteringBackground);
}

void PlatformMediaSessionManager::sessionCanProduceAudioChanged()
{
    updateSessionState();
}

void PlatformMediaSessionManager::processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command, argument);
}

bool PlatformMediaSessionManager::computeSupportsSeeking() const
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession)
        return false;
    return activeSession->supportsSeeking();
}

void PlatformMediaSessionManager::processSystemWillSleep()
{
    if (m_interrupted)
        return;

    forEachSession([] (auto& session) {
        session.beginInterruption(PlatformMediaSession::SystemSleep);
    });
}

void PlatformMediaSessionManager::processSystemDidWake()
{
    if (m_interrupted)
        return;

    forEachSession([] (auto& session) {
        session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::pauseAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.pauseSession();
    });
}


bool PlatformMediaSessionManager::mediaPlaybackIsPaused(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    bool mediaPlaybackIsPaused = false;
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&mediaPlaybackIsPaused](auto& session) {
        if (session.state() == PlatformMediaSession::Paused)
            mediaPlaybackIsPaused = true;
    });
    return mediaPlaybackIsPaused;
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForProcess()
{
    forEachSession([] (auto& session) {
        session.stopSession();
    });
}

void PlatformMediaSessionManager::suspendAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::PlaybackSuspended);
    });
}

void PlatformMediaSessionManager::resumeAllMediaPlaybackForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::suspendAllMediaBufferingForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.suspendBuffering();
    });
}

void PlatformMediaSessionManager::resumeAllMediaBufferingForGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.resumeBuffering();
    });
}

Vector<WeakPtr<PlatformMediaSession>> PlatformMediaSessionManager::sessionsMatching(const Function<bool(const PlatformMediaSession&)>& filter) const
{
    Vector<WeakPtr<PlatformMediaSession>> matchingSessions;
    for (auto& session : m_sessions) {
        if (filter(*session))
            matchingSessions.append(session);
    }
    return matchingSessions;
}

void PlatformMediaSessionManager::forEachMatchingSession(const Function<bool(const PlatformMediaSession&)>& predicate, const Function<void(PlatformMediaSession&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

void PlatformMediaSessionManager::forEachSessionInGroup(MediaSessionGroupIdentifier mediaSessionGroupIdentifier, const Function<void(PlatformMediaSession&)>& callback)
{
    if (!mediaSessionGroupIdentifier)
        return;

    forEachMatchingSession([mediaSessionGroupIdentifier](auto& session) {
        return session.client().mediaSessionGroupIdentifier() == mediaSessionGroupIdentifier;
    }, [&callback](auto& session) {
        callback(session);
    });
}

void PlatformMediaSessionManager::forEachSession(const Function<void(PlatformMediaSession&)>& callback)
{
    auto sessions = m_sessions;
    for (auto& session : sessions) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

bool PlatformMediaSessionManager::anyOfSessions(const Function<bool(const PlatformMediaSession&)>& predicate) const
{
    return WTF::anyOf(m_sessions, [&predicate](const auto& session) {
        return predicate(*session);
    });
}

void PlatformMediaSessionManager::addAudioCaptureSource(PlatformMediaSession::AudioCaptureSource& source)
{
    ASSERT(!m_audioCaptureSources.contains(source));
    m_audioCaptureSources.add(source);
    scheduleUpdateSessionState();
}


void PlatformMediaSessionManager::removeAudioCaptureSource(PlatformMediaSession::AudioCaptureSource& source)
{
    m_audioCaptureSources.remove(source);
    scheduleUpdateSessionState();
}

void PlatformMediaSessionManager::scheduleUpdateSessionState()
{
    if (updateSessionStateQueue.hasPendingTasks())
        return;

    updateSessionStateQueue.enqueueTask([this] {
        updateSessionState();
    });
}

#if USE(AUDIO_SESSION)
void PlatformMediaSessionManager::maybeDeactivateAudioSession()
{
    if (!m_becameActive || !shouldDeactivateAudioSession())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "tried to set inactive AudioSession");
    AudioSession::sharedSession().tryToSetActive(false);
    m_becameActive = false;
}
#endif

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

bool PlatformMediaSessionManager::webMFormatReaderEnabled()
{
#if ENABLE(WEBM_FORMAT_READER)
    return m_webMFormatReaderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setWebMFormatReaderEnabled(bool enabled)
{
#if ENABLE(WEBM_FORMAT_READER)
    m_webMFormatReaderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

bool PlatformMediaSessionManager::vorbisDecoderEnabled()
{
#if ENABLE(VORBIS) && PLATFORM(MAC)
    return m_vorbisDecoderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setVorbisDecoderEnabled(bool enabled)
{
#if ENABLE(VORBIS) && PLATFORM(MAC)
    m_vorbisDecoderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

bool PlatformMediaSessionManager::opusDecoderEnabled()
{
#if ENABLE(OPUS) && PLATFORM(MAC)
    return m_opusDecoderEnabled;
#else
    return false;
#endif
}

void PlatformMediaSessionManager::setOpusDecoderEnabled(bool enabled)
{
#if ENABLE(OPUS) && PLATFORM(MAC)
    m_opusDecoderEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

#else // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{

}

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PlatformMediaSessionManager::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore
