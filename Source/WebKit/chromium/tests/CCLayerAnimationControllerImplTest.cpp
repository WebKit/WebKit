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

#include "CCAnimationTestCommon.h"
#include "cc/CCLayerAnimationControllerImpl.h"
#include "cc/CCAnimationCurve.h"
#include "cc/CCAnimationEvents.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKitTests;

namespace {

PassOwnPtr<CCActiveAnimation> createActiveAnimation(PassOwnPtr<CCAnimationCurve> curve, int id, CCActiveAnimation::TargetProperty property)
{
    return CCActiveAnimation::create(curve, 0, id, property);
}

// Tests that transitioning opacity from 0 to 1 works as expected.
TEST(CCLayerAnimationControllerImplTest, TrivialTransition)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));

    controller->add(toAdd.release());
    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, *events);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST(CCLayerAnimationControllerImplTest, TrivialQueuing)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, *events);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST(CCLayerAnimationControllerImplTest, Interrupt)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    controller->add(toAdd.release());

    controller->animate(0.5, *events); // second anim starts NOW.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(1.5, *events);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is free.
TEST(CCLayerAnimationControllerImplTest, ScheduleTogetherWhenAPropertyIsBlocked)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), 1, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), 2, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, *events);
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, *events);
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The the float animation should have started at time 1 and should be done.
    controller->animate(2, *events);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and another
// animation queued to start when the shorter animation finishes (should wait
// for both to finish).
TEST(CCLayerAnimationControllerImplTest, ScheduleTogetherWithAnAnimWaiting)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(2)), 1, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));

    // Anims with id 1 should both start now.
    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The opacity animation should have finished at time 1, but the group
    // of animations with id 1 don't finish until time 2 because of the length
    // of the transform animation.
    controller->animate(2, *events);
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // The the second opacity animation should start at time 2 and should be
    // done by time 3
    controller->animate(3, *events);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future.
TEST(CCLayerAnimationControllerImplTest, ScheduleAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2, *events);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that's interrupting a running animation.
TEST(CCLayerAnimationControllerImplTest, ScheduledAnimationInterruptsRunningAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0.5f, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    // First 2s opacity transition should start immediately.
    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());
    controller->animate(2, *events);
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that interrupts a running animation
// and there is yet another animation queued to start later.
TEST(CCLayerAnimationControllerImplTest, ScheduledAnimationInterruptsRunningAnimationWithAnimInQueue)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0.5f, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 0.75f)), 3, CCActiveAnimation::Opacity));

    // First 2s opacity transition should start immediately.
    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());
    controller->animate(3, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(4, *events);
    EXPECT_EQ(0.75f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST(CCLayerAnimationControllerImplTest, TrivialLooping)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setIterations(3);
    controller->add(toAdd.release());

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1.75, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(2.25, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(2.75, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(3, *events);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // Just be extra sure.
    controller->animate(4, *events);
    EXPECT_EQ(1, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(CCLayerAnimationControllerImplTest, InfiniteLooping)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));
    toAdd->setIterations(-1);
    controller->add(toAdd.release());

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1.75, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());

    controller->animate(1073741824.25, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1073741824.75, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Aborted, 0.75f);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
}

// Test that pausing and resuming work as expected.
TEST(CCLayerAnimationControllerImplTest, PauseResume)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Paused, 0.5f);

    controller->animate(1024, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Running, 1024);

    controller->animate(1024.25, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(1024.5, *events);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
}

TEST(CCLayerAnimationControllerImplTest, AbortAGroupedAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), id, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), id, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.75f)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Aborted, 1);
    controller->animate(1, *events);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, *events);
    EXPECT_TRUE(!controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
}

} // namespace
