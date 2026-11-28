/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include <WebCore/AudioDestination.h>
#include <WebCore/AudioIOCallback.h>
#include <WebCore/SharedAudioDestination.h>

#include <wtf/MainThread.h>

namespace TestWebKitAPI {
using namespace WebCore;

namespace {

class SilentCallback final : public AudioIOCallback {
    void render(AudioBus&, size_t, const AudioIOPosition&) final { }
    void isPlayingDidChange() final { }
};

class CountingAudioDestination final : public AudioDestination, public ThreadSafeRefCounted<CountingAudioDestination> {
public:
    static Ref<CountingAudioDestination> create(const CreationOptions& options, unsigned& startCount)
    {
        return adoptRef(*new CountingAudioDestination(options, startCount));
    }

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

private:
    CountingAudioDestination(const CreationOptions& options, unsigned& startCount)
        : AudioDestination(options)
        , m_startCount(startCount)
    {
    }

    void start(Function<void(Function<void()>&&)>&&, CompletionHandler<void(bool)>&& completionHandler) final
    {
        ++m_startCount;
        completionHandler(true);
    }

    void stop(CompletionHandler<void(bool)>&& completionHandler) final
    {
        completionHandler(true);
    }

    bool isPlaying() final { return false; }
    unsigned framesPerBuffer() const final { return 128; }

    unsigned& m_startCount;
};

} // namespace

TEST(SharedAudioDestination, SecondStartRestartsUnderlyingDestination)
{
    WTF::initializeMainThread();

    SilentCallback callback;
    String inputDeviceId;
    unsigned startCount = 0;
#if PLATFORM(IOS_FAMILY)
    String sceneIdentifier;
    AudioDestinationCreationOptions options { callback, inputDeviceId, 0, 13, 17171, sceneIdentifier };
#else
    AudioDestinationCreationOptions options { callback, inputDeviceId, 0, 13, 17171 };
#endif

    Ref<AudioDestination> destination = SharedAudioDestination::create(options, [&startCount](const AudioDestinationCreationOptions& creationOptions) -> Ref<AudioDestination> {
        return CountingAudioDestination::create(creationOptions, startCount);
    });

    bool didStart = false;
    destination->start(nullptr, [&didStart](bool success) {
        didStart = success;
    });
    EXPECT_TRUE(didStart);
    EXPECT_EQ(startCount, 1u);

    didStart = false;
    destination->start(nullptr, [&didStart](bool success) {
        didStart = success;
    });
    EXPECT_TRUE(didStart);
    EXPECT_EQ(startCount, 2u);

    destination->stop([](bool) { });
    destination->start(nullptr, [](bool) { });
    EXPECT_EQ(startCount, 3u);
    destination->stop([](bool) { });
}

} // namespace TestWebKitAPI

#endif
