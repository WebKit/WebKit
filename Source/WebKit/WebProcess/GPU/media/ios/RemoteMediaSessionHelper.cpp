/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "MediaPlaybackTargetContextSerialized.h"
#include "RemoteMediaSessionHelperMessages.h"
#include "RemoteMediaSessionHelperProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/MediaPlaybackTargetCocoa.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaSessionHelper::RemoteMediaSessionHelper() = default;

IPC::Connection& RemoteMediaSessionHelper::ensureConnection()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        gpuProcessConnection->messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSessionHelper::messageReceiverName(), *this);
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::EnsureMediaSessionHelper(), { });
    }
    return gpuProcessConnection->connection();
}

void RemoteMediaSessionHelper::gpuProcessConnectionDidClose(GPUProcessConnection& gpuProcessConnection)
{
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

void RemoteMediaSessionHelper::providePresentingApplicationPID(int pid, ShouldOverride shouldOverride)
{
    ensureConnection().send(Messages::RemoteMediaSessionHelperProxy::ProvidePresentingApplicationPID(pid, shouldOverride), { });
}

void RemoteMediaSessionHelper::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, MediaPlaybackTargetContextSerialized&& targetContext)
{
    WTF::switchOn(targetContext.platformContext(), [](WebCore::MediaPlaybackTargetContextMock&&) {
        return;
    }, [&](WebCore::MediaPlaybackTargetContextCocoa&& context) {
        WebCore::MediaSessionHelper::activeVideoRouteDidChange(supportsAirPlayVideo, WebCore::MediaPlaybackTargetCocoa::create(WTFMove(context)));
    });
}

void RemoteMediaSessionHelper::activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback supportsSpatialAudioPlayback)
{
    WebCore::MediaSessionHelper::activeAudioRouteSupportsSpatialPlaybackDidChange(supportsSpatialAudioPlayback);
}

}

#endif

