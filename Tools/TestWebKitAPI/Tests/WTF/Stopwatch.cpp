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
#include <wtf/Stopwatch.h>

#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace TestWebKitAPI {

TEST(WTF_Stopwatch, ResetForgetsTimesFromRunsBeforeTheReset)
{
    auto stopwatch = Stopwatch::create();
    stopwatch->start();
    WTF::sleep(5_ms);
    auto duringFirstRun = MonotonicTime::now();
    WTF::sleep(5_ms);
    stopwatch->stop();
    WTF::sleep(5_ms);
    stopwatch->start();
    WTF::sleep(5_ms);
    auto duringSecondRun = MonotonicTime::now();
    WTF::sleep(5_ms);
    EXPECT_TRUE(stopwatch->fromMonotonicTime(duringFirstRun));
    EXPECT_TRUE(stopwatch->fromMonotonicTime(duringSecondRun));

    // Reset while the stopwatch is running.
    stopwatch->reset();
    EXPECT_FALSE(stopwatch->isActive());
    EXPECT_EQ(stopwatch->elapsedTime().value(), 0.0);
    EXPECT_FALSE(stopwatch->fromMonotonicTime(duringFirstRun));
    EXPECT_FALSE(stopwatch->fromMonotonicTime(duringSecondRun));

    // Reset while the stopwatch is stopped.
    stopwatch->start();
    WTF::sleep(5_ms);
    auto duringThirdRun = MonotonicTime::now();
    WTF::sleep(5_ms);
    stopwatch->stop();
    EXPECT_TRUE(stopwatch->fromMonotonicTime(duringThirdRun));
    stopwatch->reset();
    EXPECT_FALSE(stopwatch->fromMonotonicTime(duringThirdRun));
    EXPECT_FALSE(stopwatch->fromMonotonicTime(duringFirstRun));
    EXPECT_FALSE(stopwatch->fromMonotonicTime(duringSecondRun));
}

TEST(WTF_Stopwatch, TimesAfterResetAreRelativeToTheNewStart)
{
    auto stopwatch = Stopwatch::create();
    stopwatch->start();
    WTF::sleep(5_ms);
    stopwatch->stop();
    stopwatch->reset();
    WTF::sleep(5_ms);

    auto beforeStart = MonotonicTime::now();
    stopwatch->start();
    auto afterStart = MonotonicTime::now();
    WTF::sleep(5_ms);
    auto duringRun = MonotonicTime::now();
    WTF::sleep(5_ms);
    stopwatch->stop();

    // The stopwatch started somewhere between beforeStart and afterStart. If the run before the
    // reset was still counted, the result would be at least 5 ms above the upper bound.
    auto elapsed = stopwatch->fromMonotonicTime(duringRun);
    ASSERT_TRUE(elapsed);
    EXPECT_TRUE(*elapsed >= duringRun - afterStart);
    EXPECT_TRUE(*elapsed <= duringRun - beforeStart);
}

} // namespace TestWebKitAPI
