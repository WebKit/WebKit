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
#include "MediaSessionInterruptionProviderMac.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const char* contentSessionKind = "content";

MediaSessionManager& MediaSessionManager::singleton()
{
    static NeverDestroyed<MediaSessionManager> manager;
    return manager;
}

MediaSessionManager::MediaSessionManager()
{
#if PLATFORM(MAC)
    m_interruptionProvider = adoptRef(new MediaSessionInterruptionProviderMac(*this));
    m_interruptionProvider->beginListeningForInterruptions();
#endif
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

void MediaSessionManager::didReceiveStartOfInterruptionNotification(MediaSessionInterruptingCategory interruptingCategory)
{
    // 4.5.2 Interrupting a media session
    // When a start-of-interruption notification event is received from the platform, then the user agent must run the
    // media session interruption algorithm against all known media sessions, passing in each media session as media session.
    for (auto* session : m_sessions) {
        // 1. If media session's current state is not active, then terminate these steps.
        if (session->currentState() != MediaSession::State::Active)
            continue;

        // 2. Let interrupting media session category be the media session category that triggered this interruption.
        //    If this interruption has no known media session category, let interrupting media session category be Default.

        // 3. Run these substeps:
        if (interruptingCategory == MediaSessionInterruptingCategory::Content) {
            // -  If interrupting media session category is Content:
            //    If media session's current media session type is Default or Content then indefinitely pause all of media
            //    session's active audio-producing participants and set media session's current state to idle.
            if (session->kind() == MediaSessionKind::Default || session->kind() == MediaSessionKind::Content)
                session->handleIndefinitePauseInterruption();
        } else if (interruptingCategory == MediaSessionInterruptingCategory::Transient) {
            // - If interrupting media session category is Transient:
            //   If media session's current media session type is Default or Content then duck all of media session's active
            //   audio-producing participants and set media session's current state to interrupted.
            if (session->kind() == MediaSessionKind::Default || session->kind() == MediaSessionKind::Content)
                session->handleDuckInterruption();
        } else if (interruptingCategory == MediaSessionInterruptingCategory::TransientSolo) {
            // - If interrupting media session category is Transient Solo:
            //   If media session's current media session type is Default, Content, Transient or Transient Solo then pause
            //   all of media session's active audio-producing participants and set current media session's current state to interrupted.
            if (session->kind() != MediaSessionKind::Ambient)
                session->handlePauseInterruption();
        }
    }
}

void MediaSessionManager::didReceiveEndOfInterruptionNotification(MediaSessionInterruptingCategory interruptingCategory)
{
    // 4.5.2 Interrupting a media session
    // When an end-of-interruption notification event is received from the platform, then the user agent must run the
    // media session continuation algorithm against all known media sessions, passing in each media session as media session.
    for (auto* session : m_sessions) {
        // 1. If media session's current state is not interrupted, then terminate these steps.
        if (session->currentState() != MediaSession::State::Interrupted)
            continue;

        // 2. Let interrupting media session category be the media session category that initially triggered this interruption.
        //    If this interruption has no known media session category, let interrupting media session category be Default.

        // 3. Run these substeps:
        if (interruptingCategory == MediaSessionInterruptingCategory::Transient) {
            // - If interrupting media session category is Transient:
            //   If media session's current media session type is Default or Content, then unduck all of media session's
            //   active audio-producing participants and set media session's current state to active.
            if (session->kind() == MediaSessionKind::Default || session->kind() == MediaSessionKind::Content)
                session->handleUnduckInterruption();
        } else if (interruptingCategory == MediaSessionInterruptingCategory::TransientSolo) {
            // - If interrupting media session category is Transient Solo:
            //   If media session's current media session type is Default, Content, Transient, or Transient Solo, then
            //   unpause the media session's active audio-producing participants and set media session's current state to active.
            if (session->kind() != MediaSessionKind::Ambient)
                session->handleUnpauseInterruption();
        }
    }
}

} // namespace WebCore

#endif /* ENABLE(MEDIA_SESSION) */
