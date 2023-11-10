/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "MockMediaSessionCoordinator.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "StringCallback.h"
#include <wtf/CompletionHandler.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

Ref<MockMediaSessionCoordinator> MockMediaSessionCoordinator::create(ScriptExecutionContext& context, RefPtr<StringCallback>&& listener)
{
    return adoptRef(*new MockMediaSessionCoordinator(context, WTFMove(listener)));
}

MockMediaSessionCoordinator::MockMediaSessionCoordinator(ScriptExecutionContext& context, RefPtr<StringCallback>&& listener)
    : m_context(context)
    , m_stateChangeListener(WTFMove(listener))
{
}

std::optional<Exception> MockMediaSessionCoordinator::result() const
{
    if (m_failCommands)
        return Exception { ExceptionCode::InvalidStateError };

    return std::nullopt;
}

void MockMediaSessionCoordinator::join(CompletionHandler<void(std::optional<Exception>&&)>&& callback)
{
    m_context->postTask([this, callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        callback(result());
    });
}

void MockMediaSessionCoordinator::leave()
{
}

void MockMediaSessionCoordinator::seekTo(double time, CompletionHandler<void(std::optional<Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, time);
    m_context->postTask([this, callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        callback(result());
    });
}

void MockMediaSessionCoordinator::play(CompletionHandler<void(std::optional<Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_context->postTask([this, callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        callback(result());
    });
}

void MockMediaSessionCoordinator::pause(CompletionHandler<void(std::optional<Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_context->postTask([this, callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        callback(result());
    });
}

void MockMediaSessionCoordinator::setTrack(const String&, CompletionHandler<void(std::optional<Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_context->postTask([this, callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        callback(result());
    });
}

void MockMediaSessionCoordinator::positionStateChanged(const std::optional<MediaPositionState>&)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_stateChangeListener->scheduleCallback(m_context.get(), "positionStateChanged"_s);
}

void MockMediaSessionCoordinator::readyStateChanged(MediaSessionReadyState state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, state);
    m_stateChangeListener->scheduleCallback(m_context.get(), "readyStateChanged"_s);
}

void MockMediaSessionCoordinator::playbackStateChanged(MediaSessionPlaybackState state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, state);
    m_stateChangeListener->scheduleCallback(m_context.get(), "playbackStateChanged"_s);
}

void MockMediaSessionCoordinator::trackIdentifierChanged(const String& identifier)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, identifier);
    m_stateChangeListener->scheduleCallback(m_context.get(), "trackIdentifierChanged"_s);
}

WTFLogChannel& MockMediaSessionCoordinator::logChannel() const
{
    return LogMedia;
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
