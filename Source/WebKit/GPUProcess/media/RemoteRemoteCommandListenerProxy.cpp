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
#include "RemoteRemoteCommandListenerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteRemoteCommandListenerMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRemoteCommandListenerProxy);

RemoteRemoteCommandListenerProxy::RemoteRemoteCommandListenerProxy(GPUConnectionToWebProcess& gpuConnection, RemoteRemoteCommandListenerIdentifier&& identifier)
    : m_gpuConnection(gpuConnection)
    , m_identifier(WTFMove(identifier))
{
}

RemoteRemoteCommandListenerProxy::~RemoteRemoteCommandListenerProxy() = default;

void RemoteRemoteCommandListenerProxy::updateSupportedCommands(Vector<WebCore::PlatformMediaSession::RemoteControlCommandType>&& registeredCommands, bool supportsSeeking)
{
    m_supportedCommands.clear();
    m_supportedCommands.add(registeredCommands.begin(), registeredCommands.end());
    m_supportsSeeking = supportsSeeking;

    if (auto connection = m_gpuConnection.get())
        connection->updateSupportedRemoteCommands();
}

}

#endif
