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

Ref<RemoteRemoteCommandListener> RemoteRemoteCommandListener::create(RemoteCommandListenerClient& client)
{
    return adoptRef(*new RemoteRemoteCommandListener(client));
}

RemoteRemoteCommandListener::RemoteRemoteCommandListener(RemoteCommandListenerClient& client)
    : RemoteCommandListener(client)
{
    ensureGPUProcessConnection();
}

RemoteRemoteCommandListener::~RemoteRemoteCommandListener()
{
    if (RefPtr gpuProcessConnection = m_gpuProcessConnection.get()) {
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(Messages::RemoteRemoteCommandListener::messageReceiverName(), identifier().toUInt64());
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseRemoteCommandListener(identifier()), 0);
    }
}

GPUProcessConnection& RemoteRemoteCommandListener::ensureGPUProcessConnection()
{
    RefPtr gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteRemoteCommandListener::messageReceiverName(), identifier().toUInt64(), *this);
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::CreateRemoteCommandListener(identifier()), { });
    }
    return *gpuProcessConnection;
}

void RemoteRemoteCommandListener::gpuProcessConnectionDidClose(GPUProcessConnection& gpuProcessConnection)
{
    ASSERT(m_gpuProcessConnection.get() == &gpuProcessConnection);

    gpuProcessConnection.messageReceiverMap().removeMessageReceiver(Messages::RemoteRemoteCommandListener::messageReceiverName(), identifier().toUInt64());
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

    ensureGPUProcessConnection().connection().send(Messages::RemoteRemoteCommandListenerProxy::UpdateSupportedCommands { copyToVector(supportedCommands), m_currentSupportSeeking }, identifier());
}

}

#endif
