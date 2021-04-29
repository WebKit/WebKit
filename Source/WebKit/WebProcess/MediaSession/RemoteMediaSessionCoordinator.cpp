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
#include "RemoteMediaSessionCoordinator.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "RemoteMediaSessionCoordinatorMessages.h"
#include "RemoteMediaSessionCoordinatorProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <wtf/CompletionHandler.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
extern WTFLogChannel LogMedia;
}

namespace WebKit {

using namespace PAL;
using namespace WebCore;

Ref<RemoteMediaSessionCoordinator> RemoteMediaSessionCoordinator::create(WebPage& page, const String& identifier)
{
    return adoptRef(*new RemoteMediaSessionCoordinator(page, identifier));
}

RemoteMediaSessionCoordinator::RemoteMediaSessionCoordinator(WebPage& page, const String& identifier)
    : m_page(page)
    , m_identifier(identifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteMediaSessionCoordinator::messageReceiverName(), m_page.identifier(), *this);
}

RemoteMediaSessionCoordinator::~RemoteMediaSessionCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(Messages::RemoteMediaSessionCoordinator::messageReceiverName(), m_page.identifier());
}

void RemoteMediaSessionCoordinator::join(CompletionHandler<void(Optional<WebCore::Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_page.sendWithAsyncReply(Messages::RemoteMediaSessionCoordinatorProxy::Join { }, [weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto&& exception) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            callback(exception->toException());
            return;
        }

        callback({ });
    });
}

void RemoteMediaSessionCoordinator::leave()
{
    m_page.send(Messages::RemoteMediaSessionCoordinatorProxy::Leave { });
}

void RemoteMediaSessionCoordinator::seekTo(double time, CompletionHandler<void(Optional<WebCore::Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, time);
    m_page.sendWithAsyncReply(Messages::RemoteMediaSessionCoordinatorProxy::CoordinateSeekTo { time }, [weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto&& exception) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            callback(exception->toException());
            return;
        }

        callback({ });
    });
}

void RemoteMediaSessionCoordinator::play(CompletionHandler<void(Optional<WebCore::Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_page.sendWithAsyncReply(Messages::RemoteMediaSessionCoordinatorProxy::CoordinatePlay { }, [weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto&& exception) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            callback(exception->toException());
            return;
        }

        callback({ });
    });
}

void RemoteMediaSessionCoordinator::pause(CompletionHandler<void(Optional<WebCore::Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_page.sendWithAsyncReply(Messages::RemoteMediaSessionCoordinatorProxy::CoordinatePause { }, [weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto&& exception) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            callback(exception->toException());
            return;
        }

        callback({ });
    });
}

void RemoteMediaSessionCoordinator::setTrack(const String& trackIdentifier, CompletionHandler<void(Optional<WebCore::Exception>&&)>&& callback)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_page.sendWithAsyncReply(Messages::RemoteMediaSessionCoordinatorProxy::CoordinateSetTrack { trackIdentifier }, [weakThis = makeWeakPtr(this), callback = WTFMove(callback)](auto&& exception) mutable {
        if (!weakThis) {
            callback(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            callback(exception->toException());
            return;
        }

        callback({ });
    });
}

void RemoteMediaSessionCoordinator::positionStateChanged(const Optional<WebCore::MediaPositionState>& state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_page.send(Messages::RemoteMediaSessionCoordinatorProxy::PositionStateChanged { state });
}

void RemoteMediaSessionCoordinator::readyStateChanged(WebCore::MediaSessionReadyState state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, state);
    m_page.send(Messages::RemoteMediaSessionCoordinatorProxy::ReadyStateChanged { state });
}

void RemoteMediaSessionCoordinator::playbackStateChanged(WebCore::MediaSessionPlaybackState state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, state);
    m_page.send(Messages::RemoteMediaSessionCoordinatorProxy::PlaybackStateChanged { state });
}

void RemoteMediaSessionCoordinator::coordinatorStateChanged(WebCore::MediaSessionCoordinatorState state)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, state);
    m_page.send(Messages::RemoteMediaSessionCoordinatorProxy::CoordinatorStateChanged { state });
}

void RemoteMediaSessionCoordinator::seekSessionToTime(double time, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, time);
    if (auto coordinatorClient = client())
        coordinatorClient->seekSessionToTime(time, WTFMove((completionHandler)));
    else
        completionHandler(false);
}

void RemoteMediaSessionCoordinator::playSession(Optional<double> atTime, Optional<double> hostTime, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (auto coordinatorClient = client())
        coordinatorClient->playSession(WTFMove(atTime), WTFMove(hostTime), WTFMove((completionHandler)));
    else
        completionHandler(false);
}

void RemoteMediaSessionCoordinator::pauseSession(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    if (auto coordinatorClient = client())
        coordinatorClient->pauseSession(WTFMove((completionHandler)));
    else
        completionHandler(false);
}

void RemoteMediaSessionCoordinator::setSessionTrack(const String& trackIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, trackIdentifier);
    if (auto coordinatorClient = client())
        coordinatorClient->setSessionTrack(trackIdentifier, WTFMove((completionHandler)));
    else
        completionHandler(false);
}

WTFLogChannel& RemoteMediaSessionCoordinator::logChannel() const
{
    return LogMedia;
}


}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
