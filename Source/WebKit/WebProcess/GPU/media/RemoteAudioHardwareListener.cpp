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
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioHardwareListener);

Ref<RemoteAudioHardwareListener> RemoteAudioHardwareListener::create(AudioHardwareListener::Client& client)
{
    return adoptRef(*new RemoteAudioHardwareListener(client));
}

RemoteAudioHardwareListener::RemoteAudioHardwareListener(AudioHardwareListener::Client& client)
    : AudioHardwareListener(client)
    , m_gpuProcessConnection(WebProcess::singleton().ensureGPUProcessConnection())
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    gpuProcessConnection->addClient(*this);
    gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteAudioHardwareListener::messageReceiverName(), identifier().toUInt64(), *this);
    gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::CreateAudioHardwareListener(identifier()), { });
}

RemoteAudioHardwareListener::~RemoteAudioHardwareListener()
{
    if (auto gpuProcessConnection = m_gpuProcessConnection.get()) {
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseAudioHardwareListener(identifier()), 0);
    }
}

void RemoteAudioHardwareListener::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    audioHardwareDidBecomeInactive();

    ASSERT_UNUSED(connection, &connection == m_gpuProcessConnection.get());
    if (auto gpuProcessConnection = m_gpuProcessConnection.get()) {
        gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);
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
