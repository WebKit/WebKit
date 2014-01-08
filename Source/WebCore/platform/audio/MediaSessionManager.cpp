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

#include "MediaSession.h"

namespace WebCore {

MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEFINE_STATIC_LOCAL(MediaSessionManager, manager, ());
    return manager;
}

MediaSessionManager::MediaSessionManager()
    : m_interruptions(0)
{
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
    if (++m_interruptions > 1)
        return;

    for (auto* session : m_sessions)
        session->beginInterruption();
}

void MediaSessionManager::endInterruption(MediaSession::EndInterruptionFlags flags)
{
    ASSERT(m_interruptions > 0);
    if (--m_interruptions)
        return;
    
    for (auto* session : m_sessions)
        session->endInterruption(flags);
}

void MediaSessionManager::addSession(MediaSession& session)
{
    m_sessions.append(&session);
    session.setState(m_interruptions ? MediaSession::Interrupted : MediaSession::Running);
    updateSessionState();
}

void MediaSessionManager::removeSession(MediaSession& session)
{
    size_t index = m_sessions.find(&session);
    ASSERT(index != notFound);
    if (index == notFound)
        return;

    m_sessions.remove(index);
    updateSessionState();
}

#if !PLATFORM(MAC)
void MediaSessionManager::updateSessionState()
{
}
#endif

}
