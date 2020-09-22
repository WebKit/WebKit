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

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(IOS_FAMILY)

#include "MessageReceiver.h"
#include <WebCore/MediaSessionHelperIOS.h>

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteMediaSessionHelperProxy
    : public WebCore::MediaSessionHelperClient
    , public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED();
public:
    RemoteMediaSessionHelperProxy(GPUConnectionToWebProcess&);
    virtual ~RemoteMediaSessionHelperProxy();

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void startMonitoringWirelessRoutes();
    void stopMonitoringWirelessRoutes();
    void providePresentingApplicationPID(int);

    // MediaSessionHelperClient
    void applicationWillEnterForeground(SuspendedUnderLock) final;
    void applicationDidEnterBackground(SuspendedUnderLock) final;
    void applicationWillBecomeInactive() final;
    void applicationDidBecomeActive() final;
    void externalOutputDeviceAvailableDidChange(HasAvailableTargets) final;
    void isPlayingToAutomotiveHeadUnitDidChange(PlayingToAutomotiveHeadUnit) final;
    void activeAudioRouteDidChange(ShouldPause) final;
    void activeVideoRouteDidChange(SupportsAirPlayVideo, Ref<WebCore::MediaPlaybackTarget>&&) final;

    bool m_isMonitoringWirelessRoutes { false };
    GPUConnectionToWebProcess& m_gpuConnection;
};

}

#endif
