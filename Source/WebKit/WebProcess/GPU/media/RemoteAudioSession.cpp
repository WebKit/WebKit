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
#include "WebProcess.h"
#include <WebCore/PlatformMediaSessionManager.h>

namespace WebKit {

using namespace WebCore;

UniqueRef<RemoteAudioSession> RemoteAudioSession::create(WebProcess& process)
{
    return makeUniqueRef<RemoteAudioSession>(process);
}

RemoteAudioSession::RemoteAudioSession(WebProcess& process)
    : m_process(process)
{
}

RemoteAudioSession::~RemoteAudioSession()
{
    if (m_gpuProcessConnection)
        m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioSession::messageReceiverName());
}

void RemoteAudioSession::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection.get() == &connection);
    m_gpuProcessConnection = nullptr;
    connection.messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioSession::messageReceiverName());
    connection.removeClient(*this);
}

IPC::Connection& RemoteAudioSession::ensureConnection()
{
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = makeWeakPtr(m_process.ensureGPUProcessConnection());
        m_gpuProcessConnection->addClient(*this);
        m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteAudioSession::messageReceiverName(), *this);

        RemoteAudioSessionConfiguration configuration;
        ensureConnection().sendSync(Messages::GPUConnectionToWebProcess::EnsureAudioSession(), Messages::GPUConnectionToWebProcess::EnsureAudioSession::Reply(configuration), { });
        m_configuration = WTFMove(configuration);
    }
    return m_gpuProcessConnection->connection();
}

const RemoteAudioSessionConfiguration& RemoteAudioSession::configuration() const
{
    if (!m_configuration)
        const_cast<RemoteAudioSession*>(this)->ensureConnection();
    return *m_configuration;
}

RemoteAudioSessionConfiguration& RemoteAudioSession::configuration()
{
    if (!m_configuration)
        ensureConnection();
    return *m_configuration;
}

void RemoteAudioSession::setCategory(CategoryType type, RouteSharingPolicy policy)
{
#if PLATFORM(COCOA)
    if (type == m_category && policy == m_routeSharingPolicy)
        return;

    m_category = type;
    m_routeSharingPolicy = policy;

    ensureConnection().send(Messages::RemoteAudioSessionProxy::SetCategory(type, policy), { });
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(policy);
#endif
}

void RemoteAudioSession::setPreferredBufferSize(size_t size)
{
    configuration().preferredBufferSize = size;
    ensureConnection().send(Messages::RemoteAudioSessionProxy::SetPreferredBufferSize(size), { });
}

bool RemoteAudioSession::tryToSetActiveInternal(bool active)
{
    bool succeeded;
    ensureConnection().sendSync(Messages::RemoteAudioSessionProxy::TryToSetActive(active), Messages::RemoteAudioSessionProxy::TryToSetActive::Reply(succeeded), { });
    if (succeeded)
        configuration().isActive = active;
    return succeeded;
}

AudioSession::CategoryType RemoteAudioSession::category() const
{
#if PLATFORM(COCOA)
    return m_category;
#else
    return AudioSession::CategoryType::None;
#endif
}

}

#endif
