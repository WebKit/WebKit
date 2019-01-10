/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#if !PLATFORM(COCOA)
static PlatformMediaSessionManager* platformMediaSessionManager = nullptr;

PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    if (!platformMediaSessionManager)
        platformMediaSessionManager = new PlatformMediaSessionManager;
    return *platformMediaSessionManager;
}

PlatformMediaSessionManager* PlatformMediaSessionManager::sharedManagerIfExists()
{
    return platformMediaSessionManager;
}
#endif // !PLATFORM(COCOA)

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{
    if (auto existingManager = PlatformMediaSessionManager::sharedManagerIfExists())
        existingManager->scheduleUpdateNowPlayingInfo();
}

PlatformMediaSessionManager::PlatformMediaSessionManager()
    : m_systemSleepListener(PAL::SystemSleepListener::create(*this))
{
    resetRestrictions();
}

void PlatformMediaSessionManager::resetRestrictions()
{
    m_restrictions[PlatformMediaSession::Video] = NoRestrictions;
    m_restrictions[PlatformMediaSession::Audio] = NoRestrictions;
    m_restrictions[PlatformMediaSession::VideoAudio] = NoRestrictions;
    m_restrictions[PlatformMediaSession::WebAudio] = NoRestrictions;
    m_restrictions[PlatformMediaSession::MediaStreamCapturingAudio] = NoRestrictions;
}

bool PlatformMediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::MediaStreamCapturingAudio);

    return anyOfSessions([type] (PlatformMediaSession& session, size_t) {
        return session.mediaType() == type;
    });
}

bool PlatformMediaSessionManager::activeAudioSessionRequired() const
{
    return anyOfSessions([] (PlatformMediaSession& session, size_t) {
        return session.activeAudioSessionRequired();
    });
}

bool PlatformMediaSessionManager::canProduceAudio() const
{
    return anyOfSessions([] (PlatformMediaSession& session, size_t) {
        return session.canProduceAudio();
    });
}

int PlatformMediaSessionManager::count(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::MediaStreamCapturingAudio);

    int count = 0;
    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            ++count;
    }

    return count;
}

void PlatformMediaSessionManager::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    LOG(Media, "PlatformMediaSessionManager::beginInterruption");

    m_interrupted = true;
    forEachSession([type] (PlatformMediaSession& session, size_t) {
        session.beginInterruption(type);
    });
    updateSessionState();
}

void PlatformMediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    LOG(Media, "PlatformMediaSessionManager::endInterruption");

    m_interrupted = false;
    forEachSession([flags] (PlatformMediaSession& session, size_t) {
        session.endInterruption(flags);
    });
}

void PlatformMediaSessionManager::addSession(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::addSession - %p", &session);
    
    m_sessions.append(&session);
    if (m_interrupted)
        session.setState(PlatformMediaSession::Interrupted);

    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);

    if (!m_audioHardwareListener)
        m_audioHardwareListener = AudioHardwareListener::create(*this);

    updateSessionState();
}

void PlatformMediaSessionManager::removeSession(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::removeSession - %p", &session);
    
    size_t index = m_sessions.find(&session);
    if (index == notFound)
        return;

    if (m_iteratingOverSessions)
        m_sessions.at(index) = nullptr;
    else
        m_sessions.remove(index);

    if (m_sessions.isEmpty() || std::all_of(m_sessions.begin(), m_sessions.end(), std::logical_not<void>())) {
        m_remoteCommandListener = nullptr;
        m_audioHardwareListener = nullptr;
#if USE(AUDIO_SESSION)
        if (m_becameActive && shouldDeactivateAudioSession()) {
            AudioSession::sharedSession().tryToSetActive(false);
            m_becameActive = false;
        }
#endif
    }

    updateSessionState();
}

void PlatformMediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::MediaStreamCapturingAudio);
    m_restrictions[type] |= restriction;
}

void PlatformMediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::MediaStreamCapturingAudio);
    m_restrictions[type] &= ~restriction;
}

PlatformMediaSessionManager::SessionRestrictions PlatformMediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::MediaStreamCapturingAudio);
    return m_restrictions[type];
}

bool PlatformMediaSessionManager::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::sessionWillBeginPlayback - %p", &session);
    
    setCurrentSession(session);

    PlatformMediaSession::MediaType sessionType = session.mediaType();
    SessionRestrictions restrictions = m_restrictions[sessionType];
    if (session.state() == PlatformMediaSession::Interrupted && restrictions & InterruptedPlaybackNotPermitted)
        return false;

#if USE(AUDIO_SESSION)
    if (activeAudioSessionRequired() && !AudioSession::sharedSession().tryToSetActive(true))
        return false;

    m_becameActive = true;
#endif

    if (m_interrupted)
        endInterruption(PlatformMediaSession::NoFlags);

    forEachSession([&] (PlatformMediaSession& oneSession, size_t) {
        if (&oneSession == &session)
            return;
        if (oneSession.mediaType() == sessionType
            && restrictions & ConcurrentPlaybackNotPermitted
            && oneSession.state() == PlatformMediaSession::Playing)
            oneSession.pauseSession();
    });

    return true;
}
    
void PlatformMediaSessionManager::sessionWillEndPlayback(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::sessionWillEndPlayback - %p", &session);
    
    if (m_sessions.size() < 2)
        return;
    
    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    anyOfSessions([&] (PlatformMediaSession& oneSession, size_t i) {
        if (&oneSession == &session) {
            pausingSessionIndex = i;
            return false;
        }
        if (oneSession.state() == PlatformMediaSession::Playing) {
            lastPlayingSessionIndex = i;
            return false;
        }
        return oneSession.state() != PlatformMediaSession::Playing;
    });
    if (lastPlayingSessionIndex == notFound || pausingSessionIndex == notFound)
        return;
    
    if (pausingSessionIndex > lastPlayingSessionIndex)
        return;
    
    m_sessions.remove(pausingSessionIndex);
    m_sessions.insert(lastPlayingSessionIndex, &session);
    
    LOG(Media, "PlatformMediaSessionManager::sessionWillEndPlayback - session moved from index %zu to %zu", pausingSessionIndex, lastPlayingSessionIndex);
}

void PlatformMediaSessionManager::sessionStateChanged(PlatformMediaSession&)
{
    updateSessionState();
}

void PlatformMediaSessionManager::setCurrentSession(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::setCurrentSession - %p", &session);
    
    if (m_sessions.size() < 2)
        return;
    
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (!index || index == notFound)
        return;

    m_sessions.remove(index);
    m_sessions.insert(0, &session);
    if (m_remoteCommandListener)
        m_remoteCommandListener->updateSupportedCommands();
    
    LOG(Media, "PlatformMediaSessionManager::setCurrentSession - session moved from index %zu to 0", index);
}
    
PlatformMediaSession* PlatformMediaSessionManager::currentSession() const
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0];
}

Vector<PlatformMediaSession*> PlatformMediaSessionManager::currentSessionsMatching(const WTF::Function<bool(const PlatformMediaSession&)>& filter)
{
    Vector<PlatformMediaSession*> matchingSessions;
    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (filter(session))
            matchingSessions.append(&session);
    });
    return matchingSessions;
}

void PlatformMediaSessionManager::applicationWillBecomeInactive() const
{
    LOG(Media, "PlatformMediaSessionManager::applicationWillBecomeInactive");

    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (m_restrictions[session.mediaType()] & InactiveProcessPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::ProcessInactive);
    });
}

void PlatformMediaSessionManager::applicationDidBecomeActive() const
{
    LOG(Media, "PlatformMediaSessionManager::applicationDidBecomeActive");

    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (m_restrictions[session.mediaType()] & InactiveProcessPlaybackRestricted)
            session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::applicationDidEnterBackground(bool suspendedUnderLock) const
{
    LOG(Media, "PlatformMediaSessionManager::applicationDidEnterBackground - suspendedUnderLock(%d)", suspendedUnderLock);

    if (m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = true;

    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (suspendedUnderLock && m_restrictions[session.mediaType()] & SuspendedUnderLockPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::SuspendedUnderLock);
        else if (m_restrictions[session.mediaType()] & BackgroundProcessPlaybackRestricted)
            session.beginInterruption(PlatformMediaSession::EnteringBackground);
    });
}

void PlatformMediaSessionManager::applicationWillEnterForeground(bool suspendedUnderLock) const
{
    LOG(Media, "PlatformMediaSessionManager::applicationWillEnterForeground - suspendedUnderLock(%d)", suspendedUnderLock);

    if (!m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = false;

    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if ((suspendedUnderLock && m_restrictions[session.mediaType()] & SuspendedUnderLockPlaybackRestricted) || m_restrictions[session.mediaType()] & BackgroundProcessPlaybackRestricted)
            session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSession& session)
{
    if (!m_isApplicationInBackground || !(m_restrictions[session.mediaType()] & BackgroundProcessPlaybackRestricted))
        return;

    if (session.state() != PlatformMediaSession::Interrupted)
        session.beginInterruption(PlatformMediaSession::EnteringBackground);
}

void PlatformMediaSessionManager::sessionCanProduceAudioChanged(PlatformMediaSession&)
{
    updateSessionState();
}

void PlatformMediaSessionManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument* argument)
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command, argument);
}

bool PlatformMediaSessionManager::supportsSeeking() const
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession)
        return false;
    return activeSession->supportsSeeking();
}

void PlatformMediaSessionManager::systemWillSleep()
{
    if (m_interrupted)
        return;

    forEachSession([] (PlatformMediaSession& session, size_t) {
        session.beginInterruption(PlatformMediaSession::SystemSleep);
    });
}

void PlatformMediaSessionManager::systemDidWake()
{
    if (m_interrupted)
        return;

    forEachSession([] (PlatformMediaSession& session, size_t) {
        session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::audioOutputDeviceChanged()
{
    updateSessionState();
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForDocument(const Document* document)
{
    forEachSession([document] (PlatformMediaSession& session, size_t) {
        if (session.client().hostingDocument() == document)
            session.pauseSession();
    });
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForProcess()
{
    forEachSession([] (PlatformMediaSession& session, size_t) {
        session.stopSession();
    });
}

void PlatformMediaSessionManager::suspendAllMediaPlaybackForDocument(const Document& document)
{
    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (session.client().hostingDocument() == &document)
            session.beginInterruption(PlatformMediaSession::PlaybackSuspended);
    });
}

void PlatformMediaSessionManager::resumeAllMediaPlaybackForDocument(const Document& document)
{
    forEachSession([&] (PlatformMediaSession& session, size_t) {
        if (session.client().hostingDocument() == &document)
            session.endInterruption(PlatformMediaSession::MayResumePlaying);
    });
}

void PlatformMediaSessionManager::forEachSession(const Function<void(PlatformMediaSession&, size_t)>& predicate) const
{
    ++m_iteratingOverSessions;

    for (size_t i = 0, size = m_sessions.size(); i < size; ++i) {
        auto session = m_sessions[i];
        if (!session)
            continue;
        predicate(*session, i);
    }

    --m_iteratingOverSessions;
    if (!m_iteratingOverSessions)
        m_sessions.removeAll(nullptr);
}

PlatformMediaSession* PlatformMediaSessionManager::findSession(const Function<bool(PlatformMediaSession&, size_t)>& predicate) const
{
    ++m_iteratingOverSessions;

    PlatformMediaSession* foundSession = nullptr;
    for (size_t i = 0, size = m_sessions.size(); i < size; ++i) {
        auto session = m_sessions[i];
        if (!session)
            continue;

        if (!predicate(*session, i))
            continue;

        foundSession = session;
        break;
    }

    --m_iteratingOverSessions;
    if (!m_iteratingOverSessions)
        m_sessions.removeAll(nullptr);

    return foundSession;
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

#else // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

void PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary()
{

}

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

} // namespace WebCore
