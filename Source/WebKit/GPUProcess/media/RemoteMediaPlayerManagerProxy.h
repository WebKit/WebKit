/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "SandboxExtension.h"
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Logger.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

#if ENABLE(LINEAR_MEDIA_PLAYER)
#include <WebCore/VideoReceiverEndpoint.h>
#endif

namespace WebKit {
class RemoteMediaPlayerManagerProxy;
class VideoReceiverEndpointMessage;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::RemoteMediaPlayerManagerProxy> : std::true_type { };
}

namespace WebKit {

class RemoteMediaPlayerProxy;
struct RemoteMediaPlayerConfiguration;
struct RemoteMediaPlayerProxyConfiguration;

class RemoteMediaPlayerManagerProxy
    : public IPC::MessageReceiver
{
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaPlayerManagerProxy);
public:
    explicit RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess&);
    ~RemoteMediaPlayerManagerProxy();

    RefPtr<GPUConnectionToWebProcess> gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }
    void clear();

#if !RELEASE_LOG_DISABLED
    Logger& logger();
#endif

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    bool didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) { return didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncPlayerMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    RefPtr<WebCore::MediaPlayer> mediaPlayer(const WebCore::MediaPlayerIdentifier&);

    std::optional<WebCore::ShareableBitmap::Handle> bitmapImageForCurrentTime(WebCore::MediaPlayerIdentifier);

#if ENABLE(LINEAR_MEDIA_PLAYER)
    WebCore::PlatformVideoTarget videoTargetForMediaElementIdentifier(WebCore::HTMLMediaElementIdentifier);
    void handleVideoReceiverEndpointMessage(const VideoReceiverEndpointMessage&);
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void createMediaPlayer(WebCore::MediaPlayerIdentifier, WebCore::MediaPlayerClientIdentifier, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&);
    void deleteMediaPlayer(WebCore::MediaPlayerIdentifier);

    // Media player factory
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, CompletionHandler<void(Vector<String>&&)>&&);
    void supportsTypeAndCodecs(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&&, CompletionHandler<void(WebCore::MediaPlayer::SupportsType)>&&);
    void supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, const String&&, CompletionHandler<void(bool)>&&);

    HashMap<WebCore::MediaPlayerIdentifier, Ref<RemoteMediaPlayerProxy>> m_proxies;
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;

#if ENABLE(LINEAR_MEDIA_PLAYER)
    struct VideoRecevierEndpointCacheEntry {
        WebCore::MediaPlayerIdentifier playerIdentifier;
        WebCore::VideoReceiverEndpoint endpoint;
        WebCore::PlatformVideoTarget videoTarget;
    };
    HashMap<WebCore::HTMLMediaElementIdentifier, VideoRecevierEndpointCacheEntry> m_videoReceiverEndpointCache;
#endif

#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> m_logger;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
