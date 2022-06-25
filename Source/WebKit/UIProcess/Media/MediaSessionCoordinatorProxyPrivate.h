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

#include <WebCore/ExceptionData.h>
#include <WebCore/MediaSessionCoordinatorPrivate.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct ExceptionData;
}

namespace WebKit {

using MediaSessionCommandCompletionHandler = CompletionHandler<void(std::optional<WebCore::ExceptionData>&&)>;

class MediaSessionCoordinatorProxyPrivate
    : public CanMakeWeakPtr<MediaSessionCoordinatorProxyPrivate>
    , public RefCounted<MediaSessionCoordinatorProxyPrivate> {
public:
    virtual ~MediaSessionCoordinatorProxyPrivate() = default;

    virtual String identifier() const = 0;

    virtual void join(MediaSessionCommandCompletionHandler&&) = 0;
    virtual void leave() = 0;
    virtual void seekTo(double, MediaSessionCommandCompletionHandler&&) = 0;
    virtual void play(MediaSessionCommandCompletionHandler&&) = 0;
    virtual void pause(MediaSessionCommandCompletionHandler&&) = 0;
    virtual void setTrack(const String&, MediaSessionCommandCompletionHandler&&) = 0;

    virtual void positionStateChanged(const std::optional<WebCore::MediaPositionState>&) = 0;
    virtual void readyStateChanged(WebCore::MediaSessionReadyState) = 0;
    virtual void playbackStateChanged(WebCore::MediaSessionPlaybackState) = 0;
    virtual void trackIdentifierChanged(const String&) = 0;

    virtual void setClient(WeakPtr<WebCore::MediaSessionCoordinatorClient> client) { m_client = client; }

protected:
    explicit MediaSessionCoordinatorProxyPrivate() = default;

    WeakPtr<WebCore::MediaSessionCoordinatorClient> client() const { return m_client; }

private:
    WeakPtr<WebCore::MediaSessionCoordinatorClient> m_client;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
