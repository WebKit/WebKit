/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#include <public/WebAnimation.h>

#include <gtest/gtest.h>
#include <public/WebFloatAnimationCurve.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;

namespace {

// Linux/Win bots failed on this test.
// https://bugs.webkit.org/show_bug.cgi?id=90651
#if OS(WINDOWS)
#define MAYBE_DefaultSettings DISABLED_DefaultSettings
#elif OS(LINUX)
#define MAYBE_DefaultSettings DISABLED_DefaultSettings
#else
#define MAYBE_DefaultSettings DefaultSettings
#endif
TEST(WebAnimationTest, MAYBE_DefaultSettings)
{
    WebFloatAnimationCurve curve;
    OwnPtr<WebAnimation> animation = adoptPtr(WebAnimation::create(curve, WebAnimation::TargetPropertyOpacity));

    // Ensure that the defaults are correct.
    EXPECT_EQ(1, animation->iterations());
    EXPECT_EQ(0, animation->startTime());
    EXPECT_EQ(0, animation->timeOffset());
    EXPECT_FALSE(animation->alternatesDirection());
}

// Linux/Win bots failed on this test.
// https://bugs.webkit.org/show_bug.cgi?id=90651
#if OS(WINDOWS)
#define MAYBE_ModifiedSettings DISABLED_ModifiedSettings
#elif OS(LINUX)
#define MAYBE_ModifiedSettings DISABLED_ModifiedSettings
#else
#define MAYBE_ModifiedSettings ModifiedSettings
#endif
TEST(WebAnimationTest, MAYBE_ModifiedSettings)
{
    WebFloatAnimationCurve curve;
    OwnPtr<WebAnimation> animation = adoptPtr(WebAnimation::create(curve, WebAnimation::TargetPropertyOpacity));
    animation->setIterations(2);
    animation->setStartTime(2);
    animation->setTimeOffset(2);
    animation->setAlternatesDirection(true);

    EXPECT_EQ(2, animation->iterations());
    EXPECT_EQ(2, animation->startTime());
    EXPECT_EQ(2, animation->timeOffset());
    EXPECT_TRUE(animation->alternatesDirection());
}

} // namespace
