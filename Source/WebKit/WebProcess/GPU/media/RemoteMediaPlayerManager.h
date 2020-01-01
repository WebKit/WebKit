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

#include "MediaPlayerPrivateRemoteIdentifier.h"
#include "MessageReceiver.h"
#include "RemoteMediaPlayerState.h"
#include "RemoteMediaResourceIdentifier.h"
#include "SharedMemory.h"
#include "WebProcessSupplement.h"
#include <WebCore/MediaPlayer.h>
#include <wtf/HashMap.h>

namespace WebCore {
class MediaPlayerPrivateInterface;
class Settings;
}

namespace WebKit {

class MediaPlayerPrivateRemote;
class WebProcess;

class RemoteMediaPlayerManager : public WebProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RemoteMediaPlayerManager(WebProcess&);
    ~RemoteMediaPlayerManager();

    static const char* supplementName();

    void updatePreferences(const WebCore::Settings&);

    IPC::Connection& gpuProcessConnection() const;

    void didReceiveMessageFromGPUProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

    void deleteRemoteMediaPlayer(MediaPlayerPrivateRemoteIdentifier);

private:
    std::unique_ptr<WebCore::MediaPlayerPrivateInterface> createRemoteMediaPlayer(WebCore::MediaPlayer*, WebCore::MediaPlayerEnums::MediaEngineIdentifier);

    // WebProcessSupplement
    void initialize(const WebProcessCreationParameters&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages::RemoteMediaPlayerManager
    void networkStateChanged(MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerState&&);
    void readyStateChanged(MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerState&&);
    void volumeChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, double);
    void muteChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, bool);
    void timeChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerState&&);
    void durationChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerState&&);
    void rateChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, double);
    void playbackStateChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, bool);
    void engineFailedToLoad(WebKit::MediaPlayerPrivateRemoteIdentifier, long);
    void updateCachedState(WebKit::MediaPlayerPrivateRemoteIdentifier, RemoteMediaPlayerState&&);
    void characteristicChanged(WebKit::MediaPlayerPrivateRemoteIdentifier, bool hasAudio, bool hasVideo, WebCore::MediaPlayerEnums::MovieLoadType);
    void requestResource(MediaPlayerPrivateRemoteIdentifier, RemoteMediaResourceIdentifier, WebCore::ResourceRequest&&, WebCore::PlatformMediaResourceLoader::LoadOptions, CompletionHandler<void()>&&);
    void removeResource(MediaPlayerPrivateRemoteIdentifier, RemoteMediaResourceIdentifier);

    friend class MediaPlayerRemoteFactory;
    void getSupportedTypes(WebCore::MediaPlayerEnums::MediaEngineIdentifier, HashSet<String, ASCIICaseInsensitiveHash>&);
    WebCore::MediaPlayer::SupportsType supportsTypeAndCodecs(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const WebCore::MediaEngineSupportParameters&);
    bool supportsKeySystem(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String& keySystem, const String& mimeType);
    HashSet<RefPtr<WebCore::SecurityOrigin>> originsInMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&);
    void clearMediaCache(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&, WallTime modifiedSince);
    void clearMediaCacheForOrigins(WebCore::MediaPlayerEnums::MediaEngineIdentifier, const String&, const HashSet<RefPtr<WebCore::SecurityOrigin>>&);

    HashMap<MediaPlayerPrivateRemoteIdentifier, WeakPtr<MediaPlayerPrivateRemote>> m_players;
    WebProcess& m_process;
};

} // namespace WebKit

#endif
