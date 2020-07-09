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
#include "RemoteMediaSessionHelperMessages.h"

namespace WebKit {
using namespace WebCore;

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

void RemoteMediaSessionHelperProxy::providePresentingApplicationPID(int pid)
{
    MediaSessionHelper::sharedHelper().providePresentingApplicationPID(pid);
}

void RemoteMediaSessionHelperProxy::applicationWillEnterForeground(SuspendedUnderLock suspendedUnderLock)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ApplicationWillEnterForeground(suspendedUnderLock), { });
}

void RemoteMediaSessionHelperProxy::applicationDidEnterBackground(SuspendedUnderLock suspendedUnderLock)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ApplicationDidEnterBackground(suspendedUnderLock), { });
}

void RemoteMediaSessionHelperProxy::applicationWillBecomeInactive()
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ApplicationWillBecomeInactive(), { });
}

void RemoteMediaSessionHelperProxy::applicationDidBecomeActive()
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ApplicationDidBecomeActive(), { });
}

void RemoteMediaSessionHelperProxy::externalOutputDeviceAvailableDidChange(HasAvailableTargets hasAvailableTargets)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ExternalOutputDeviceAvailableDidChange(hasAvailableTargets), { });
}

void RemoteMediaSessionHelperProxy::isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit playing)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::IsPlayingToAutomotiveHeadUnitDidChange(playing), { });
}

void RemoteMediaSessionHelperProxy::activeAudioRouteDidChange(ShouldPause shouldPause)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ActiveAudioRouteDidChange(shouldPause), { });
}

void RemoteMediaSessionHelperProxy::activeVideoRouteDidChange(SupportsAirPlayVideo supportsAirPlayVideo, Ref<WebCore::MediaPlaybackTarget>&& target)
{
    m_gpuConnection.connection().send(Messages::RemoteMediaSessionHelper::ActiveVideoRouteDidChange(supportsAirPlayVideo, target->targetContext()), { });
}

}

#endif
