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
#include "RemoteRemoteCommandListener.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessProxy.h"
#include "RemoteRemoteCommandListenerMessages.h"
#include "RemoteRemoteCommandListenerProxyMessages.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRemoteCommandListener> RemoteRemoteCommandListener::create(RemoteCommandListenerClient& client, WebProcess& webProcess)
{
    return makeUnique<RemoteRemoteCommandListener>(client, webProcess);
}

RemoteRemoteCommandListener::RemoteRemoteCommandListener(RemoteCommandListenerClient& client, WebProcess& webProcess)
    : RemoteCommandListener(client)
    , m_process(webProcess)
    , m_identifier(RemoteRemoteCommandListenerIdentifier::generate())
{
    ensureGPUProcessConnection();
}

RemoteRemoteCommandListener::~RemoteRemoteCommandListener()
{
    if (m_gpuProcessConnection) {
        m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);
        m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseRemoteCommandListener(m_identifier), 0);
    }
}

GPUProcessConnection& RemoteRemoteCommandListener::ensureGPUProcessConnection()
{
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = m_process.ensureGPUProcessConnection();
        m_gpuProcessConnection->addClient(*this);
        m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteRemoteCommandListener::messageReceiverName(), m_identifier.toUInt64(), *this);
        m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::CreateRemoteCommandListener(m_identifier), { });
    }
    return *m_gpuProcessConnection;
}

void RemoteRemoteCommandListener::gpuProcessConnectionDidClose(GPUProcessConnection& gpuProcessConnection)
{
    gpuProcessConnection.removeClient(*this);
    gpuProcessConnection.messageReceiverMap().removeMessageReceiver(*this);
    m_gpuProcessConnection = nullptr;

    // FIXME: GPUProcess will be relaunched/RemoteCommandListener re-created when calling updateSupportedCommands().
    // FIXME: Should we relaunch the GPUProcess pro-actively and re-create the RemoteCommandListener?
}

void RemoteRemoteCommandListener::didReceiveRemoteControlCommand(WebCore::PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    client().didReceiveRemoteControlCommand(type, argument);
}

void RemoteRemoteCommandListener::updateSupportedCommands()
{
    auto& supportedCommands = this->supportedCommands();
    if (m_currentCommands == supportedCommands && m_currentSupportSeeking == supportsSeeking())
        return;

    m_currentCommands = supportedCommands;
    m_currentSupportSeeking = supportsSeeking();

    ensureGPUProcessConnection().connection().send(Messages::RemoteRemoteCommandListenerProxy::UpdateSupportedCommands { copyToVector(supportedCommands), m_currentSupportSeeking }, m_identifier);
}

}

#endif
