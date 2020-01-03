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
#include "MediaPlayerPrivateRemoteIdentifier.h"
#include "MessageReceiver.h"
#include <WebCore/MediaPlayer.h>
#include <wtf/LoggerHelper.h>

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteMediaResourceManager;
class RemoteMediaPlayerProxy;
struct RemoteMediaPlayerConfiguration;
struct RemoteMediaPlayerProxyConfiguration;

class RemoteMediaPlayerManagerProxy
    : private IPC::MessageReceiver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess&);
    ~RemoteMediaPlayerManagerProxy();

    GPUConnectionToWebProcess& gpuConnectionToWebProcess() const { return m_gpuConnectionToWebProcess; }
    void clear();

    void didReceiveMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }
    void didReceiveSyncMessageFromWebProcess(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& encoder) { didReceiveSyncMessage(connection, decoder, encoder); }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "RemoteMediaPlayerManagerProxy"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void createMediaPlayer(MediaPlayerPrivateRemoteIdentifier, WebCore::MediaPlayerEnums::MediaEngineIdentifier, RemoteMediaPlayerProxyConfiguration&&, CompletionHandler<void(RemoteMediaPlayerConfiguration&)>&&);
    void deleteMediaPlayer(MediaPlayerPrivateRemoteIdentifier);

    // Media player factory
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, CompletionHandler<void(Vector<String>&&)>&&);
    void supportsType(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&&, CompletionHandler<void(WebCore::MediaPlayer::SupportsType)>&&);
    void originsInMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, CompletionHandler<void(Vector<WebCore::SecurityOriginData>&&)>&&);
    void clearMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, WallTime);
    void clearMediaCacheForOrigins(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, Vector<WebCore::SecurityOriginData>&&);
    void supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&&, const String&&, CompletionHandler<void(bool)>&&);

    void load(MediaPlayerPrivateRemoteIdentifier, URL&&, WebCore::ContentType&&, String&&, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&&);
    void prepareForPlayback(MediaPlayerPrivateRemoteIdentifier, bool privateMode, WebCore::MediaPlayerEnums::Preload, bool preservesPitch, bool prepareForRendering);
    void cancelLoad(MediaPlayerPrivateRemoteIdentifier);
    void prepareToPlay(MediaPlayerPrivateRemoteIdentifier);

    void play(MediaPlayerPrivateRemoteIdentifier);
    void pause(MediaPlayerPrivateRemoteIdentifier);

    void seek(MediaPlayerPrivateRemoteIdentifier, MediaTime&&);
    void seekWithTolerance(MediaPlayerPrivateRemoteIdentifier, MediaTime&&, MediaTime&& negativeTolerance, MediaTime&& positiveTolerance);

    void setVolume(MediaPlayerPrivateRemoteIdentifier, double);
    void setMuted(MediaPlayerPrivateRemoteIdentifier, bool);

    void setPreload(MediaPlayerPrivateRemoteIdentifier, WebCore::MediaPlayerEnums::Preload);
    void setPrivateBrowsingMode(MediaPlayerPrivateRemoteIdentifier, bool);
    void setPreservesPitch(MediaPlayerPrivateRemoteIdentifier, bool);

    HashMap<MediaPlayerPrivateRemoteIdentifier, std::unique_ptr<RemoteMediaPlayerProxy>> m_proxies;
    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;

#if !RELEASE_LOG_DISABLED
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

#endif
