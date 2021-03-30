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

#include <WebCore/MediaSessionCoordinatorPrivate.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class MediaSessionCoordinatorPrivateProxy
    : public CanMakeWeakPtr<MediaSessionCoordinatorPrivateProxy>
    , public RefCounted<MediaSessionCoordinatorPrivateProxy> {
public:
    virtual ~MediaSessionCoordinatorPrivateProxy() = default;

    virtual void seekTo(double, CompletionHandler<void(const WebCore::ExceptionData&)>&&) = 0;
    virtual void play(CompletionHandler<void(const WebCore::ExceptionData&)>&&) = 0;
    virtual void pause(CompletionHandler<void(const WebCore::ExceptionData&)>&&) = 0;
    virtual void setTrack(const String&, CompletionHandler<void(const WebCore::ExceptionData&)>&&) = 0;

    virtual void positionStateChanged(const Optional<WebCore::MediaPositionState>&) = 0;
    virtual void readyStateChanged(WebCore::MediaSessionReadyState) = 0;
    virtual void playbackStateChanged(WebCore::MediaSessionPlaybackState) = 0;

    virtual void setClient(WeakPtr<WebCore::MediaSessionCoordinatorClient> client) { m_client = client; }

protected:
    explicit MediaSessionCoordinatorPrivateProxy() = default;

    WeakPtr<WebCore::MediaSessionCoordinatorClient> client() const { return m_client; }

private:
    WeakPtr<WebCore::MediaSessionCoordinatorClient> m_client;
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
