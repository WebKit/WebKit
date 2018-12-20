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
#include "Event.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "MediaSessionManager.h"
#include "Page.h"

namespace WebCore {

MediaSession::MediaSession(Document& document, Kind kind)
    : m_document(document)
    , m_kind(kind)
{
    // 4. Media Sessions
    // 3. If media session's current media session type is "content", then create a new media remote controller for media
    //    session. (Otherwise media session has no media remote controller.)
    if (m_kind == Kind::Content)
        m_controls = MediaRemoteControls::create(document, this);

    MediaSessionManager::singleton().addMediaSession(*this);
}

MediaSession::~MediaSession()
{
    MediaSessionManager::singleton().removeMediaSession(*this);

    if (m_controls)
        m_controls->clearSession();
}

MediaRemoteControls* MediaSession::controls()
{
    return m_controls.get();
}

void MediaSession::addMediaElement(HTMLMediaElement& element)
{
    ASSERT(!m_participatingElements.contains(&element));
    m_participatingElements.add(&element);
}

void MediaSession::removeMediaElement(HTMLMediaElement& element)
{
    ASSERT(m_participatingElements.contains(&element));
    m_participatingElements.remove(&element);

    changeActiveMediaElements([&]() {
        m_activeParticipatingElements.remove(&element);
    });

    if (m_iteratedActiveParticipatingElements)
        m_iteratedActiveParticipatingElements->remove(&element);
}

void MediaSession::changeActiveMediaElements(const WTF::Function<void(void)>& worker)
{
    if (Page *page = m_document.page()) {
        bool hadActiveMediaElements = MediaSessionManager::singleton().hasActiveMediaElements();

        worker();

        bool hasActiveMediaElements = MediaSessionManager::singleton().hasActiveMediaElements();
        if (hadActiveMediaElements != hasActiveMediaElements)
            page->chrome().client().hasMediaSessionWithActiveMediaElementsDidChange(hasActiveMediaElements);
    } else
        worker();
}

void MediaSession::addActiveMediaElement(HTMLMediaElement& element)
{
    changeActiveMediaElements([&]() {
        m_activeParticipatingElements.add(&element);
    });
}

bool MediaSession::isMediaElementActive(HTMLMediaElement& element)
{
    return m_activeParticipatingElements.contains(&element);
}

bool MediaSession::hasActiveMediaElements() const
{
    return !m_activeParticipatingElements.isEmpty();
}

void MediaSession::setMetadata(const Optional<Metadata>& optionalMetadata)
{
    if (!optionalMetadata)
        m_metadata = { };
    else {
        auto& metadata = optionalMetadata.value();
        m_metadata = { metadata.title, metadata.artist, metadata.album, m_document.completeURL(metadata.artwork) };
    }

    if (auto* page = m_document.page())
        page->chrome().client().mediaSessionMetadataDidChange(m_metadata);
}

void MediaSession::deactivate()
{
    // 5.1.2. Object members
    // When the deactivate() method is invoked, the user agent must run the following steps:
    // 1. Let media session be the current media session.
    // 2. Indefinitely pause all of media session’s audio-producing participants.
    // 3. Set media session's resume list to an empty list.
    // 4. Set media session's audio-producing participants to an empty list.
    changeActiveMediaElements([&]() {
        while (!m_activeParticipatingElements.isEmpty())
            m_activeParticipatingElements.takeAny()->pause();
    });

    // 5. Run the media session deactivation algorithm for media session.
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

void MediaSession::handleDuckInterruption()
{
    for (auto* element : m_activeParticipatingElements)
        element->setShouldDuck(true);

    m_currentState = State::Interrupted;
}

void MediaSession::handleUnduckInterruption()
{
    for (auto* element : m_activeParticipatingElements)
        element->setShouldDuck(false);

    m_currentState = State::Active;
}

void MediaSession::handleIndefinitePauseInterruption()
{
    safelyIterateActiveMediaElements([](HTMLMediaElement* element) {
        element->pause();
    });

    m_activeParticipatingElements.clear();
    m_currentState = State::Idle;
}

void MediaSession::handlePauseInterruption()
{
    m_currentState = State::Interrupted;

    safelyIterateActiveMediaElements([](HTMLMediaElement* element) {
        element->pause();
    });
}

void MediaSession::handleUnpauseInterruption()
{
    m_currentState = State::Active;

    safelyIterateActiveMediaElements([](HTMLMediaElement* element) {
        element->play();
    });
}

void MediaSession::togglePlayback()
{
    safelyIterateActiveMediaElements([](HTMLMediaElement* element) {
        if (element->paused())
            element->play();
        else
            element->pause();
    });
}

void MediaSession::safelyIterateActiveMediaElements(const WTF::Function<void(HTMLMediaElement*)>& handler)
{
    ASSERT(!m_iteratedActiveParticipatingElements);

    HashSet<HTMLMediaElement*> activeParticipatingElementsCopy = m_activeParticipatingElements;
    m_iteratedActiveParticipatingElements = &activeParticipatingElementsCopy;

    while (!activeParticipatingElementsCopy.isEmpty())
        handler(activeParticipatingElementsCopy.takeAny());

    m_iteratedActiveParticipatingElements = nullptr;
}

void MediaSession::skipToNextTrack()
{
    if (m_controls && m_controls->nextTrackEnabled())
        m_controls->dispatchEvent(Event::create(eventNames().nexttrackEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void MediaSession::skipToPreviousTrack()
{
    if (m_controls && m_controls->previousTrackEnabled())
        m_controls->dispatchEvent(Event::create(eventNames().previoustrackEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void MediaSession::controlIsEnabledDidChange()
{
    // Media remote controls are only allowed on Content media sessions.
    ASSERT(m_kind == Kind::Content);

    // Media elements belonging to Content media sessions have mutually-exclusive playback.
    ASSERT(m_activeParticipatingElements.size() <= 1);

    if (m_activeParticipatingElements.isEmpty())
        return;

    HTMLMediaElement* element = *m_activeParticipatingElements.begin();
    m_document.updateIsPlayingMedia(element->elementID());
}

}

#endif /* ENABLE(MEDIA_SESSION) */
