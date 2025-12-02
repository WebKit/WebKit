/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "WebProcess.h"
#include <WebCore/AudioDestination.h>
#include <WebCore/AudioIOCallback.h>
#include <WebCore/CDMFactory.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/SharedAudioDestination.h>

#if PLATFORM(COCOA)
#include <WebCore/MediaSessionManagerCocoa.h>
#endif

#if ENABLE(MEDIA_SOURCE)
#include <WebCore/DeprecatedGlobalSettings.h>
#endif
#if ENABLE(VIDEO) && ENABLE(GPU_PROCESS)
#include "AudioVideoRendererRemote.h"
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebMediaStrategy);

WebMediaStrategy::~WebMediaStrategy() = default;

#if ENABLE(WEB_AUDIO)
Ref<WebCore::AudioDestination> WebMediaStrategy::createAudioDestination(const WebCore::AudioDestinationCreationOptions& options)
{
    ASSERT(isMainRunLoop());
#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess)
        return WebCore::SharedAudioDestination::create(options, [] (auto& options) {
            return RemoteAudioDestinationProxy::create(options);
        });
#endif
    return WebCore::AudioDestination::create(options);
}
#endif

#if ENABLE(VIDEO) && ENABLE(GPU_PROCESS)
RefPtr<AudioVideoRenderer> WebMediaStrategy::createAudioVideoRenderer(LoggerHelper* loggerHelper, WebCore::HTMLMediaElementIdentifier mediaElementIdentifier, WebCore::MediaPlayerIdentifier playerIdentifier) const
{
    return AudioVideoRendererRemote::create(loggerHelper, mediaElementIdentifier, playerIdentifier, WebProcess::singleton().ensureProtectedGPUProcessConnection());
}
#endif

std::unique_ptr<WebCore::NowPlayingManager> WebMediaStrategy::createNowPlayingManager() const
{
    ASSERT(isMainRunLoop());
#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess) {
        class NowPlayingInfoForGPUManager : public WebCore::NowPlayingManager {
            void clearNowPlayingInfoPrivate() final
            {
                if (RefPtr connection = WebProcess::singleton().existingGPUProcessConnection())
                    connection->connection().send(Messages::GPUConnectionToWebProcess::ClearNowPlayingInfo { }, 0);
            }

            void setNowPlayingInfoPrivate(const WebCore::NowPlayingInfo& nowPlayingInfo, bool) final
            {
                Ref connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
                connection->send(Messages::GPUConnectionToWebProcess::SetNowPlayingInfo { nowPlayingInfo }, 0);
            }
        };
        return makeUnique<NowPlayingInfoForGPUManager>();
    }
#endif
    return WebCore::MediaStrategy::createNowPlayingManager();
}

bool WebMediaStrategy::hasThreadSafeMediaSourceSupport() const
{
#if ENABLE(GPU_PROCESS)
    return m_useGPUProcess;
#else
    return false;
#endif
}

#if ENABLE(MEDIA_SOURCE)
void WebMediaStrategy::enableMockMediaSource()
{
    ASSERT(isMainRunLoop());
#if USE(AVFOUNDATION)
    WebCore::DeprecatedGlobalSettings::setAVFoundationEnabled(false);
#endif
#if USE(GSTREAMER)
    WebCore::DeprecatedGlobalSettings::setGStreamerEnabled(false);
#endif
    m_mockMediaSourceEnabled = true;

#if USE(AVFOUNDATION)
    if (hasRemoteRendererFor(MediaPlayerMediaEngineIdentifier::AVFoundationMSE)) {
        WebCore::MediaStrategy::addMockMediaSourceEngine();
        return;
    }
#endif

#if ENABLE(GPU_PROCESS)
    if (m_useGPUProcess) {
        Ref connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
        connection->send(Messages::GPUConnectionToWebProcess::EnableMockMediaSource { }, 0);
        return;
    }
#endif
    WebCore::MediaStrategy::addMockMediaSourceEngine();
}
#endif

} // namespace WebKit
