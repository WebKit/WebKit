/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#if ENABLE(WEB_AUDIO)

#include <WebCore/AudioDestination.h>
#include <WebCore/PageIdentifier.h>

namespace WebCore {

class SharedAudioDestinationAdapter;

class SharedAudioDestination final : public AudioDestination, public ThreadSafeRefCounted<SharedAudioDestination, WTF::DestructionThread::Main> {
public:
    using AudioDestinationCreationFunction = Function<Ref<AudioDestination>(const CreationOptions&)>;
    WEBCORE_EXPORT static Ref<SharedAudioDestination> create(const CreationOptions&, AudioDestinationCreationFunction&&);
    WEBCORE_EXPORT virtual ~SharedAudioDestination();

    void ref() const final { return ThreadSafeRefCounted::ref(); }
    void deref() const final { return ThreadSafeRefCounted::deref(); }

    void sharedRender(AudioBus& destinationBus, size_t framesToProcess, const AudioIOPosition&);

private:
    SharedAudioDestination(const CreationOptions&, AudioDestinationCreationFunction&&);

    // AudioDestination
    void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    void stop(CompletionHandler<void(bool)>&&) final;
    bool isPlaying() final { return m_isPlaying; }
    unsigned framesPerBuffer() const final;
    MediaTime outputLatency() const final;
#if PLATFORM(IOS_FAMILY)
    void setSceneIdentifier(const String&) final;
#endif

    void setIsPlaying(bool);

    Ref<SharedAudioDestinationAdapter> protectedOutputAdapter() const;

    Lock m_dispatchToRenderThreadLock;
    Function<void(Function<void()>&&)> m_dispatchToRenderThread WTF_GUARDED_BY_LOCK(m_dispatchToRenderThreadLock);

    bool m_isPlaying { false };
    Ref<SharedAudioDestinationAdapter> m_outputAdapter;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
