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
#include "RemoteMediaSessionCoordinatorProxy.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "MediaSessionCoordinatorPrivateProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ExceptionData.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

RefPtr<RemoteMediaSessionCoordinatorProxy> RemoteMediaSessionCoordinatorProxy::create(WebPageProxy& webPageProxy, Ref<MediaSessionCoordinatorPrivateProxy>&& privateCoordinator)
{
    return adoptRef(*new RemoteMediaSessionCoordinatorProxy(webPageProxy, WTFMove(privateCoordinator)));
}

RemoteMediaSessionCoordinatorProxy::RemoteMediaSessionCoordinatorProxy(WebPageProxy& webPageProxy, Ref<MediaSessionCoordinatorPrivateProxy>&& privateCoordinator)
    : m_webPageProxy(webPageProxy)
    , m_privateCoordinator(WTFMove(privateCoordinator))
    , m_logger(m_webPageProxy.logger())
    , m_logIdentifier(uniqueLogIdentifier())
{
    m_webPageProxy.process().addMessageReceiver(Messages::RemoteMediaSessionCoordinatorProxy::messageReceiverName(), m_webPageProxy.webPageID(), *this);
}

RemoteMediaSessionCoordinatorProxy::~RemoteMediaSessionCoordinatorProxy()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::RemoteMediaSessionCoordinatorProxy::messageReceiverName(), m_webPageProxy.webPageID());
}


void RemoteMediaSessionCoordinatorProxy::coordinateSeekTo(double time, CompletionHandler<void(const WebCore::ExceptionData&)>&& completionHandler)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, time);

    m_privateCoordinator->seekTo(time, [identifier] (const WebCore::ExceptionData&& exception) mutable {
        ALWAYS_LOG(identifier, "completion");
        completionHandler(WTFMove(exception));
    });
}

void RemoteMediaSessionCoordinatorProxy::coordinatePlay(CompletionHandler<void(const WebCore::ExceptionData&)>&& completionHandler)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_privateCoordinator->play([identifier] (const WebCore::ExceptionData&& exception) mutable {
        ALWAYS_LOG(identifier, "completion");
        completionHandler(WTFMove(exception));
    });
}

void RemoteMediaSessionCoordinatorProxy::coordinatePause(CompletionHandler<void(const WebCore::ExceptionData&)>&& completionHandler)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_privateCoordinator->pause([identifier] (const WebCore::ExceptionData&& exception) mutable {
        ALWAYS_LOG(identifier, "completion");
        completionHandler(WTFMove(exception));
    });
}

void RemoteMediaSessionCoordinatorProxy::::coordinateSetTrack(const String& track, CompletionHandler<void(const WebCore::ExceptionData&)>&& completionHandler)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    m_privateCoordinator->setTrack(track, [identifier] (const WebCore::ExceptionData&& exception) mutable {
        ALWAYS_LOG(identifier, "completion");
        completionHandler(WTFMove(exception));
    });
}

void RemoteMediaSessionCoordinatorProxy::positionStateChanged(Optional<WebCore::MediaPositionState> state)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_privateCoordinator->positionStateChanged(state);
}

void RemoteMediaSessionCoordinatorProxy::playbackStateChanged(MediaSessionPlaybackState state)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_privateCoordinator->playbackStateChanged(state);
}

void RemoteMediaSessionCoordinatorProxy::readyStateChanged(MediaSessionReadyState state)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_privateCoordinator->readyStateChanged(state);
}

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
