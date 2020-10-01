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
#include "GPUProcessConnection.h"
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
    m_process.ensureGPUProcessConnection().messageReceiverMap().addMessageReceiver(Messages::RemoteMediaSessionHelper::messageReceiverName(), *this);
    connection().send(Messages::GPUConnectionToWebProcess::EnsureMediaSessionHelper(), { });
}

IPC::Connection& RemoteMediaSessionHelper::connection()
{
    return m_process.ensureGPUProcessConnection().connection();
}

void RemoteMediaSessionHelper::startMonitoringWirelessRoutes()
{
    if (m_monitoringWirelessRoutesCount++)
        return;

    connection().send(Messages::RemoteMediaSessionHelperProxy::StartMonitoringWirelessRoutes(), { });
}

void RemoteMediaSessionHelper::stopMonitoringWirelessRoutes()
{
    if (!m_monitoringWirelessRoutesCount) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (--m_monitoringWirelessRoutesCount)
        return;

    connection().send(Messages::RemoteMediaSessionHelperProxy::StopMonitoringWirelessRoutes(), { });
}

void RemoteMediaSessionHelper::providePresentingApplicationPID(int pid)
{
    connection().send(Messages::RemoteMediaSessionHelperProxy::ProvidePresentingApplicationPID(pid), { });
}

void RemoteMediaSessionHelper::applicationWillEnterForeground(SuspendedUnderLock suspendedUnderLock)
{
    for (auto& client : m_clients)
        client.applicationWillEnterForeground(suspendedUnderLock);
}

void RemoteMediaSessionHelper::applicationDidEnterBackground(SuspendedUnderLock suspendedUnderLock)
{
    for (auto& client : m_clients)
        client.applicationDidEnterBackground(suspendedUnderLock);
}

void RemoteMediaSessionHelper::applicationWillBecomeInactive()
{
    for (auto& client : m_clients)
        client.applicationWillBecomeInactive();
}

void RemoteMediaSessionHelper::applicationDidBecomeActive()
{
    for (auto& client : m_clients)
        client.applicationDidBecomeActive();
}

void RemoteMediaSessionHelper::externalOutputDeviceAvailableDidChange(HasAvailableTargets hasAvailableTargets)
{
    for (auto& client : m_clients)
        client.externalOutputDeviceAvailableDidChange(hasAvailableTargets);
}

void RemoteMediaSessionHelper::isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit playingToAutomotiveHeadUnit)
{
    for (auto& client : m_clients)
        client.isPlayingToAutomotiveHeadUnitDidChange(playingToAutomotiveHeadUnit);
}

void RemoteMediaSessionHelper::activeAudioRouteDidChange(ShouldPause shouldPause)
{
    for (auto& client : m_clients)
        client.activeAudioRouteDidChange(shouldPause);
}

void RemoteMediaSessionHelper::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, MediaPlaybackTargetContext&& targetContext)
{
    RefPtr<MediaPlaybackTarget> targetObject;
    if (targetContext.type() == MediaPlaybackTargetContext::AVOutputContextType)
        targetObject = WebCore::MediaPlaybackTargetCocoa::create(targetContext.avOutputContext());
    else {
        ASSERT_NOT_REACHED();
        return;
    }

    for (auto& client : m_clients)
        client.activeVideoRouteDidChange(supportsAirPlayVideo, targetObject.copyRef().releaseNonNull());
}

}

#endif

