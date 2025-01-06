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

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(IOS_FAMILY)

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include <WebCore/MediaPlaybackTargetContext.h>
#include <WebCore/MediaSessionHelperIOS.h>

namespace WebKit {

class MediaPlaybackTargetContextSerialized;
class WebProcess;

class RemoteMediaSessionHelper final
    : public WebCore::MediaSessionHelper
    , public IPC::MessageReceiver
    , public GPUProcessConnection::Client {
public:
    RemoteMediaSessionHelper();
    virtual ~RemoteMediaSessionHelper() = default;

    IPC::Connection& ensureConnection();

    using HasAvailableTargets = WebCore::MediaSessionHelperClient::HasAvailableTargets;
    using PlayingToAutomotiveHeadUnit = WebCore::MediaSessionHelperClient::PlayingToAutomotiveHeadUnit;
    using ShouldPause = WebCore::MediaSessionHelperClient::ShouldPause;
    using SupportsAirPlayVideo = WebCore::MediaSessionHelperClient::SupportsAirPlayVideo;
    using SuspendedUnderLock = WebCore::MediaSessionHelperClient::SuspendedUnderLock;

    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // MediaSessionHelper
    void startMonitoringWirelessRoutesInternal() final;
    void stopMonitoringWirelessRoutesInternal() final;
    void providePresentingApplicationPID(int, ShouldOverride) final;

    // Messages
    void activeVideoRouteDidChange(SupportsAirPlayVideo, MediaPlaybackTargetContextSerialized&&);
    void activeAudioRouteSupportsSpatialPlaybackDidChange(SupportsSpatialAudioPlayback);

    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
};

}

#endif
