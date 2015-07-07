/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "NotImplemented.h"
#include "MediaSession.h"

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
    , m_interrupted(false)
{
    resetRestrictions();
}

void MediaSessionManager::resetRestrictions()
{
    m_restrictions[MediaSession::Video] = NoRestrictions;
    m_restrictions[MediaSession::Audio] = NoRestrictions;
    m_restrictions[MediaSession::WebAudio] = NoRestrictions;
}

bool MediaSessionManager::has(MediaSession::MediaType type) const
{
    ASSERT(type >= MediaSession::None && type <= MediaSession::WebAudio);

    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            return true;
    }

    return false;
}

bool MediaSessionManager::activeAudioSessionRequired() const
{
    for (auto* session : m_sessions) {
        if (session->mediaType() != MediaSession::None && session->state() == MediaSession::State::Playing)
            return true;
    }
    
    return false;
}

int MediaSessionManager::count(MediaSession::MediaType type) const
{
    ASSERT(type >= MediaSession::None && type <= MediaSession::WebAudio);

    int count = 0;
    for (auto* session : m_sessions) {
        if (session->mediaType() == type)
            ++count;
    }

    return count;
}

void MediaSessionManager::beginInterruption(MediaSession::InterruptionType type)
{
    LOG(Media, "MediaSessionManager::beginInterruption");

    m_interrupted = true;
    Vector<MediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->beginInterruption(type);
    updateSessionState();
}

void MediaSessionManager::endInterruption(MediaSession::EndInterruptionFlags flags)
{
    LOG(Media, "MediaSessionManager::endInterruption");

    m_interrupted = false;
    Vector<MediaSession*> sessions = m_sessions;
    for (auto* session : sessions)
        session->endInterruption(flags);
}

void MediaSessionManager::addSession(MediaSession& session)
{
    LOG(Media, "MediaSessionManager::addSession - %p", &session);
    
    m_sessions.append(&session);
    if (m_interrupted)
        session.setState(MediaSession::Interrupted);

    if (!m_remoteCommandListener)
        m_remoteCommandListener = RemoteCommandListener::create(*this);

    if (!m_audioHardwareListener)
        m_audioHardwareListener = AudioHardwareListener::create(*this);

    updateSessionState();

    if (m_clients.isEmpty() || !(session.mediaType() == MediaSession::Video || session.mediaType() == MediaSession::Audio))
        return;

    for (auto& client : m_clients)
        client->startListeningForRemoteControlCommands();
}

void MediaSessionManager::removeSession(MediaSession& session)
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

    if (m_clients.isEmpty() || !(session.mediaType() == MediaSession::Video || session.mediaType() == MediaSession::Audio))
        return;

    for (auto& client : m_clients)
        client->startListeningForRemoteControlCommands();
}

void MediaSessionManager::addRestriction(MediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > MediaSession::None && type <= MediaSession::WebAudio);
    m_restrictions[type] |= restriction;
}

void MediaSessionManager::removeRestriction(MediaSession::MediaType type, SessionRestrictions restriction)
{
    ASSERT(type > MediaSession::None && type <= MediaSession::WebAudio);
    m_restrictions[type] &= ~restriction;
}

MediaSessionManager::SessionRestrictions MediaSessionManager::restrictions(MediaSession::MediaType type)
{
    ASSERT(type > MediaSession::None && type <= MediaSession::WebAudio);
    return m_restrictions[type];
}

void MediaSessionManager::sessionWillBeginPlayback(MediaSession& session)
{
    LOG(Media, "MediaSessionManager::sessionWillBeginPlayback - %p", &session);
    
    setCurrentSession(session);

    if (!m_clients.isEmpty() && (session.mediaType() == MediaSession::Video || session.mediaType() == MediaSession::Audio)) {
        for (auto& client : m_clients)
            client->didBeginPlayback();
    }

    MediaSession::MediaType sessionType = session.mediaType();
    SessionRestrictions restrictions = m_restrictions[sessionType];
    if (!restrictions & ConcurrentPlaybackNotPermitted)
        return;

    Vector<MediaSession*> sessions = m_sessions;
    for (auto* oneSession : sessions) {
        if (oneSession == &session)
            continue;
        if (oneSession->mediaType() != sessionType)
            continue;
        if (restrictions & ConcurrentPlaybackNotPermitted)
            oneSession->pauseSession();
    }
    
    updateSessionState();
}
    
void MediaSessionManager::sessionWillEndPlayback(MediaSession& session)
{
    LOG(Media, "MediaSessionManager::sessionWillEndPlayback - %p", &session);
    
    if (m_sessions.size() < 2)
        return;
    
    size_t pausingSessionIndex = notFound;
    size_t lastPlayingSessionIndex = notFound;
    for (size_t i = 0; i < m_sessions.size(); ++i) {
        MediaSession* oneSession = m_sessions[i];
        
        if (oneSession == &session) {
            pausingSessionIndex = i;
            continue;
        }
        if (oneSession->state() == MediaSession::Playing) {
            lastPlayingSessionIndex = i;
            continue;
        }
        if (oneSession->state() != MediaSession::Playing)
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

void MediaSessionManager::setCurrentSession(MediaSession& session)
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
    
MediaSession* MediaSessionManager::currentSession()
{
    if (!m_sessions.size())
        return nullptr;

    return m_sessions[0];
}
    
bool MediaSessionManager::sessionRestrictsInlineVideoPlayback(const MediaSession& session) const
{
    MediaSession::MediaType sessionType = session.presentationType();
    if (sessionType != MediaSession::Video)
        return false;

    return m_restrictions[sessionType] & InlineVideoPlaybackRestricted;
}

void MediaSessionManager::applicationWillEnterBackground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterBackground");
    Vector<MediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundPlaybackNotPermitted)
            session->beginInterruption(MediaSession::EnteringBackground);
    }
}

void MediaSessionManager::applicationWillEnterForeground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterForeground");
    Vector<MediaSession*> sessions = m_sessions;
    for (auto* session : sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundPlaybackNotPermitted)
            session->endInterruption(MediaSession::MayResumePlaying);
    }
}

void MediaSessionManager::wirelessRoutesAvailableChanged()
{
    notImplemented();
}

#if !PLATFORM(COCOA)
void MediaSessionManager::updateSessionState()
{
}
#endif

void MediaSessionManager::didReceiveRemoteControlCommand(MediaSession::RemoteControlCommandType command)
{
    MediaSession* activeSession = currentSession();
    if (!activeSession || !activeSession->canReceiveRemoteControlCommands())
        return;
    activeSession->didReceiveRemoteControlCommand(command);
}

void MediaSessionManager::addClient(MediaSessionManagerClient* client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.append(client);
}

void MediaSessionManager::removeClient(MediaSessionManagerClient* client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(m_clients.find(client));
}

void MediaSessionManager::systemWillSleep()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->beginInterruption(MediaSession::SystemSleep);
}

void MediaSessionManager::systemDidWake()
{
    if (m_interrupted)
        return;

    for (auto session : m_sessions)
        session->endInterruption(MediaSession::MayResumePlaying);
}

void MediaSessionManager::audioOutputDeviceChanged()
{
    updateSessionState();
}

}

#endif
