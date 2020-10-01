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
#include "WebMediaStrategy.h"

#include "GPUConnectionToWebProcess.h"
#include "GPUProcessConnection.h"
#include "RemoteAudioDestinationProxy.h"
#include "RemoteCDMFactory.h"
#include "WebProcess.h"
#include <WebCore/AudioDestination.h>
#include <WebCore/AudioIOCallback.h>
#include <WebCore/CDMFactory.h>
#include <WebCore/NowPlayingInfo.h>

#if PLATFORM(COCOA)
#include <WebCore/MediaSessionManagerCocoa.h>
#endif

namespace WebKit {

WebMediaStrategy::~WebMediaStrategy() = default;

#if ENABLE(WEB_AUDIO)
Ref<WebCore::AudioDestination> WebMediaStrategy::createAudioDestination(WebCore::AudioIOCallback& callback, const String& inputDeviceId,
    unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess)
        return RemoteAudioDestinationProxy::create(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate);
#endif
    return WebCore::AudioDestination::create(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate);
}
#endif

#if PLATFORM(COCOA)
void WebMediaStrategy::clearNowPlayingInfo()
{
#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess) {
        auto& connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
        connection.send(Messages::GPUConnectionToWebProcess::ClearNowPlayingInfo { }, 0);
        return;
    }
#endif
    WebCore::MediaSessionManagerCocoa::clearNowPlayingInfo();
}

void WebMediaStrategy::setNowPlayingInfo(bool setAsNowPlayingApplication, const WebCore::NowPlayingInfo& nowPlayingInfo)
{
#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess) {
        auto& connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
        connection.send(Messages::GPUConnectionToWebProcess::SetNowPlayingInfo { setAsNowPlayingApplication, nowPlayingInfo }, 0);
        return;
    }
#endif
    WebCore::MediaSessionManagerCocoa::setNowPlayingInfo(setAsNowPlayingApplication, nowPlayingInfo);
}
#endif

} // namespace WebKit
