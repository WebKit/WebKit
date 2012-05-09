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

#include "cc/CCActiveAnimation.h"
#include "CCAnimationTestCommon.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebKitTests;
using namespace WebCore;

namespace {

PassOwnPtr<CCActiveAnimation> createActiveAnimation(int iterations)
{
    OwnPtr<CCActiveAnimation> toReturn(CCActiveAnimation::create(adoptPtr(new FakeFloatAnimationCurve), 0, 1, CCActiveAnimation::Opacity));
    toReturn->setIterations(iterations);
    return toReturn.release();
}

TEST(CCActiveAnimationTest, TrimTimeZeroIterations)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
}

TEST(CCActiveAnimationTest, TrimTimeOneIteration)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(2));
}

TEST(CCActiveAnimationTest, TrimTimeInfiniteIterations)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(-1));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1.5));
}

TEST(CCActiveAnimationTest, TrimTimeAlternating)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(-1));
    anim->setAlternatesDirection(true);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(0.75, anim->trimTimeToCurrentIteration(1.25));
}

TEST(CCActiveAnimationTest, TrimTimeStartTime)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(4));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(4.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(6));
}

TEST(CCActiveAnimationTest, TrimTimeTimeOffset)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setTimeOffset(4);
    anim->setStartTime(4);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1));
}

TEST(CCActiveAnimationTest, TrimTimePauseResume)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->setRunState(CCActiveAnimation::Paused, 0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->setRunState(CCActiveAnimation::Running, 1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(CCActiveAnimationTest, TrimTimeSuspendResume)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(0, anim->trimTimeToCurrentIteration(0));
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(0.5));
    anim->suspend(0.5);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    anim->resume(1024);
    EXPECT_EQ(0.5, anim->trimTimeToCurrentIteration(1024));
    EXPECT_EQ(1, anim->trimTimeToCurrentIteration(1024.5));
}

TEST(CCActiveAnimationTest, IsFinishedAtZeroIterations)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_TRUE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
}

TEST(CCActiveAnimationTest, IsFinishedAtOneIteration)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(-1));
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_TRUE(anim->isFinishedAt(1));
    EXPECT_TRUE(anim->isFinishedAt(2));
}

TEST(CCActiveAnimationTest, IsFinishedAtInfiniteIterations)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(-1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    EXPECT_FALSE(anim->isFinishedAt(0.5));
    EXPECT_FALSE(anim->isFinishedAt(1));
    EXPECT_FALSE(anim->isFinishedAt(1.5));
}

TEST(CCActiveAnimationTest, IsFinishedAtNotRunning)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(0));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinishedAt(0));
}

TEST(CCActiveAnimationTest, IsFinished)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Paused, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(CCActiveAnimationTest, IsFinishedNeedsSynchronizedStartTime)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->setRunState(CCActiveAnimation::Running, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Paused, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForNextTick, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForTargetAvailability, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::WaitingForStartTime, 2);
    EXPECT_FALSE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Finished, 0);
    EXPECT_TRUE(anim->isFinished());
    anim->setRunState(CCActiveAnimation::Aborted, 0);
    EXPECT_TRUE(anim->isFinished());
}

TEST(CCActiveAnimationTest, RunStateChangesIgnoredWhileSuspended)
{
    OwnPtr<CCActiveAnimation> anim(createActiveAnimation(1));
    anim->suspend(0);
    EXPECT_EQ(CCActiveAnimation::Paused, anim->runState());
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(CCActiveAnimation::Paused, anim->runState());
    anim->resume(0);
    anim->setRunState(CCActiveAnimation::Running, 0);
    EXPECT_EQ(CCActiveAnimation::Running, anim->runState());
}

} // namespace
