/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PlatformMediaSession.h"

namespace WebCore {

#if !PLATFORM(IOS)
PlatformMediaSessionManager& PlatformMediaSessionManager::sharedManager()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(PlatformMediaSessionManager, manager, ());
    return manager;
}
#endif

PlatformMediaSessionManager::PlatformMediaSessionManager()
    : m_systemSleepListener(SystemSleepListener::create(*this))
{
    resetRestrictions();
}

void PlatformMediaSessionManager::resetRestrictions()
{
    m_restrictions[PlatformMediaSession::Video] = NoRestrictions;
    m_restrictions[PlatformMediaSession::Audio] = NoRestrictions;
    m_restrictions[PlatformMediaSession::WebAudio] = NoRestrictions;
}

bool PlatformMediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);

    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            return true;
    }

    return false;
}

bool PlatformMediaSessionManager::activeAudioSessionRequired() const
{
    for (auto* session : m_sessions) {
        if (session->mediaType() != PlatformMediaSession::None && session->state() == PlatformMediaSession::State::Playing)
            return true;
    }
    
    return false;
}

int PlatformMediaSessionManager::count(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);

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
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->beginInterruption(type);
    updateSessionState();
}

void PlatformMediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    LOG(Media, "PlatformMediaSessionManager::endInterruption");

    m_interrupted = false;
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->endInterruption(flags);
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
    ASSERT(index != notFound);
    if (index == notFound)
        return;
    
    m_sessions.remove(index);

    if (m_sessions.isEmpty()) {
        m_remoteCommandListener = nullptr;
        m_audioHardwareListener = nullptr;
    }

    updateSessionState();
}

void PlatformMediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
    m_restrictions[type] |= restriction;
}

void PlatformMediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
    m_restrictions[type] &= ~restriction;
}

PlatformMediaSessionManager::SessionRestrictions PlatformMediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
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
#endif

    if (m_interrupted)
        endInterruption(PlatformMediaSession::NoFlags);

    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* oneSession : sessions) {
        if (oneSession == &session)
            continue;
        if (oneSession->mediaType() == sessionType && restrictions & ConcurrentPlaybackNotPermitted)
            oneSession->pauseSession();
    }

    updateSessionState();
    return true;
}
    
void PlatformMediaSessionManager::sessionWillEndPlayback(PlatformMediaSession& session)
{
    LOG(Media, "PlatformMediaSessionManager::sessionWillEndPlayback - %p", &session);
    
    if (m_sessions.size() < 2)
        return;
    
    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    for (size_t i = 0; i < m_sessions.size(); ++i) {
        PlatformMediaSession* oneSession = m_sessions[i];
        
        if (oneSession == &session) {
            pausingSessionIndex = i;
            continue;
        }
        if (oneSession->state() == PlatformMediaSession::Playing) {
            lastPlayingSessionIndex = i;
            continue;
        }
        if (oneSession->state() != PlatformMediaSession::Playing)
            break;
    }
    if (lastPlayingSessionIndex == notFound || pausingSessionIndex == notFound)
        return;
    
    if (pausingSessionIndex > lastPlayingSessionIndex)
        return;
    
    m_sessions.remove(pausingSessionIndex);
    m_sessions.insert(lastPlayingSessionIndex, &session);
    
    LOG(Media, "PlatformMediaSessionManager::sessionWillEndPlayback - session moved from index %zu to %zu", pausingSessionIndex, lastPlayingSessionIndex);
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
    
    LOG(Media, "PlatformMediaSessionManager::setCurrentSession - session moved from index %zu to 0", index);
}
    
PlatformMediaSession* PlatformMediaSessionManager::currentSession()
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0];
}
    
bool PlatformMediaSessionManager::sessionRestrictsInlineVideoPlayback(const PlatformMediaSession& session) const
{
    PlatformMediaSession::MediaType sessionType = session.presentationType();
    if (sessionType != PlatformMediaSession::Video)
        return false;

    return m_restrictions[sessionType] & InlineVideoPlaybackRestricted;
}

bool PlatformMediaSessionManager::sessionCanLoadMedia(const PlatformMediaSession& session) const
{
    return session.state() == PlatformMediaSession::Playing || !session.isHidden() || session.isPlayingToWirelessPlaybackTarget();
}

void PlatformMediaSessionManager::applicationWillEnterBackground() const
{
    LOG(Media, "PlatformMediaSessionManager::applicationWillEnterBackground");
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundProcessPlaybackRestricted)
            session->beginInterruption(PlatformMediaSession::EnteringBackground);
    }
}

void PlatformMediaSessionManager::applicationDidEnterBackground(bool isSuspendedUnderLock) const
{
    LOG(Media, "PlatformMediaSessionManager::applicationDidEnterBackground");

    if (!isSuspendedUnderLock)
        return;

    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundProcessPlaybackRestricted)
            session->forceInterruption(PlatformMediaSession::EnteringBackground);
    }
}

void PlatformMediaSessionManager::applicationWillEnterForeground() const
{
    LOG(Media, "PlatformMediaSessionManager::applicationWillEnterForeground");
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundProcessPlaybackRestricted)
            session->endInterruption(PlatformMediaSession::MayResumePlaying);
    }
}

#if !PLATFORM(COCOA)
void PlatformMediaSessionManager::updateSessionState()
{
}
#endif

void PlatformMediaSessionManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command);
}

void PlatformMediaSessionManager::systemWillSleep()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->beginInterruption(PlatformMediaSession::SystemSleep);
}

void PlatformMediaSessionManager::systemDidWake()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->endInterruption(PlatformMediaSession::MayResumePlaying);
}

void PlatformMediaSessionManager::audioOutputDeviceChanged()
{
    updateSessionState();
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForDocument(const Document* document)
{
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (session->client().hostingDocument() == document)
            session->pauseSession();
    }
}

void PlatformMediaSessionManager::stopAllMediaPlaybackForProcess()
{
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->pauseSession();
}

}

#endif
