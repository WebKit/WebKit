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

#include "Helpers/Test.h"
#include <WebCore/SharedTimebase.h>
#include <wtf/MediaTime.h>
#include <wtf/MonotonicTime.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(SharedTimebase, Basic)
{
    auto writerTimebase = SharedTimebase::create();
    ASSERT_TRUE(writerTimebase);

    auto timebaseHandle = writerTimebase->createHandle();
    ASSERT_TRUE(timebaseHandle);

    auto now = MonotonicTime::now();
    auto fakeNow = now;
    auto reader = SharedTimebaseReader::create(WTF::move(*timebaseHandle), [&fakeNow] { return fakeNow; });
    ASSERT_TRUE(reader);

    writerTimebase->storeSnapshot({
        .currentTime = MediaTime::zeroTime(),
        .playbackRate = 1.0,
        .hostTime = now
    });

    fakeNow = now;
    EXPECT_EQ(reader->currentTime(), MediaTime::zeroTime());

    fakeNow = now + 1_s;
    EXPECT_EQ(reader->currentTime(), MediaTime::createWithSeconds(1_s));

    // Rate now change to 0.5s at now+1s.
    writerTimebase->storeSnapshot({
        .currentTime = MediaTime::createWithSeconds(1_s),
        .playbackRate = 0.5,
        .hostTime = now + 1_s
    });

    fakeNow = now + 2_s;
    EXPECT_EQ(reader->currentTime().toDouble(), 1.5);

    // Reading with a host time before the writer host time does not apply the playback rate estimation
    fakeNow = now - 1_s;
    EXPECT_EQ(reader->currentTime().toDouble(), 1.5);

    fakeNow = now + 20_s;
    EXPECT_EQ(reader->currentTime().toDouble(), (19_s).seconds() * 0.5 + 1); // Time was 1s at now+1 with a rate of 0.5.

    // High-water-mark clamp: a snapshot rewinding to zero must not make
    // currentTime() go backwards.
    auto highWater = reader->currentTime();
    writerTimebase->storeSnapshot({
        .currentTime = MediaTime::zeroTime(),
        .playbackRate = 0.0,
        .hostTime = now
    });
    fakeNow = now;
    EXPECT_EQ(reader->currentTime(), highWater);

    // resetForTiscontinuity drops the high-water-mark; the next read
    // observes the new (lower) snapshot.
    reader->resetForTimeDiscontinuity();
    EXPECT_EQ(reader->currentTime(), MediaTime::zeroTime());
}

}
