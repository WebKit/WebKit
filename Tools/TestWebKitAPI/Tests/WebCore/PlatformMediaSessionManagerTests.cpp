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

#include "config.h"

#include <WebCore/PlatformMediaSession.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <wtf/Logger.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(MAC)
#include <WebCore/AudioHardwareListener.h>
#include <WebCore/MediaSessionManagerCocoa.h>
#include <wtf/RefCounted.h>
#endif

namespace TestWebKitAPI {
using namespace WebCore;

class TestMediaSessionManager final : public PlatformMediaSessionManager {
public:
    static Ref<TestMediaSessionManager> create() { return adoptRef(*new TestMediaSessionManager()); }

    using PlatformMediaSessionManager::currentSession;

private:
    TestMediaSessionManager()
        : PlatformMediaSessionManager(std::nullopt)
    {
    }
};

class TestMediaSessionClient final : public PlatformMediaSessionClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestMediaSessionClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TestMediaSessionClient);
public:
    explicit TestMediaSessionClient(MediaSessionManagerInterface& manager)
        : m_manager(manager)
    {
    }

    RefPtr<MediaSessionManagerInterface> sessionManager() const final { return m_manager.ptr(); }

    PlatformMediaSessionMediaType mediaType() const final { return PlatformMediaSessionMediaType::Video; }
    PlatformMediaSessionMediaType presentationType() const final { return PlatformMediaSessionMediaType::Video; }

    void mayResumePlayback(bool) final { }
    void suspendPlayback() final { }

    bool canReceiveRemoteControlCommands() const final { return false; }
    void didReceiveRemoteControlCommand(PlatformMediaSessionRemoteControlCommandType, const PlatformMediaSessionRemoteCommandArgument&) final { }
    bool supportsSeeking() const final { return false; }

    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSessionInterruptionType) const final { return false; }

    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final { return std::nullopt; }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return 0; }
#endif

private:
    Ref<MediaSessionManagerInterface> m_manager;
#if !RELEASE_LOG_DISABLED
    const Ref<Logger> m_logger { Logger::create(this) };
#endif
};

TEST(PlatformMediaSessionManager, PausingFrontSessionDemotesItInCurrentSession)
{
    auto manager = TestMediaSessionManager::create();

    auto clientA = makeUnique<TestMediaSessionClient>(manager.get());
    auto clientB = makeUnique<TestMediaSessionClient>(manager.get());

    auto sessionA = PlatformMediaSession::create(*clientA);
    auto sessionB = PlatformMediaSession::create(*clientB);

    // setActive(true) registers the session with the manager (appended), giving
    // the order [A, B]. Both are playing.
    sessionA->setActive(true);
    sessionB->setActive(true);
    sessionA->setState(PlatformMediaSession::State::Playing);
    sessionB->setState(PlatformMediaSession::State::Playing);

    EXPECT_EQ(manager->currentSession().get(), static_cast<PlatformMediaSessionInterface*>(sessionA.ptr()));

    // A pauses. Since no other session is paused, A must be moved to the back,
    // so B becomes the current session.
    manager->sessionWillEndPlayback(sessionA.get(), DelayCallingUpdateNowPlaying::No);

    // With the bug, the reorder hits a copy and currentSession() is still A.
    EXPECT_EQ(manager->currentSession().get(), static_cast<PlatformMediaSessionInterface*>(sessionB.ptr()));
}

#if PLATFORM(MAC)

// Stands in for the audio-hardware listener the UI-process proxy retains under site isolation:
// kept alive by the test and fired manually after the manager has dropped its own reference.
class TestFiringAudioHardwareListener final : public AudioHardwareListener, public RefCounted<TestFiringAudioHardwareListener> {
public:
    static Ref<TestFiringAudioHardwareListener> create(Client& client) { return adoptRef(*new TestFiringAudioHardwareListener(client)); }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void fireOutputDeviceChanged() { m_client.audioOutputDeviceChanged(); }

private:
    explicit TestFiringAudioHardwareListener(Client& client)
        : AudioHardwareListener(client)
    {
    }
};

class TestMediaSessionManagerCocoa final : public MediaSessionManagerCocoa {
public:
    static Ref<TestMediaSessionManagerCocoa> create() { return adoptRef(*new TestMediaSessionManagerCocoa()); }

private:
    TestMediaSessionManagerCocoa()
        : MediaSessionManagerCocoa(std::nullopt)
    {
    }
};

TEST(PlatformMediaSessionManager, AudioOutputDeviceChangeAfterLastSessionRemovedDoesNotCrash)
{
    RefPtr<TestFiringAudioHardwareListener> listener;
    AudioHardwareListener::setCreationFunction([&](AudioHardwareListener::Client& client) -> Ref<AudioHardwareListener> {
        Ref newListener = TestFiringAudioHardwareListener::create(client);
        listener = newListener.ptr();
        return newListener;
    });

    auto manager = TestMediaSessionManagerCocoa::create();
    auto client = makeUnique<TestMediaSessionClient>(manager.get());
    auto session = PlatformMediaSession::create(*client);

    // Registering the first session creates MediaSessionManagerCocoa::m_audioHardwareListener.
    session->setActive(true);
    ASSERT_TRUE(listener);

    // Removing the last session clears m_audioHardwareListener; the listener object survives here
    // (as the proxy keeps it alive under site isolation) and can still deliver a device change.
    session->setActive(false);

    // Before the fix this dereferenced the now-null m_audioHardwareListener and crashed.
    listener->fireOutputDeviceChanged();

    AudioHardwareListener::resetCreationFunction();
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
