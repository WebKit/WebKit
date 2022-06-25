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

#include "MessageReceiver.h"
#include <WebCore/MediaSessionCoordinatorPrivate.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WebKit {

class WebPage;

class RemoteMediaSessionCoordinator final : public WebCore::MediaSessionCoordinatorPrivate , public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteMediaSessionCoordinator> create(WebPage&, const String&);
    ~RemoteMediaSessionCoordinator();

private:
    explicit RemoteMediaSessionCoordinator(WebPage&, const String&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // MessageReceivers.
    void seekSessionToTime(double, CompletionHandler<void(bool)>&&);
    void playSession(std::optional<double>, std::optional<MonotonicTime>, CompletionHandler<void(bool)>&&);
    void pauseSession(CompletionHandler<void(bool)>&&);
    void setSessionTrack(const String&, CompletionHandler<void(bool)>&&);
    void coordinatorStateChanged(WebCore::MediaSessionCoordinatorState);

    // MediaSessionCoordinatorPrivate overrides.
    String identifier() const final { return m_identifier; }
    void join(CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&) final;
    void leave() final;
    void seekTo(double, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&) final;
    void play(CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&) final;
    void pause(CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&) final;
    void setTrack(const String&, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&) final;

    void positionStateChanged(const std::optional<WebCore::MediaPositionState>&) final;
    void readyStateChanged(WebCore::MediaSessionReadyState) final;
    void playbackStateChanged(WebCore::MediaSessionPlaybackState) final;
    void trackIdentifierChanged(const String&) final;

    const char* logClassName() const { return "RemoteMediaSessionCoordinator"; }
    WTFLogChannel& logChannel() const;

    WebPage& m_page;
    String m_identifier;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
