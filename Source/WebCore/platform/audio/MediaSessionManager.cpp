/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include "MediaSession.h"

namespace WebCore {

#if !PLATFORM(IOS)
MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEFINE_STATIC_LOCAL(MediaSessionManager, manager, ());
    return manager;
}
#endif

MediaSessionManager::MediaSessionManager()
    : m_activeSession(nullptr)
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

void MediaSessionManager::beginInterruption()
{
    LOG(Media, "MediaSessionManager::beginInterruption");

    m_interrupted = true;
    for (auto* session : m_sessions)
        session->beginInterruption();
}

void MediaSessionManager::endInterruption(MediaSession::EndInterruptionFlags flags)
{
    LOG(Media, "MediaSessionManager::endInterruption");

    m_interrupted = false;
    for (auto* session : m_sessions)
        session->endInterruption(flags);
}

void MediaSessionManager::addSession(MediaSession& session)
{
    m_sessions.append(&session);
    if (m_interrupted)
        session.setState(MediaSession::Interrupted);
    updateSessionState();
}

void MediaSessionManager::removeSession(MediaSession& session)
{
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (index == notFound)
        return;
    
    if (m_activeSession == &session)
        setCurrentSession(nullptr);
    
    m_sessions.remove(index);
    updateSessionState();
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

void MediaSessionManager::sessionWillBeginPlayback(const MediaSession& session)
{
    setCurrentSession(&session);
    
    MediaSession::MediaType sessionType = session.mediaType();
    SessionRestrictions restrictions = m_restrictions[sessionType];
    if (!restrictions & ConcurrentPlaybackNotPermitted)
        return;

    for (auto* oneSession : m_sessions) {
        if (oneSession == &session)
            continue;
        if (oneSession->mediaType() != sessionType)
            continue;
        if (restrictions & ConcurrentPlaybackNotPermitted)
            oneSession->pauseSession();
    }
}

bool MediaSessionManager::sessionRestrictsInlineVideoPlayback(const MediaSession& session) const
{
    MediaSession::MediaType sessionType = session.mediaType();
    if (sessionType != MediaSession::Video)
        return false;

    return m_restrictions[sessionType] & InlineVideoPlaybackRestricted;
}

void MediaSessionManager::applicationWillEnterBackground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterBackground");
    for (auto* session : m_sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundPlaybackNotPermitted)
            session->beginInterruption();
    }
}

void MediaSessionManager::applicationWillEnterForeground() const
{
    LOG(Media, "MediaSessionManager::applicationWillEnterForeground");
    for (auto* session : m_sessions) {
        if (m_restrictions[session->mediaType()] & BackgroundPlaybackNotPermitted)
            session->endInterruption(MediaSession::MayResumePlaying);
    }
}

#if !PLATFORM(COCOA)
void MediaSessionManager::updateSessionState()
{
}
#endif

}
