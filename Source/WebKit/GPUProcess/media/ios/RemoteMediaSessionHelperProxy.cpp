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
#include "RemoteMediaSessionHelperProxy.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(IOS_FAMILY)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MediaPlaybackTargetContextSerialized.h"
#include "RemoteMediaSessionHelperMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionHelperProxy);

RemoteMediaSessionHelperProxy::RemoteMediaSessionHelperProxy(GPUConnectionToWebProcess& gpuConnection)
    : m_gpuConnection(gpuConnection)
{
    MediaSessionHelper::sharedHelper().addClient(*this);
}

RemoteMediaSessionHelperProxy::~RemoteMediaSessionHelperProxy()
{
    stopMonitoringWirelessRoutes();
    MediaSessionHelper::sharedHelper().removeClient(*this);
}

void RemoteMediaSessionHelperProxy::ref() const
{
    m_gpuConnection.get()->ref();
}

void RemoteMediaSessionHelperProxy::deref() const
{
    m_gpuConnection.get()->deref();
}

void RemoteMediaSessionHelperProxy::startMonitoringWirelessRoutes()
{
    if (m_isMonitoringWirelessRoutes)
        return;

    m_isMonitoringWirelessRoutes = true;
    MediaSessionHelper::sharedHelper().startMonitoringWirelessRoutes();
}

void RemoteMediaSessionHelperProxy::stopMonitoringWirelessRoutes()
{
    if (!m_isMonitoringWirelessRoutes)
        return;

    m_isMonitoringWirelessRoutes = false;
    MediaSessionHelper::sharedHelper().stopMonitoringWirelessRoutes();
}

void RemoteMediaSessionHelperProxy::providePresentingApplicationPID(int pid, MediaSessionHelper::ShouldOverride shouldOverride)
{
    m_presentingApplicationPID = pid;
    MediaSessionHelper::sharedHelper().providePresentingApplicationPID(pid, shouldOverride);
}

void RemoteMediaSessionHelperProxy::overridePresentingApplicationPIDIfNeeded()
{
    if (m_presentingApplicationPID)
        MediaSessionHelper::sharedHelper().providePresentingApplicationPID(*m_presentingApplicationPID, MediaSessionHelper::ShouldOverride::Yes);
}

void RemoteMediaSessionHelperProxy::applicationWillEnterForeground(SuspendedUnderLock suspendedUnderLock)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ApplicationWillEnterForeground(suspendedUnderLock), { });
}

void RemoteMediaSessionHelperProxy::applicationDidEnterBackground(SuspendedUnderLock suspendedUnderLock)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ApplicationDidEnterBackground(suspendedUnderLock), { });
}

void RemoteMediaSessionHelperProxy::applicationWillBecomeInactive()
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ApplicationWillBecomeInactive(), { });
}

void RemoteMediaSessionHelperProxy::applicationDidBecomeActive()
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ApplicationDidBecomeActive(), { });
}

void RemoteMediaSessionHelperProxy::externalOutputDeviceAvailableDidChange(HasAvailableTargets hasAvailableTargets)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ExternalOutputDeviceAvailableDidChange(hasAvailableTargets), { });
}

void RemoteMediaSessionHelperProxy::isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit playing)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::IsPlayingToAutomotiveHeadUnitDidChange(playing), { });
}

void RemoteMediaSessionHelperProxy::activeAudioRouteDidChange(ShouldPause shouldPause)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ActiveAudioRouteDidChange(shouldPause), { });
}

void RemoteMediaSessionHelperProxy::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, Ref<WebCore::MediaPlaybackTarget>&& target)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ActiveVideoRouteDidChange(supportsAirPlayVideo, MediaPlaybackTargetContextSerialized { target->targetContext() }), { });
}

void RemoteMediaSessionHelperProxy::activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback supportsSpatialPlayback)
{
    if (auto connection = m_gpuConnection.get())
        connection->connection().send(Messages::RemoteMediaSessionHelper::ActiveAudioRouteSupportsSpatialPlaybackDidChange(supportsSpatialPlayback), { });
}

std::optional<SharedPreferencesForWebProcess> RemoteMediaSessionHelperProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnection.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

}

#endif
