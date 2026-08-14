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

// Tests for IPC::Event and IPC::Signal used in-process. The tests that serialize the Signal to
// another process live in EventTests.cpp.

#include "config.h"

#include "Helpers/Test.h"
#include "IPCEvent.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

namespace {

// Long enough that a wait that should be signalled does not time out on a loaded machine, but
// still bounded so that a broken implementation fails instead of hanging the test run.
constexpr Seconds longTimeout = 20_s;
// Short enough to keep the tests fast, used only where the wait is expected to time out.
constexpr Seconds shortTimeout = 50_ms;

}

TEST(IPCEventTests, CreateEventSignalPairWorks)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
}

TEST(IPCEventTests, SignalBeforeWaitIsRemembered)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    pair->signal.signal();
    EXPECT_TRUE(pair->event.wait());
}

TEST(IPCEventTests, SignalBeforeWaitForIsRemembered)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    pair->signal.signal();
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
}

TEST(IPCEventTests, WaitForTimesOutWithoutSignal)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    EXPECT_FALSE(pair->event.waitFor(shortTimeout));
    EXPECT_FALSE(pair->event.waitFor(IPC::Timeout::now()));
    // A timeout is not sticky: the event still works.
    pair->signal.signal();
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
}

TEST(IPCEventTests, OneSignalReleasesOneWait)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    pair->signal.signal();
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
    EXPECT_FALSE(pair->event.waitFor(shortTimeout));
}

TEST(IPCEventTests, SignalDoesNotBlockWhenNotWaitedFor)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    // Signalling must not block when the previous signal has not been waited for, the same as
    // Semaphore::signal(). The Event holds one signal, so the rest are dropped.
    auto start = MonotonicTime::now();
    for (int i = 0; i < 100; ++i)
        pair->signal.signal();
    EXPECT_LT(MonotonicTime::now() - start, shortTimeout);

    EXPECT_TRUE(pair->event.waitFor(longTimeout));
    EXPECT_FALSE(pair->event.waitFor(shortTimeout));

    // Dropping those signals did not break the pair.
    pair->signal.signal();
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
}

TEST(IPCEventTests, SignalFromOtherThreadReleasesWait)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    auto thread = Thread::create("IPCEventTests signal"_s, [signal = WTF::move(pair->signal)]() mutable {
        signal.signal();
    });
    EXPECT_TRUE(pair->event.wait());
    thread->waitForCompletion();
}

TEST(IPCEventTests, MovedToEventWorks)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    IPC::Event moveConstructed { WTF::move(pair->event) };
    pair->signal.signal();
    EXPECT_TRUE(moveConstructed.waitFor(longTimeout));

    auto pair2 = IPC::createEventSignalPair();
    ASSERT_TRUE(pair2.has_value());
    IPC::Event moveAssigned { WTF::move(pair2->event) };
    moveAssigned = WTF::move(moveConstructed);
    pair->signal.signal();
    EXPECT_TRUE(moveAssigned.waitFor(longTimeout));
}

TEST(IPCEventTests, MovingSignalDoesNotInterruptWait)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    std::optional<IPC::Signal> movedSignal;
    {
        IPC::Signal signal { WTF::move(pair->signal) };
        movedSignal.emplace(WTF::move(signal));
        // `signal` is destroyed here. It does not hold the send right anymore, so this must not
        // look like the peer going away.
    }
    EXPECT_FALSE(pair->event.waitFor(shortTimeout));
    movedSignal->signal();
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
}

#if PLATFORM(COCOA)

// The Signal is a Mach send right and the Event the matching receive right, so losing the last
// send right interrupts the wait. The non-Cocoa implementation is a plain semaphore pair, which
// does not have the property (see the FIXME in IPCEvent.h).

TEST(IPCEventTests, DestroyingSignalStopsWait)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    {
        auto signal = WTF::move(pair->signal);
        EXPECT_FALSE(pair->event.waitFor(shortTimeout));
    }
    EXPECT_FALSE(pair->event.wait());
}

TEST(IPCEventTests, DestroyingSignalStopsAllWaitsAfter)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    { auto signal = WTF::move(pair->signal); }

    // The no-senders notification is sent only once, so the Event has to remember it. Otherwise
    // wait() would block forever and waitFor() would burn the whole timeout on every call.
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(pair->event.wait());
        auto start = MonotonicTime::now();
        EXPECT_FALSE(pair->event.waitFor(longTimeout));
        EXPECT_LT(MonotonicTime::now() - start, shortTimeout);
    }
}

TEST(IPCEventTests, DestroyingSignalStopsWaitOnOtherThread)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    auto thread = Thread::create("IPCEventTests wait"_s, [&] {
        EXPECT_FALSE(pair->event.wait());
        // The interrupt is sticky also for the waits that come after it.
        EXPECT_FALSE(pair->event.wait());
    });
    { auto signal = WTF::move(pair->signal); }
    thread->waitForCompletion();
}

TEST(IPCEventTests, PendingSignalIsDeliveredBeforeInterrupt)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    {
        auto signal = WTF::move(pair->signal);
        signal.signal();
    }
    EXPECT_TRUE(pair->event.waitFor(longTimeout));
    EXPECT_FALSE(pair->event.wait());
    EXPECT_FALSE(pair->event.wait());
}

TEST(IPCEventTests, MovedFromEventDoesNotWait)
{
    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair.has_value());
    IPC::Event event { WTF::move(pair->event) };
    // The moved-from Event has no receive right, so it must not block.
    EXPECT_FALSE(pair->event.wait());
    EXPECT_FALSE(pair->event.waitFor(longTimeout));
    pair->signal.signal();
    EXPECT_TRUE(event.waitFor(longTimeout));
}

#endif // PLATFORM(COCOA)

} // namespace TestWebKitAPI
