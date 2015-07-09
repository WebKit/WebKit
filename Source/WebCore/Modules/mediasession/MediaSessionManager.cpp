/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSessionManager.h"

#if ENABLE(MEDIA_SESSION)

#include "MediaSession.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const char* contentSessionKind = "content";

MediaSessionManager& MediaSessionManager::singleton()
{
    static NeverDestroyed<MediaSessionManager> manager;
    return manager;
}

bool MediaSessionManager::hasActiveMediaElements() const
{
    for (auto* session : m_sessions) {
        if (session->hasActiveMediaElements())
            return true;
    }

    return false;
}

void MediaSessionManager::addMediaSession(MediaSession& session)
{
    m_sessions.add(&session);
}

void MediaSessionManager::removeMediaSession(MediaSession& session)
{
    m_sessions.remove(&session);
}

void MediaSessionManager::togglePlayback()
{
    for (auto* session : m_sessions) {
        String sessionKind = session->kind();
        if (session->currentState() == MediaSession::State::Active && (sessionKind == contentSessionKind || sessionKind == ""))
            session->togglePlayback();
    }
}

void MediaSessionManager::skipToNextTrack()
{
    // 5.2.2 When the user presses the MediaTrackNext media key, then for each Content-based media session that is
    // currently ACTIVE and has a media remote controller with its nextTrackEnabled attribute set to true, queue a task
    // to fire a simple event named nexttrack at its media remote controller.
    for (auto* session : m_sessions) {
        if (session->currentState() == MediaSession::State::Active && session->kind() == contentSessionKind)
            session->skipToNextTrack();
    }
}

void MediaSessionManager::skipToPreviousTrack()
{
    // 5.2.2 When the user presses the MediaTrackPrevious media key, then for each Content-based media session that is
    // currently ACTIVE and has a media remote controller with its previousTrackEnabled attribute set to true, queue a task
    // to fire a simple event named previoustrack at its media remote controller.
    for (auto* session : m_sessions) {
        if (session->currentState() == MediaSession::State::Active && session->kind() == contentSessionKind)
            session->skipToPreviousTrack();
    }
}

}

#endif /* ENABLE(MEDIA_SESSION) */
