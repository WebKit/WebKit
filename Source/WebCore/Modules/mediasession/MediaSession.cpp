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
#include "MediaSession.h"

#if ENABLE(MEDIA_SESSION)

#include "HTMLMediaElement.h"
#include "MediaSessionManager.h"

namespace WebCore {

MediaSession::MediaSession(ScriptExecutionContext& context, const String& kind)
    : m_kind(kind)
{
    if (m_kind == "content")
        m_controls = adoptRef(*new MediaRemoteControls(context));

    MediaSessionManager::singleton().addMediaSession(*this);
}

MediaSession::~MediaSession()
{
    MediaSessionManager::singleton().removeMediaSession(*this);
}

MediaRemoteControls* MediaSession::controls(bool& isNull)
{
    MediaRemoteControls* controls = m_controls.get();
    isNull = !controls;
    return controls;
}

void MediaSession::addMediaElement(HTMLMediaElement& element)
{
    ASSERT(!m_participatingElements.contains(&element));
    m_participatingElements.append(&element);
}

void MediaSession::removeMediaElement(HTMLMediaElement& element)
{
    ASSERT(m_participatingElements.contains(&element));
    m_participatingElements.remove(m_participatingElements.find(&element));
}

void MediaSession::addActiveMediaElement(HTMLMediaElement& element)
{
    m_activeParticipatingElements.add(&element);
}

void MediaSession::releaseSession()
{
}

bool MediaSession::invoke()
{
    // 4.4 Activating a media session
    // 1. If we're already ACTIVE then return success.
    if (m_currentState == State::Active)
        return true;

    // 2. Optionally, based on platform conventions, request the most appropriate platform-level media focus for media
    //    session based on its current media session type.

    // 3. Run these substeps...

    // 4. Set our current state to ACTIVE and return success.
    m_currentState = State::Active;
    return true;
}

void MediaSession::togglePlayback()
{
    HashSet<HTMLMediaElement*> activeParticipatingElementsCopy = m_activeParticipatingElements;

    for (auto* element : activeParticipatingElementsCopy) {
        if (element->paused())
            element->play();
        else
            element->pause();
    }
}

}

#endif /* ENABLE(MEDIA_SESSION) */
