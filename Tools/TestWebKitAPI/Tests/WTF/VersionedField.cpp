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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/VersionedField.h>

#include <array>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WTF_VersionedField, WatchThenPublish)
{
    VersionedField field { 5 };
    auto watched = field.watch();
    EXPECT_TRUE(watched.isWatched());
    EXPECT_EQ(watched.value, 5u);
    EXPECT_TRUE(field.tryUpdateWatched(watched, 9));
    EXPECT_EQ(field.value(), 9u);
}

TEST(WTF_VersionedField, WatchAfterPublishSeesPublishedValue)
{
    VersionedField field { 5 };
    EXPECT_TRUE(field.tryUpdateWatched(field.watch(), 9));
    auto watched = field.watch();
    EXPECT_EQ(watched.value, 9u);
    EXPECT_TRUE(field.tryUpdateWatched(watched, 12));
    EXPECT_EQ(field.value(), 12u);
}

TEST(WTF_VersionedField, FirstPublishWins)
{
    VersionedField field { 5 };
    auto first = field.watch();
    auto second = field.watch();
    EXPECT_EQ(first, second);
    EXPECT_TRUE(field.tryUpdateWatched(first, 9));
    EXPECT_FALSE(field.tryUpdateWatched(second, 12));
    EXPECT_EQ(field.value(), 9u);
}

TEST(WTF_VersionedField, WatchedMinimizeInvalidatesWatch)
{
    VersionedField field { 5 };
    auto watched = field.watch();
    field.minimize(3);
    EXPECT_EQ(field.value(), 3u);
    EXPECT_FALSE(field.tryUpdateWatched(watched, 9));
    EXPECT_EQ(field.value(), 3u);
}

TEST(WTF_VersionedField, WatchedNoOpMinimizeInvalidatesWatch)
{
    VersionedField field { 5 };
    auto watched = field.watch();
    field.minimize(7);
    EXPECT_EQ(field.value(), 5u);
    EXPECT_FALSE(field.tryUpdateWatched(watched, 9));
    EXPECT_EQ(field.value(), 5u);
}

TEST(WTF_VersionedField, WatchedNoOpMaximizeInvalidatesWatch)
{
    VersionedField field { 5 };
    auto watched = field.watch();
    field.maximize(3);
    EXPECT_EQ(field.value(), 5u);
    EXPECT_FALSE(field.tryUpdateWatched(watched, 1));
    EXPECT_EQ(field.value(), 5u);
}

TEST(WTF_VersionedField, NoOpMinimizeAfterPublishInvalidatesWatch)
{
    // A successful publish leaves the field watched, so a later no-op hard write still has to fail
    // anyone who watched before it.
    VersionedField field { 5 };
    EXPECT_TRUE(field.tryUpdateWatched(field.watch(), 9));
    auto watched = field.watch();
    field.minimize(12);
    EXPECT_EQ(field.value(), 9u);
    EXPECT_FALSE(field.tryUpdateWatched(watched, 15));
}

TEST(WTF_VersionedField, UnwatchedNoOpMinimizeDoesNotChangeVersion)
{
    VersionedField field { 5 };
    VersionedField reference { 5 };
    field.minimize(7);
    EXPECT_EQ(field.watch(), reference.watch());
}

TEST(WTF_VersionedField, UnwatchedMinimizeAndMaximize)
{
    VersionedField field { 5 };
    field.minimize(3);
    EXPECT_EQ(field.value(), 3u);
    field.maximize(8);
    EXPECT_EQ(field.value(), 8u);
    field.minimize(10);
    field.maximize(1);
    EXPECT_EQ(field.value(), 8u);
}

TEST(WTF_VersionedField, SetInvalidatesWatch)
{
    VersionedField field { 5 };
    auto watched = field.watch();
    field.set(5);
    EXPECT_FALSE(field.tryUpdateWatched(watched, 9));
    EXPECT_EQ(field.value(), 5u);

    auto rewatched = field.watch();
    EXPECT_NE(rewatched, watched);
    EXPECT_TRUE(field.tryUpdateWatched(rewatched, 9));
    EXPECT_EQ(field.value(), 9u);
}

TEST(WTF_VersionedField, SetChangesValueWhenUnwatched)
{
    VersionedField field { 5 };
    field.set(2);
    EXPECT_EQ(field.value(), 2u);
    field.set(7);
    EXPECT_EQ(field.value(), 7u);
}

// Models a directory of slots with a cursor below which no slot is available, the way BlockDirectory
// uses m_emptyCursor. Searchers scan from the cursor and try to move it past what they claim. Releasers
// make a slot available again and rewind the cursor to it. If a searcher could publish over a rewind it
// never saw, a released slot would end up below the cursor.
template<bool fullyFenced>
static void testConcurrentSearchersNeverHideReleasedSlots()
{
    static constexpr unsigned slotCount = 8;
    static constexpr unsigned threadCount = 4;
    static constexpr unsigned iterations = 100000;

    std::array<Atomic<bool>, slotCount> available;
    for (auto& slot : available)
        slot.store(true);
    VersionedField cursor { 0 };

    Vector<Ref<Thread>> threads;
    for (unsigned t = 0; t < threadCount; ++t) {
        threads.append(Thread::create("VersionedField test"_s, [&] {
            for (unsigned i = 0; i < iterations; ++i) {
                VersionedField::Snapshot watched;
                if constexpr (fullyFenced)
                    watched = cursor.watch<MemoryOrder::FullyFenced>();
                else {
                    watched = cursor.watch<MemoryOrder::Relaxed>();
                    WTF::storeLoadFence();
                }
                unsigned claimed = slotCount;
                for (unsigned slot = watched.value; slot < slotCount; ++slot) {
                    if (available[slot].load<MemoryOrder::Relaxed>() && available[slot].exchange<MemoryOrder::Acquire>(false)) {
                        claimed = slot;
                        break;
                    }
                }
                cursor.tryUpdateWatched<MemoryOrder::Relaxed>(watched, claimed == slotCount ? slotCount : claimed + 1);
                if (claimed == slotCount)
                    continue;

                if constexpr (fullyFenced)
                    available[claimed].exchange<MemoryOrder::FullyFenced>(true);
                else {
                    available[claimed].store<MemoryOrder::Release>(true);
                    WTF::storeLoadFence();
                }
                cursor.minimize<MemoryOrder::Relaxed>(claimed);
            }
        }));
    }
    for (auto& thread : threads)
        thread->waitForCompletion();

    // Every slot is available again, so nothing may be left behind the cursor.
    EXPECT_EQ(cursor.value(), 0u);
}

TEST(WTF_VersionedField, ConcurrentSearchersNeverHideReleasedSlots)
{
    testConcurrentSearchersNeverHideReleasedSlots<false>();
}

TEST(WTF_VersionedField, ConcurrentSearchersNeverHideReleasedSlotsFullyFenced)
{
    testConcurrentSearchersNeverHideReleasedSlots<true>();
}

} // namespace TestWebKitAPI
