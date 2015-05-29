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
#include "MediaSessionManager.h"

#if ENABLE(VIDEO)

#include "AudioSession.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PlatformMediaSession.h"

namespace WebCore {

#if !PLATFORM(IOS)
MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(MediaSessionManager, manager, ());
    return manager;
}
#endif

MediaSessionManager::MediaSessionManager()
    : m_systemSleepListener(SystemSleepListener::create(*this))
{
    resetRestrictions();
}

void MediaSessionManager::resetRestrictions()
{
    m_restrictions[PlatformMediaSession::Video] = NoRestrictions;
    m_restrictions[PlatformMediaSession::Audio] = NoRestrictions;
    m_restrictions[PlatformMediaSession::WebAudio] = NoRestrictions;
}

bool MediaSessionManager::has(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);

    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            return true;
    }

    return false;
}

bool MediaSessionManager::activeAudioSessionRequired() const
{
    for (auto* session : m_sessions) {
        if (session->mediaType() != PlatformMediaSession::None && session->state() == PlatformMediaSession::State::Playing)
            return true;
    }
    
    return false;
}

int MediaSessionManager::count(PlatformMediaSession::MediaType type) const
{
    ASSERT(type >= PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);

    int count = 0;
    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            ++count;
    }

    return count;
}

void MediaSessionManager::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    LOG(Media, "MediaSessionManager::beginInterruption");

    m_interrupted = true;
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->beginInterruption(type);
    updateSessionState();
}

void MediaSessionManager::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    LOG(Media, "MediaSessionManager::endInterruption");

    m_interrupted = false;
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->endInterruption(flags);
}

void MediaSessionManager::addSession(PlatformMediaSession& session)
{
    LOG(Media, "MediaSessionManager::addSession - %p", &session);
    
    m_sessions.append(&session);
    if (m_interrupted)
        session.setState(PlatformMediaSession::Interrupted);

    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);

    if (!m_audioHardwareListener)
        m_audioHardwareListener = AudioHardwareListener::create(*this);

    updateSessionState();
}

void MediaSessionManager::removeSession(PlatformMediaSession& session)
{
    LOG(Media, "MediaSessionManager::removeSession - %p", &session);
    
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

void MediaSessionManager::addRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
    m_restrictions[type] |= restriction;
}

void MediaSessionManager::removeRestriction(PlatformMediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
    m_restrictions[type] &= ~restriction;
}

MediaSessionManager::SessionRestrictions MediaSessionManager::restrictions(PlatformMediaSession::MediaType type)
{
    ASSERT(type > PlatformMediaSession::None && type <= PlatformMediaSession::WebAudio);
    return m_restrictions[type];
}

bool MediaSessionManager::sessionWillBeginPlayback(PlatformMediaSession& session)
{
    LOG(Media, "MediaSessionManager::sessionWillBeginPlayback - %p", &session);
    
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
    
void MediaSessionManager::sessionWillEndPlayback(PlatformMediaSession& session)
{
    LOG(Media, "MediaSessionManager::sessionWillEndPlayback - %p", &session);
    
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
    
    LOG(Media, "MediaSessionManager::sessionWillEndPlayback - session moved from index %zu to %zu", pausingSessionIndex, lastPlayingSessionIndex);
}

void MediaSessionManager::setCurrentSession(PlatformMediaSession& session)
{
    LOG(Media, "MediaSessionManager::setCurrentSession - %p", &session);
    
    if (m_sessions.size() < 2)
        return;
    
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (!index || index == notFound)
        return;

    m_sessions.remove(index);
    m_sessions.insert(0, &session);
    
    LOG(Media, "MediaSessionManager::setCurrentSession - session moved from index %zu to 0", index);
}
    
PlatformMediaSession* MediaSessionManager::currentSession()
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0];
}
    
bool MediaSessionManager::sessionRestrictsInlineVideoPlayback(const PlatformMediaSession& session) const
{
    PlatformMediaSession::MediaType sessionType = session.presentationType();
    if (sessionType != PlatformMediaSession::Video)
        return false;

    return m_restrictions[sessionType] & InlineVideoPlaybackRestricted;
}

bool MediaSessionManager::sessionCanLoadMedia(const PlatformMediaSession& session) const
{
    return session.state() == PlatformMediaSession::Playing || !session.isHidden() || session.isPlayingToWirelessPlaybackTarget();
}

void MediaSessionManager::applicationWillEnterBackground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterBackground");
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundProcessPlaybackRestricted)
            session->beginInterruption(PlatformMediaSession::EnteringBackground);
    }
}

void MediaSessionManager::applicationWillEnterForeground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterForeground");
    Vector<PlatformMediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundProcessPlaybackRestricted)
            session->endInterruption(PlatformMediaSession::MayResumePlaying);
    }
}

#if !PLATFORM(COCOA)
void MediaSessionManager::updateSessionState()
{
}
#endif

void MediaSessionManager::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    PlatformMediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command);
}

void MediaSessionManager::systemWillSleep()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->beginInterruption(PlatformMediaSession::SystemSleep);
}

void MediaSessionManager::systemDidWake()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->endInterruption(PlatformMediaSession::MayResumePlaying);
}

void MediaSessionManager::audioOutputDeviceChanged()
{
    updateSessionState();
}

}

#endif
