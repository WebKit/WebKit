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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Dictionary.h"
#include "Event.h"
#include "HTMLMediaElement.h"
#include "MediaSessionManager.h"

namespace WebCore {

MediaSession::MediaSession(ScriptExecutionContext& context, const String& kind)
    : m_document(downcast<Document>(context))
    , m_kind(kind)
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

void MediaSession::setMetadata(const Dictionary& metadata)
{
    // 5.1.3
    // 1. Let media session be the current media session.
    // 2. Let baseURL be the API base URL specified by the entry settings object.
    // 3. Set media session's title to metadata's title.
    String title;
    metadata.get("title", title);

    // 4. Set media session's artist name to metadata's artist.
    String artist;
    metadata.get("artist", artist);

    // 5. Set media session's album name to metadata's album.
    String album;
    metadata.get("album", album);

    // 6. If metadata's artwork is present, parse it using baseURL, and if that does not return failure, set media
    //    session's artwork URL to the return value.
    URL artworkURL;
    String artworkPath;
    if (metadata.get("artwork", artworkPath))
        artworkURL = m_document.completeURL(artworkPath);

    m_metadata = MediaSessionMetadata(title, artist, album, artworkURL);

    if (Page *page = m_document.page())
        page->chrome().client().mediaSessionMetadataDidChange(m_metadata);
}

void MediaSession::releaseSession()
{
    // 5.1.3
    // 1. Let media session be the current media session.
    // 2. Indefinitely pause all of media session's active participating media elements.
    // 3. Reset media session's active participating media elements to an empty list.
    while (!m_activeParticipatingElements.isEmpty())
        m_activeParticipatingElements.takeAny()->pause();

    // 4. Run the media session release algorithm for media session.
    releaseInternal();
}

void MediaSession::releaseInternal()
{
    // 6.5. Releasing a media session
    // 1. If current media session's current state is idle, then terminate these steps.
    if (m_currentState == State::Idle)
        return;

    // 2. If current media session still has one or more active participating media elements, then terminate these steps.
    if (!m_activeParticipatingElements.isEmpty())
        return;

    // 3. Optionally, based on platform conventions, the user agent must release any currently held platform media focus
    //    for current media session.
    // 4. Optionally, based on platform conventions, the user agent must remove any previously established ongoing media
    //    interface in the underlying platform’s notifications area and any ongoing media interface in the underlying
    //    platform's lock screen area for current media session, if any.
    // 5. Optionally, based on platform conventions, the user agent must prevent any hardware and/or software media keys
    //    from controlling playback of current media session's active participating media elements.
    // 6. Set current media session's current state to idle.
    m_currentState = State::Idle;
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
    ASSERT(!m_iteratedActiveParticipatingElements);

    HashSet<HTMLMediaElement*> activeParticipatingElementsCopy = m_activeParticipatingElements;
    m_iteratedActiveParticipatingElements = &activeParticipatingElementsCopy;

    while (!activeParticipatingElementsCopy.isEmpty()) {
        HTMLMediaElement* element = activeParticipatingElementsCopy.takeAny();

        if (element->paused())
            element->play();
        else
            element->pause();
    }

    m_iteratedActiveParticipatingElements = nullptr;
}

void MediaSession::skipToNextTrack()
{
    if (m_controls && m_controls->nextTrackEnabled())
        m_controls->dispatchEvent(Event::create(eventNames().nexttrackEvent, false, false));
}

void MediaSession::skipToPreviousTrack()
{
    if (m_controls && m_controls->previousTrackEnabled())
        m_controls->dispatchEvent(Event::create(eventNames().previoustrackEvent, false, false));
}

}

#endif /* ENABLE(MEDIA_SESSION) */
