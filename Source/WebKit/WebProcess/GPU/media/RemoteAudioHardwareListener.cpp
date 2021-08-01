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
#include "RemoteAudioHardwareListener.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessProxy.h"
#include "RemoteAudioHardwareListenerMessages.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

Ref<RemoteAudioHardwareListener> RemoteAudioHardwareListener::create(AudioHardwareListener::Client& client, WebProcess& webProcess)
{
    return adoptRef(*new RemoteAudioHardwareListener(client, webProcess));
}

RemoteAudioHardwareListener::RemoteAudioHardwareListener(AudioHardwareListener::Client& client, WebProcess& webProcess)
    : AudioHardwareListener(client)
    , m_identifier(RemoteAudioHardwareListenerIdentifier::generate())
    , m_gpuProcessConnection(makeWeakPtr(WebProcess::singleton().ensureGPUProcessConnection()))
{
    m_gpuProcessConnection->addClient(*this);
    m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteAudioHardwareListener::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::CreateAudioHardwareListener(m_identifier), { });
}

RemoteAudioHardwareListener::~RemoteAudioHardwareListener()
{
    if (m_gpuProcessConnection) {
        m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);
        m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseAudioHardwareListener(m_identifier), 0);
    }
}

void RemoteAudioHardwareListener::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    audioHardwareDidBecomeInactive();

    ASSERT_UNUSED(connection, &connection == m_gpuProcessConnection);
    if (m_gpuProcessConnection) {
        m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);
        m_gpuProcessConnection = nullptr;
    }
}

void RemoteAudioHardwareListener::audioHardwareDidBecomeActive()
{
    setHardwareActivity(AudioHardwareActivityType::IsActive);
    m_client.audioHardwareDidBecomeActive();
}

void RemoteAudioHardwareListener::audioHardwareDidBecomeInactive()
{
    setHardwareActivity(AudioHardwareActivityType::IsInactive);
    m_client.audioHardwareDidBecomeInactive();
}

void RemoteAudioHardwareListener::audioOutputDeviceChanged(size_t bufferSizeMinimum, size_t bufferSizeMaximum)
{
    setSupportedBufferSizes({ bufferSizeMinimum, bufferSizeMaximum });
    m_client.audioOutputDeviceChanged();
}

}

#endif
