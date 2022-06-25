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
#include "RemoteMediaSessionHelper.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(IOS_FAMILY)

#include "Connection.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "RemoteMediaSessionHelperMessages.h"
#include "RemoteMediaSessionHelperProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/MediaPlaybackTargetCocoa.h>
#include <WebCore/MediaPlaybackTargetContext.h>
#include <WebCore/MediaPlaybackTargetMock.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaSessionHelper::RemoteMediaSessionHelper(WebProcess& process)
    : m_process(process)
{
}

IPC::Connection& RemoteMediaSessionHelper::ensureConnection()
{
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = m_process.ensureGPUProcessConnection();
        m_gpuProcessConnection->addClient(*this);
        m_gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSessionHelper::messageReceiverName(), *this);
        m_gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::EnsureMediaSessionHelper(), { });
    }
    return m_gpuProcessConnection->connection();
}

void RemoteMediaSessionHelper::gpuProcessConnectionDidClose(GPUProcessConnection& gpuProcessConnection)
{
    gpuProcessConnection.removeClient(*this);
    gpuProcessConnection.messageReceiverMap().removeMessageReceiver(*this);
    m_gpuProcessConnection = nullptr;
}

void RemoteMediaSessionHelper::startMonitoringWirelessRoutesInternal()
{
    ensureConnection().send(Messages::RemoteMediaSessionHelperProxy::StartMonitoringWirelessRoutes(), { });
}

void RemoteMediaSessionHelper::stopMonitoringWirelessRoutesInternal()
{
    ensureConnection().send(Messages::RemoteMediaSessionHelperProxy::StopMonitoringWirelessRoutes(), { });
}

void RemoteMediaSessionHelper::providePresentingApplicationPID(int pid)
{
    ensureConnection().send(Messages::RemoteMediaSessionHelperProxy::ProvidePresentingApplicationPID(pid), { });
}

void RemoteMediaSessionHelper::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, MediaPlaybackTargetContext&& targetContext)
{
    ASSERT(targetContext.type() != MediaPlaybackTargetContext::Type::AVOutputContext);
    if (targetContext.type() == MediaPlaybackTargetContext::Type::AVOutputContext)
        return;

    WebCore::MediaSessionHelper::activeVideoRouteDidChange(supportsAirPlayVideo, WebCore::MediaPlaybackTargetCocoa::create(WTFMove(targetContext)));
}

}

#endif

