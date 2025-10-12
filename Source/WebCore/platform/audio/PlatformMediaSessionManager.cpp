/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include "PlatformMediaSessionManager.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "PlatformMediaSession.h"
#include <algorithm>
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

#if RELEASE_LOG_DISABLED
#define PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(formatString, ...)
#else
#define PLATFORMMEDIASESSIONMANAGER_RELEASE_LOG(formatString, ...) \
do { \
    if (willLog(WTFLogLevel::Always)) { \
        RELEASE_LOG_FORWARDABLE(Media, PLATFORMMEDIASESSIONMANAGER_##formatString, ##__VA_ARGS__); \
        if (logger().developerExtrasEnabled()) { \
            char buffer[1024] = { 0 }; \
            SAFE_SPRINTF(std::span { buffer }, MESSAGE_PLATFORMMEDIASESSIONMANAGER_##formatString, ##__VA_ARGS__); \
            logger().toObservers(logChannel(), WTFLogLevel::Always, String::fromUTF8(buffer)); \
        } \
    } \
} while (0)
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformMediaSessionManager);

#if !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))
RefPtr<PlatformMediaSessionManager> PlatformMediaSessionManager::create(PageIdentifier pageIdentifier)
{
    return adoptRef(new PlatformMediaSessionManager(pageIdentifier));
}
#endif // !PLATFORM(COCOA) && (!USE(GLIB) || !ENABLE(MEDIA_SESSION))

PlatformMediaSessionManager::PlatformMediaSessionManager(PageIdentifier pageIdentifier)
    : MediaSessionManagerInterface(pageIdentifier)
{
}

void PlatformMediaSessionManager::addSession(PlatformMediaSessionInterface& session)
{
    m_sessions.appendOrMoveToLast(session);
    MediaSessionManagerInterface::addSession(session);
}

void PlatformMediaSessionManager::removeSession(PlatformMediaSessionInterface& session)
{
    m_sessions.removeNullReferences();
    if (!m_sessions.remove(session))
        return;

    MediaSessionManagerInterface::removeSession(session);
}

void PlatformMediaSessionManager::setCurrentSession(PlatformMediaSessionInterface& session)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), ", size = ", m_sessions.computeSize());

    m_sessions.removeNullReferences();
    m_sessions.prependOrMoveToFirst(session);
}

RefPtr<PlatformMediaSessionInterface> PlatformMediaSessionManager::currentSession() const
{
    if (!m_sessions.computeSize())
        return nullptr;

    return &m_sessions.first();
}

Vector<WeakPtr<PlatformMediaSessionInterface>> PlatformMediaSessionManager::copySessionsToVector() const
{
    m_sessions.removeNullReferences();
    return copyToVector(m_sessions);
}

void PlatformMediaSessionManager::forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

WeakPtr<PlatformMediaSessionInterface> PlatformMediaSessionManager::bestEligibleSessionForRemoteControls(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& filterFunction, PlatformMediaSession::PlaybackControlsPurpose purpose)
{
    Vector<WeakPtr<PlatformMediaSessionInterface>> eligibleAudioVideoSessions;
    Vector<WeakPtr<PlatformMediaSessionInterface>> eligibleWebAudioSessions;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession(filterFunction, [&](auto& session) {
        if (session.presentationType() == PlatformMediaSession::MediaType::WebAudio) {
            if (eligibleAudioVideoSessions.isEmpty())
                eligibleWebAudioSessions.append(session);
        } else
            eligibleAudioVideoSessions.append(session);
    });
#else
    UNUSED_PARAM(filterFunction);
#endif

    if (eligibleAudioVideoSessions.isEmpty()) {
        if (eligibleWebAudioSessions.isEmpty())
            return nullptr;
        return RefPtr { eligibleWebAudioSessions[0].get() }->selectBestMediaSession(eligibleWebAudioSessions, purpose);
    }

    return RefPtr { eligibleAudioVideoSessions[0].get() }->selectBestMediaSession(eligibleAudioVideoSessions, purpose);
}

} // namespace WebCore
