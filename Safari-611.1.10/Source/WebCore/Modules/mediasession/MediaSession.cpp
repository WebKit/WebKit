/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SESSION)

#include "MediaMetadata.h"
#include "Navigator.h"

namespace WebCore {

Ref<MediaSession> MediaSession::create(Navigator& navigator)
{
    return adoptRef(*new MediaSession(navigator));
}

MediaSession::MediaSession(Navigator& navigator)
    : m_navigator(makeWeakPtr(navigator))
{
}

MediaSession::~MediaSession() = default;

void MediaSession::setMetadata(RefPtr<MediaMetadata>&& metadata)
{
    if (m_metadata)
        m_metadata->resetMediaSession();
    m_metadata = WTFMove(metadata);
    if (m_metadata)
        m_metadata->setMediaSession(*this);
    metadataUpdated();
}

void MediaSession::setPlaybackState(MediaSessionPlaybackState state)
{
    if (m_playbackState == state)
        return;

    auto currentPosition = this->currentPosition();
    if (m_positionState && currentPosition) {
        m_positionState->position = *currentPosition;
        m_timeAtLastPositionUpdate = MonotonicTime::now();
    }
    m_playbackState = state;
}

void MediaSession::setActionHandler(MediaSessionAction action, RefPtr<MediaSessionActionHandler>&& handler)
{
    if (handler)
        m_actionHandlers.set(action, handler);
    else
        m_actionHandlers.remove(action);
    actionHandlersUpdated();
}

bool MediaSession::hasActionHandler(MediaSessionAction action) const
{
    return m_actionHandlers.contains(action);
}

RefPtr<MediaSessionActionHandler> MediaSession::handlerForAction(MediaSessionAction action) const
{
    return m_actionHandlers.get(action);
}

ExceptionOr<void> MediaSession::setPositionState(Optional<MediaPositionState>&& state)
{
    if (!state) {
        m_positionState = WTF::nullopt;
        return { };
    }

    if (!(state->duration >= 0
        && state->position >= 0
        && state->position <= state->duration
        && std::isfinite(state->playbackRate)
        && state->playbackRate))
        return Exception { TypeError };

    m_positionState = WTFMove(state);
    m_lastReportedPosition = m_positionState->position;
    m_timeAtLastPositionUpdate = MonotonicTime::now();
    return { };
}

Optional<double> MediaSession::currentPosition() const
{
    if (!m_positionState || !m_lastReportedPosition)
        return WTF::nullopt;

    auto actualPlaybackRate = m_playbackState == MediaSessionPlaybackState::Playing ? m_positionState->playbackRate : 0;

    auto elapsedTime = (MonotonicTime::now() - m_timeAtLastPositionUpdate) * actualPlaybackRate;

    return std::max(0., std::min(*m_lastReportedPosition + elapsedTime.value(), m_positionState->duration));
}

void MediaSession::metadataUpdated()
{
}

void MediaSession::actionHandlersUpdated()
{
}

}

#endif // ENABLE(MEDIA_SESSION)
