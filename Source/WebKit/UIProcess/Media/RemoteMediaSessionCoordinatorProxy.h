/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "MediaSessionCoordinatorProxyPrivate.h"
#include "MessageReceiver.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
struct ExceptionData;
}

namespace WebKit {

class WebPageProxy;

class RemoteMediaSessionCoordinatorProxy
    : public IPC::MessageReceiver
    , public RefCounted<RemoteMediaSessionCoordinatorProxy>
    , public WebCore::MediaSessionCoordinatorClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteMediaSessionCoordinatorProxy> create(WebPageProxy&, Ref<MediaSessionCoordinatorProxyPrivate>&&);
    ~RemoteMediaSessionCoordinatorProxy();

    void seekTo(double, CompletionHandler<void(bool)>&&);
    void play(CompletionHandler<void(bool)>&&);
    void pause(CompletionHandler<void(bool)>&&);
    void setTrack(const String&, CompletionHandler<void(bool)>&&);

    using MediaSessionCoordinatorClient::weakPtrFactory;
    using WeakValueType = MediaSessionCoordinatorClient::WeakValueType;

private:
    explicit RemoteMediaSessionCoordinatorProxy(WebPageProxy&, Ref<MediaSessionCoordinatorProxyPrivate>&&);

    using CoordinateCompletionHandler = CompletionHandler<void(const WebCore::ExceptionData&)>;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Receivers.
    using CommandCompletionHandler = CompletionHandler<void(Optional<WebCore::ExceptionData>&&)>;
    void join(CommandCompletionHandler&&);
    void leave();
    void coordinateSeekTo(double, CommandCompletionHandler&&);
    void coordinatePlay(CommandCompletionHandler&&);
    void coordinatePause(CommandCompletionHandler&&);
    void coordinateSetTrack(const String&, CommandCompletionHandler&&);
    void positionStateChanged(const Optional<WebCore::MediaPositionState>&);
    void readyStateChanged(WebCore::MediaSessionReadyState);
    void playbackStateChanged(WebCore::MediaSessionPlaybackState);
    void coordinatorStateChanged(WebCore::MediaSessionCoordinatorState);

    // MediaSessionCoordinatorClient
    void seekSessionToTime(double, CompletionHandler<void(bool)>&&) final;
    void playSession(CompletionHandler<void(bool)>&&) final;
    void pauseSession(CompletionHandler<void(bool)>&&) final;
    void setSessionTrack(const String&, CompletionHandler<void(bool)>&&) final;

#if !RELEASE_LOG_DISABLED
    const WTF::Logger& logger() const { return m_logger; }
    const void* logIdentifier() const { return m_logIdentifier; }
    const char* logClassName() const { return "RemoteMediaSessionCoordinatorProxy"; }
    WTFLogChannel& logChannel() const;
#endif

    WebPageProxy& m_webPageProxy;
    Ref<MediaSessionCoordinatorProxyPrivate> m_privateCoordinator;
#if !RELEASE_LOG_DISABLED
    Ref<const WTF::Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
