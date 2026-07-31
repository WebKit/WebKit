/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/NativePromise.h>
#include <wtf/Ref.h>

namespace WebCore {

class PlatformMediaSessionInterface;

// Abstracts the environment-coupled operations a MediaSessionManager performs, so that a manager
// which does not have a local WebCore::Page (e.g. the UI-process RemoteMediaSessionManagerProxy,
// which aggregates sessions from many web processes) can route them appropriately instead of
// reaching a WebCore::Page directly.
class MediaSessionManagerClient {
public:
    virtual ~MediaSessionManagerClient() = default;

    // Activate/deactivate the audio session on behalf of a session (may be null). The returned
    // promise resolves on success, rejects on failure to activate.
    virtual Ref<GenericPromise> tryToSetAudioSessionActive(bool active, PlatformMediaSessionInterface*) = 0;

    // Notify the environment (the owning page) that the active NowPlaying session changed.
    virtual void hasActiveNowPlayingSessionChanged(PlatformMediaSessionInterface*) = 0;
};

} // namespace WebCore
