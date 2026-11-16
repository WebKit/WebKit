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

#if ENABLE(VIDEO)

#include <WebCore/AudioTrackPrivate.h>
#include <WebCore/VideoTrackPrivate.h>
#include <wtf/Seconds.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {
using namespace WebCore;

// The media backends install their track change callbacks from the thread driving the
// media pipeline (e.g. the MediaSource work queue) while the callbacks are invoked from
// the thread servicing the DOM. Installing a new callback must not destroy a callback
// that is being invoked right now.

namespace {

constexpr Seconds callbackBodyDuration { 50_ms };
constexpr Seconds reassignmentInterval { 5_ms };
constexpr unsigned invocationCount { 10 };

class TestAudioTrackPrivate final : public AudioTrackPrivate {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestAudioTrackPrivate);
public:
    static Ref<TestAudioTrackPrivate> create() { return adoptRef(*new TestAudioTrackPrivate); }
};

class TestVideoTrackPrivate final : public VideoTrackPrivate {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestVideoTrackPrivate);
public:
    static Ref<TestVideoTrackPrivate> create() { return adoptRef(*new TestVideoTrackPrivate); }
};

// Records, for one installed callback, whether that callback was destroyed.
class CallbackObservation : public ThreadSafeRefCounted<CallbackObservation> {
public:
    static Ref<CallbackObservation> create() { return adoptRef(*new CallbackObservation); }

    std::atomic<bool> destroyed { false };
};

// Held by value by an installed callback, so that destroying the callback marks its
// observation as destroyed.
class CallbackObserver {
public:
    explicit CallbackObserver(Ref<CallbackObservation>&& observation)
        : m_observation(WTF::move(observation))
    {
    }

    CallbackObserver(CallbackObserver&&) = default;
    CallbackObserver& operator=(CallbackObserver&&) = default;

    ~CallbackObserver()
    {
        if (m_observation)
            m_observation->destroyed = true;
    }

private:
    RefPtr<CallbackObservation> m_observation;
};

class TrackCallbackRaceState : public ThreadSafeRefCounted<TrackCallbackRaceState> {
public:
    static Ref<TrackCallbackRaceState> create() { return adoptRef(*new TrackCallbackRaceState); }

    std::atomic<bool> stopReassigning { false };
    std::atomic<unsigned> invocations { 0 };
    std::atomic<unsigned> destroyedWhileRunning { 0 };
};

void installEnabledChangedCallback(AudioTrackPrivate& track, TrackCallbackRaceState& state)
{
    Ref observation = CallbackObservation::create();
    track.setEnabledChangedCallback([observation, observer = CallbackObserver { observation.copyRef() }, state = Ref { state }](AudioTrackPrivate&, bool) mutable {
        // Copy everything this body needs onto the stack: on a build without the fix the
        // callable, and with it these captures, is destroyed while this body runs.
        Ref liveObservation = observation;
        Ref liveState = state;
        ++liveState->invocations;
        WTF::sleep(callbackBodyDuration);
        if (liveObservation->destroyed)
            ++liveState->destroyedWhileRunning;
    });
}

void installSelectedChangedCallback(VideoTrackPrivate& track, TrackCallbackRaceState& state)
{
    Ref observation = CallbackObservation::create();
    track.setSelectedChangedCallback([observation, observer = CallbackObserver { observation.copyRef() }, state = Ref { state }](VideoTrackPrivate&, bool) mutable {
        Ref liveObservation = observation;
        Ref liveState = state;
        ++liveState->invocations;
        WTF::sleep(callbackBodyDuration);
        if (liveObservation->destroyed)
            ++liveState->destroyedWhileRunning;
    });
}

} // namespace

TEST(AudioTrackPrivate, EnabledChangedCallbackSurvivesConcurrentReassignment)
{
    Ref track = TestAudioTrackPrivate::create();
    Ref state = TrackCallbackRaceState::create();

    installEnabledChangedCallback(track, state);

    Ref reassigningThread = Thread::create("AudioTrackPrivateCallbackReassigner"_s, [track = Ref { track }, state = Ref { state }] {
        while (!state->stopReassigning) {
            installEnabledChangedCallback(track, state);
            WTF::sleep(reassignmentInterval);
        }
    });

    bool enabled = false;
    for (unsigned i = 0; i < invocationCount; ++i) {
        enabled = !enabled;
        track->setEnabled(enabled);
    }

    state->stopReassigning = true;
    reassigningThread->waitForCompletion();

    EXPECT_EQ(invocationCount, state->invocations.load());
    EXPECT_EQ(0U, state->destroyedWhileRunning.load());
}

TEST(VideoTrackPrivate, SelectedChangedCallbackSurvivesConcurrentReassignment)
{
    Ref track = TestVideoTrackPrivate::create();
    Ref state = TrackCallbackRaceState::create();

    installSelectedChangedCallback(track, state);

    Ref reassigningThread = Thread::create("VideoTrackPrivateCallbackReassigner"_s, [track = Ref { track }, state = Ref { state }] {
        while (!state->stopReassigning) {
            installSelectedChangedCallback(track, state);
            WTF::sleep(reassignmentInterval);
        }
    });

    bool selected = false;
    for (unsigned i = 0; i < invocationCount; ++i) {
        selected = !selected;
        track->setSelected(selected);
    }

    state->stopReassigning = true;
    reassigningThread->waitForCompletion();

    EXPECT_EQ(invocationCount, state->invocations.load());
    EXPECT_EQ(0U, state->destroyedWhileRunning.load());
}

} // namespace TestWebKitAPI

#endif // ENABLE(VIDEO)
