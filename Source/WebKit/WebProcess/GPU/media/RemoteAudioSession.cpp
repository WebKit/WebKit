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
#include "RemoteAudioSession.h"

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessProxy.h"
#include "RemoteAudioSessionMessages.h"
#include "RemoteAudioSessionProxyMessages.h"
#include <WebCore/PlatformMediaSessionManager.h>

namespace WebKit {

using namespace WebCore;

UniqueRef<RemoteAudioSession> RemoteAudioSession::create(WebProcess& process)
{
    RemoteAudioSessionConfiguration configuration;
    process.ensureGPUProcessConnection().connection().sendSync(Messages::GPUConnectionToWebProcess::EnsureAudioSession(), Messages::GPUConnectionToWebProcess::EnsureAudioSession::Reply(configuration), { });
    return makeUniqueRef<RemoteAudioSession>(process, WTFMove(configuration));
}

RemoteAudioSession::RemoteAudioSession(WebProcess& process, RemoteAudioSessionConfiguration&& configuration)
    : m_process(process)
    , m_configuration(WTFMove(configuration))
{
    m_process.ensureGPUProcessConnection().messageReceiverMap().addMessageReceiver(Messages::RemoteAudioSession::messageReceiverName(), *this);
}

RemoteAudioSession::~RemoteAudioSession()
{
    if (auto* connection = m_process.existingGPUProcessConnection())
        connection->messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioSession::messageReceiverName());
}

IPC::Connection& RemoteAudioSession::connection()
{
    return m_process.ensureGPUProcessConnection().connection();
}

void RemoteAudioSession::setCategory(CategoryType type, RouteSharingPolicy policy)
{
    connection().send(Messages::RemoteAudioSessionProxy::SetCategory(type, policy), { });
}

void RemoteAudioSession::setPreferredBufferSize(size_t size)
{
    connection().send(Messages::RemoteAudioSessionProxy::SetPreferredBufferSize(size), { });
}

bool RemoteAudioSession::tryToSetActiveInternal(bool active)
{
    bool succeeded;
    connection().sendSync(Messages::RemoteAudioSessionProxy::TryToSetActive(active), Messages::RemoteAudioSessionProxy::TryToSetActive::Reply(succeeded), { });
    return succeeded;
}

void RemoteAudioSession::configurationChanged(RemoteAudioSessionConfiguration&& configuration)
{
    bool mutedStateChanged = configuration.isMuted != m_configuration.isMuted;

    m_configuration = configuration;

    if (mutedStateChanged)
        handleMutedStateChange();
}

}

#endif
