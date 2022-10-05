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

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "GPUConnectionToWebProcess.h"
#include "MessageReceiver.h"
#include "SandboxExtension.h"
#include "ShareableBitmap.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <wtf/Lock.h>
#include <wtf/Logger.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteMediaPlayerProxy;
struct RemoteMediaPlayerConfiguration;
struct RemoteMediaPlayerProxyConfiguration;

class RemoteMediaPlayerManagerProxy
    : public IPC::MessageReceiver
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess&);
    ~RemoteMediaPlayerManagerProxy();

    GPUConnectionToWebProcess* gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }
    void clear();

#if !RELEASE_LOG_DISABLED
    Logger& logger();
#endif

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    bool didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) { return didReceiveSyncMessage(connection, decoder, encoder); }
    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncPlayerMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    RefPtr<WebCore::MediaPlayer> mediaPlayer(const WebCore::MediaPlayerIdentifier&);

    ShareableBitmapHandle bitmapImageForCurrentTime(WebCore::MediaPlayerIdentifier);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void createMediaPlayer(WebCore::MediaPlayerIdentifier, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&);
    void deleteMediaPlayer(WebCore::MediaPlayerIdentifier);

    // Media player factory
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, CompletionHandler<void(Vector<String>&&)>&&);
    void supportsTypeAndCodecs(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&&, CompletionHandler<void(WebCore::MediaPlayer::SupportsType)>&&);
    void originsInMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, CompletionHandler<void(HashSet<WebCore::SecurityOriginData>&&)>&&);
    void clearMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, WallTime);
    void clearMediaCacheForOrigins(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, HashSet<WebCore::SecurityOriginData>&&);
    void supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, const String&&, CompletionHandler<void(bool)>&&);

    HashMap<WebCore::MediaPlayerIdentifier, std::unique_ptr<RemoteMediaPlayerProxy>> m_proxies;
    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;

#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> m_logger;
#endif
};

} // namespace WebKit

#endif
