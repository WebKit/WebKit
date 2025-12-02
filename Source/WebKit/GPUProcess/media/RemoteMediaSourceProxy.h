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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "MessageReceiver.h"
#include "RemoteMediaSourceIdentifier.h"
#include "RemoteSourceBufferProxy.h"
#include <WebCore/MediaSourcePrivate.h>
#include <WebCore/MediaSourcePrivateClient.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class ContentType;
class PlatformTimeRanges;
}

namespace WebKit {

class RemoteMediaPlayerManagerProxy;
class RemoteMediaPlayerProxy;

class RemoteMediaSourceProxy final
    : public WebCore::MediaSourcePrivateClient
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaSourceProxy);
public:
    RemoteMediaSourceProxy(RemoteMediaPlayerManagerProxy&, RemoteMediaSourceIdentifier, RemoteMediaPlayerProxy&);
    virtual ~RemoteMediaSourceProxy();

    void ref() const final { WebCore::MediaSourcePrivateClient::ref(); }
    void deref() const final { WebCore::MediaSourcePrivateClient::deref(); }

    void setMediaPlayers(RemoteMediaPlayerProxy&, WebCore::MediaPlayerPrivateInterface*);

    // MediaSourcePrivateClient overrides
    void setPrivateAndOpen(Ref<WebCore::MediaSourcePrivate>&&) final;
    void reOpen() final;
    Ref<WebCore::MediaTimePromise> waitForTarget(const WebCore::SeekTarget&) final;
    RefPtr<WebCore::MediaSourcePrivate> mediaSourcePrivate() const final { return m_private; }

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t) final;
#endif

    void failedToCreateRenderer(RendererType) final;

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    void connectionToWebProcessClosed();

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    using AddSourceBufferCallback = CompletionHandler<void(WebCore::MediaSourcePrivate::AddStatus, std::optional<RemoteSourceBufferIdentifier>)>;
    void addSourceBuffer(const WebCore::ContentType&, const WebCore::MediaSourceConfiguration&, AddSourceBufferCallback&&);
    void durationChanged(const MediaTime&);
    void bufferedChanged(WebCore::PlatformTimeRanges&&);
    void markEndOfStream(WebCore::MediaSourcePrivate::EndOfStreamStatus);
    void unmarkEndOfStream();
    void setMediaPlayerReadyState(WebCore::MediaPlayerEnums::ReadyState);
    void setTimeFudgeFactor(const MediaTime&);
    void attached();
    void shutdown();

    void disconnect();
    RefPtr<GPUConnectionToWebProcess> connectionToWebProcess() const;

    WeakPtr<RemoteMediaPlayerManagerProxy> m_manager;
    RemoteMediaSourceIdentifier m_identifier;
    RefPtr<WebCore::MediaSourcePrivate> m_private;
    WeakPtr<RemoteMediaPlayerProxy> m_remoteMediaPlayerProxy;

    Vector<RefPtr<RemoteSourceBufferProxy>> m_sourceBuffers;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
