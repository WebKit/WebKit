/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "MediaSession.h"

#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaSessionManager.h"

namespace WebCore {

#if !LOG_DISABLED
static const char* stateName(MediaSession::State state)
{
#define CASE(state) case MediaSession::state: return #state
    switch (state) {
    CASE(Idle);
    CASE(Playing);
    CASE(Paused);
    CASE(Interrupted);
    }

    ASSERT_NOT_REACHED();
    return "";
}
#endif

std::unique_ptr<MediaSession> MediaSession::create(MediaSessionClient& client)
{
    return std::make_unique<MediaSession>(client);
}

MediaSession::MediaSession(MediaSessionClient& client)
    : m_client(client)
    , m_state(Idle)
    , m_stateToRestore(Idle)
    , m_notifyingClient(false)
{
    ASSERT(m_client.mediaType() >= None && m_client.mediaType() <= WebAudio);
    MediaSessionManager::sharedManager().addSession(*this);
}

MediaSession::~MediaSession()
{
    MediaSessionManager::sharedManager().removeSession(*this);
}

void MediaSession::setState(State state)
{
    LOG(Media, "MediaSession::setState - %s", stateName(state));
    m_state = state;
}

void MediaSession::beginInterruption()
{
    LOG(Media, "MediaSession::beginInterruption");

    m_stateToRestore = state();
    m_notifyingClient = true;
    client().pausePlayback();
    setState(Interrupted);
    m_notifyingClient = false;
}

void MediaSession::endInterruption(EndInterruptionFlags flags)
{
    LOG(Media, "MediaSession::endInterruption - flags = %i, stateToRestore = %s", (int)flags, stateName(m_stateToRestore));

    State stateToRestore = m_stateToRestore;
    m_stateToRestore = Idle;
    setState(Paused);

    if (flags & MayResumePlaying && stateToRestore == Playing) {
        LOG(Media, "MediaSession::endInterruption - resuming playback");
        client().resumePlayback();
    }
}

bool MediaSession::clientWillBeginPlayback()
{
    setState(Playing);
    MediaSessionManager::sharedManager().sessionWillBeginPlayback(*this);
    return true;
}

bool MediaSession::clientWillPausePlayback()
{
    if (state() == Interrupted) {
        if (!m_notifyingClient)
            m_stateToRestore = Paused;
        return false;
    }
    
    setState(Paused);
    return true;
}

void MediaSession::pauseSession()
{
    LOG(Media, "MediaSession::pauseSession");
    m_client.pausePlayback();
}

MediaSession::MediaType MediaSession::mediaType() const
{
    return m_client.mediaType();
}
    
}
