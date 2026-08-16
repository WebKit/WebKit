/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteMediaSessionProxy.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "MessageSenderInlines.h"
#include "RemoteMediaSessionClientProxy.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionProxy);

Ref<RemoteMediaSessionProxy> RemoteMediaSessionProxy::create(RemoteMediaSessionState& state, WebProcessProxy& process)
{
    Ref client = RemoteMediaSessionClientProxy::create(state, process);
    Ref session = adoptRef(*new RemoteMediaSessionProxy(client.copyRef(), state, process));
    client->attachToSession(session);
    return session;
}

RemoteMediaSessionProxy::RemoteMediaSessionProxy(Ref<RemoteMediaSessionClientProxy>&& client, const RemoteMediaSessionState& state, WebProcessProxy& process)
    : PlatformMediaSession(client)
    , m_client(WTF::move(client))
    , m_process(process)
    , m_sessionState(state)
#if !RELEASE_LOG_DISABLED
    , m_logger(process.logger())
#endif
{
    setMediaSessionIdentifier(state.sessionIdentifier);
}

RemoteMediaSessionProxy::~RemoteMediaSessionProxy()
{
    invalidateClient();
}

void RemoteMediaSessionProxy::updateState(const RemoteMediaSessionState& remoteState)
{
    m_sessionState = remoteState;
    downcast<RemoteMediaSessionClientProxy>(protect(client())).updateState(remoteState);
}

void RemoteMediaSessionProxy::setState(WebCore::PlatformMediaSessionState state)
{
    PlatformMediaSession::setState(state);
    m_sessionState.state = state;
}

WeakPtr<WebCore::PlatformMediaSessionInterface> RemoteMediaSessionProxy::selectBestMediaSession(const Vector<WeakPtr<WebCore::PlatformMediaSessionInterface>>&, WebCore::PlatformMediaSessionPlaybackControlsPurpose)
{
    // FIXME: Another synchronous API we need to fix.
    return nullptr;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void RemoteMediaSessionProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    send(Messages::RemoteMediaSessionManager::ClientSetShouldPlayToPlaybackTarget(sessionIdentifier(), shouldPlay));
}
#endif

IPC::Connection* RemoteMediaSessionProxy::messageSenderConnection() const
{
    return m_process && m_process->hasConnection() ? &m_process->connection() : nullptr;
}

uint64_t RemoteMediaSessionProxy::messageSenderDestinationID() const
{
    return pageIdentifier().toUInt64();
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
