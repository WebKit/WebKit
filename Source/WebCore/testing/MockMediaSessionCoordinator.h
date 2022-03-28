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

#include "MediaSessionCoordinatorPrivate.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ScriptExecutionContext;
class StringCallback;

class MockMediaSessionCoordinator : public MediaSessionCoordinatorPrivate, public CanMakeWeakPtr<MockMediaSessionCoordinator> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MockMediaSessionCoordinator> create(ScriptExecutionContext&, RefPtr<StringCallback>&&);

    void setCommandsShouldFail(bool shouldFail) { m_failCommands = shouldFail; }

private:
    MockMediaSessionCoordinator(ScriptExecutionContext&, RefPtr<StringCallback>&&);

    String identifier() const final { return "Mock Coordinator"_s; }

    void join(CompletionHandler<void(std::optional<Exception>&&)>&&) final;
    void leave() final;

    void seekTo(double, CompletionHandler<void(std::optional<Exception>&&)>&&) final;
    void play(CompletionHandler<void(std::optional<Exception>&&)>&&) final;
    void pause(CompletionHandler<void(std::optional<Exception>&&)>&&) final;
    void setTrack(const String&, CompletionHandler<void(std::optional<Exception>&&)>&&) final;

    void positionStateChanged(const std::optional<MediaPositionState>&) final;
    void readyStateChanged(MediaSessionReadyState) final;
    void playbackStateChanged(MediaSessionPlaybackState) final;
    void trackIdentifierChanged(const String&) final;

    const char* logClassName() const { return "MockMediaSessionCoordinator"; }
    WTFLogChannel& logChannel() const;

    std::optional<Exception> result() const;

    Ref<ScriptExecutionContext> m_context;
    RefPtr<StringCallback> m_stateChangeListener;
    bool m_failCommands { false };
};

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
