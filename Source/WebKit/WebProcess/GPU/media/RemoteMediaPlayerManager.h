/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "GPUProcessConnection.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class MediaPlayerPrivateInterface;
}

namespace WebKit {

class MediaPlayerPrivateRemote;
class RemoteMediaPlayerMIMETypeCache;
class WebProcess;
struct PlatformTextTrackData;
struct TrackPrivateRemoteConfiguration;
struct WebProcessCreationParameters;

class RemoteMediaPlayerManager
    : public GPUProcessConnection::Client
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteMediaPlayerManager> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaPlayerManager);
public:
    static Ref<RemoteMediaPlayerManager> create();
    ~RemoteMediaPlayerManager();

    void setUseGPUProcess(bool);

    GPUProcessConnection& gpuProcessConnection();

    void didReceivePlayerMessage(IPC::Connection&, IPC::Decoder&);

    void deleteRemoteMediaPlayer(WebCore::MediaPlayerIdentifier);

    WebCore::MediaPlayerIdentifier findRemotePlayerId(const WebCore::MediaPlayerPrivateInterface*);

    RemoteMediaPlayerMIMETypeCache& typeCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier);

    void ref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteMediaPlayerManager>::ref(); }
    void deref() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteMediaPlayerManager>::deref(); }
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteMediaPlayerManager>::controlBlock(); }

    void initialize(const WebProcessCreationParameters&);

private:
    RemoteMediaPlayerManager();
    Ref<WebCore::MediaPlayerPrivateInterface> createRemoteMediaPlayer(WebCore::MediaPlayer*, WebCore::MediaPlayerEnums::MediaEngineIdentifier);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    friend class MediaPlayerRemoteFactory;
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, HashSet<String>&);
    WebCore::MediaPlayer::SupportsType supportsTypeAndCodecs(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&);
    bool supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String& keySystem, const String& mimeType);

    HashMap<WebCore::MediaPlayerIdentifier, ThreadSafeWeakPtr<MediaPlayerPrivateRemote>> m_players;
    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
