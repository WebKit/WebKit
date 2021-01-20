/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <CoreGraphics/CGGeometry.h>
#include <WebKit/FullscreenTouchSecheuristic.h>

namespace WebKit {

static void configureSecheuristic(FullscreenTouchSecheuristic& secheuristic)
{
    secheuristic.setRampUpSpeed(0.25_s);
    secheuristic.setRampDownSpeed(1_s);
    secheuristic.setGamma(0.1);
    secheuristic.setGammaCutoff(0.08);
    secheuristic.setSize(CGSizeMake(100, 100));
    secheuristic.reset();
}

TEST(FullscreenTouchSecheuristic, Basic)
{
    using namespace WebKit;
    FullscreenTouchSecheuristic secheuristic;
    configureSecheuristic(secheuristic);

    ASSERT_EQ(secheuristic.scoreOfNextTouch({0, 0}, 0_s), 0);
    ASSERT_EQ(secheuristic.scoreOfNextTouch({0, 0}, .25_s), 0);
    ASSERT_GT(secheuristic.scoreOfNextTouch({50, 50}, .25_s), 0);

    secheuristic.reset();
    secheuristic.setXWeight(0);
    secheuristic.setYWeight(1);
    ASSERT_EQ(secheuristic.scoreOfNextTouch({0, 0}, 0_s), 0);
    ASSERT_EQ(secheuristic.scoreOfNextTouch({50, 0}, .25_s), 0);
    ASSERT_GT(secheuristic.scoreOfNextTouch({0, 50}, .25_s), 0);

    secheuristic.reset();
    secheuristic.setXWeight(1);
    secheuristic.setYWeight(0);
    ASSERT_EQ(secheuristic.scoreOfNextTouch({0, 0}, 0_s), 0);
    ASSERT_EQ(secheuristic.scoreOfNextTouch({0, 50}, .25_s), 0);
    ASSERT_GT(secheuristic.scoreOfNextTouch({50, 0}, .25_s), 0);
}

TEST(FullscreenTouchSecheuristic, TapOnceVsTapTwice)
{
    using namespace WebKit;
    FullscreenTouchSecheuristic twice;
    configureSecheuristic(twice);

    static const auto tapDelta = 0.25_s;
    static const auto tapDuration = 0.1_s;
    static const CGPoint location1 {0, 0};
    static const CGPoint location2 {50, 50};

    auto twiceScore = twice.scoreOfNextTouch(location1, 0_s);
    twiceScore = twice.scoreOfNextTouch(location1, tapDuration);

    ASSERT_EQ(twiceScore, 0);

    FullscreenTouchSecheuristic once;
    configureSecheuristic(once);

    auto onceScore = once.scoreOfNextTouch(location1, tapDuration);
    ASSERT_EQ(onceScore, 0);

    for (auto i = 0; i < 20; ++i) {
        twiceScore = twice.scoreOfNextTouch(location2, tapDelta);
        twiceScore = twice.scoreOfNextTouch(location2, tapDuration);

        twiceScore = twice.scoreOfNextTouch(location1, tapDelta);
        twiceScore = twice.scoreOfNextTouch(location1, tapDuration);
    }

    for (auto i = 0; i < 20; ++i) {
        onceScore = once.scoreOfNextTouch(location2, tapDelta + tapDuration);
        onceScore = once.scoreOfNextTouch(location1, tapDelta + tapDuration);
    }

    ASSERT_LT(abs(twiceScore - onceScore), 0.01);
}

TEST(FullscreenTouchSecheuristic, WKFullScreenViewControllerParameters)
{
    using namespace WebKit;

    static const auto tapDelta = 0.25_s;
    static const auto tapDuration = 0.1_s;
    static const CGPoint locations[] = {
        {1556, 1604},
        {1883, 1427},
        {1460, 1786},
        {1930, 1602},
        {760,  1430},
        {760,  1430},
        {200,  1780},
        {470,  1280},
        {430,  1610},
        {2080, 1430},
        {2080, 1430},
        {1930, 1600},
        {760,  1430},
        {2570, 1620},
    };

    FullscreenTouchSecheuristic secheuristic;
    secheuristic.setParameters(FullscreenTouchSecheuristicParameters::iosParameters());
    secheuristic.setSize(CGSizeMake(2732, 2048));
    secheuristic.reset();

    size_t successfulTaps = 0;
    for (auto& location : locations) {
        auto score = secheuristic.scoreOfNextTouch(location, tapDelta + tapDuration);
        if (score > secheuristic.requiredScore())
            break;
        ++successfulTaps;
    }

    ASSERT_LT(successfulTaps, std::size(locations));
}

}
