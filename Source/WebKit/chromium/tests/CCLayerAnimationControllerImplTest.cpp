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

#include "cc/CCLayerAnimationControllerImpl.h"

#include "TransformOperations.h"
#include "cc/CCAnimationCurve.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace {

class FakeControllerClient : public CCLayerAnimationControllerImplClient {
public:
    FakeControllerClient() : m_opacity(0) { }
    virtual ~FakeControllerClient() { }

    virtual float opacity() const { return m_opacity; }
    virtual void setOpacity(float opacity) { m_opacity = opacity; }
    virtual const TransformationMatrix& transform() const { return m_transform; }
    virtual void setTransform(const TransformationMatrix& transform) { m_transform = transform; }
    virtual void animationControllerImplDidActivate(CCLayerAnimationControllerImpl* controller)
    {
        m_activeControllers.append(controller);
    }

    Vector<CCLayerAnimationControllerImpl*>& activeControllers() { return m_activeControllers; }

private:
    float m_opacity;
    TransformationMatrix m_transform;
    Vector<CCLayerAnimationControllerImpl*> m_activeControllers;
};

class FakeTransformTransition : public CCTransformAnimationCurve {
public:
    FakeTransformTransition(double duration) : m_duration(duration) { }
    virtual double duration() const { return m_duration; }
    virtual TransformOperations getValue(double time) const
    {
        return TransformOperations();
    }

private:
    double m_duration;
};

class FakeFloatTransition : public CCFloatAnimationCurve {
public:
    FakeFloatTransition(double duration, float from, float to)
        : m_duration(duration)
        , m_from(from)
        , m_to(to)
    {
    }

    virtual double duration() const { return m_duration; }
    virtual float getValue(double time) const
    {
        time /= m_duration;
        if (time >= 1)
            time = 1;
        return (1 - time) * m_from + time * m_to;
    }

private:
    double m_duration;
    float m_from;
    float m_to;
};

// Tests that transitioning opacity from 0 to 1 works as expected.
TEST(CCLayerAnimationControllerImplTest, TrivialTransition)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));

    controller->add(toAdd.release());
    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST(CCLayerAnimationControllerImplTest, TrivialQueuing)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST(CCLayerAnimationControllerImplTest, Interrupt)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    controller->add(toAdd.release());

    controller->animate(0.5); // second anim starts NOW.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(1.5);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is free.
TEST(CCLayerAnimationControllerImplTest, ScheduleTogetherWhenAPropertyIsBlocked)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeTransformTransition(1)), 1, CCActiveAnimation::Transform));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeTransformTransition(1)), 2, CCActiveAnimation::Transform));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 2, CCActiveAnimation::Opacity));

    controller->animate(0);
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1);
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The the float animation should have started at time 1 and should be done.
    controller->animate(2);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and another
// animation queued to start when the shorter animation finishes (should wait
// for both to finish).
TEST(CCLayerAnimationControllerImplTest, ScheduleTogetherWithAnAnimWaiting)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeTransformTransition(2)), 1, CCActiveAnimation::Transform));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 1, 0.5f)), 2, CCActiveAnimation::Opacity));

    // Anims with id 1 should both start now.
    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The opacity animation should have finished at time 1, but the group
    // of animations with id 1 don't finish until time 2 because of the length
    // of the transform animation.
    controller->animate(2);
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // The the second opacity animation should start at time 2 and should be
    // done by time 3
    controller->animate(3);
    EXPECT_EQ(0.5f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future.
TEST(CCLayerAnimationControllerImplTest, ScheduleAnimation)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2);
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that's interrupting a running animation.
TEST(CCLayerAnimationControllerImplTest, ScheduledAnimationInterruptsRunningAnimation)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0.5f, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    // First 2s opacity transition should start immediately.
    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());
    controller->animate(2);
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that interrupts a running animation
// and there is yet another animation queued to start later.
TEST(CCLayerAnimationControllerImplTest, ScheduledAnimationInterruptsRunningAnimationWithAnimInQueue)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(2, 0.5f, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 0.75f)), 3, CCActiveAnimation::Opacity));

    // First 2s opacity transition should start immediately.
    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());
    controller->animate(3);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(4);
    EXPECT_EQ(0.75f, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST(CCLayerAnimationControllerImplTest, TrivialLooping)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setIterations(3);
    controller->add(toAdd.release());

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1.75);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(2.25);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(2.75);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(3);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // Just be extra sure.
    controller->animate(4);
    EXPECT_EQ(1, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(CCLayerAnimationControllerImplTest, InfiniteLooping)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    OwnPtr<CCActiveAnimation> toAdd(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));
    toAdd->setIterations(-1);
    controller->add(toAdd.release());

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1.75);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());

    controller->animate(1073741824.25);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25f, dummy.opacity());
    controller->animate(1073741824.75);
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
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Paused, 0.5f);

    controller->animate(1024);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Running, 1024);

    controller->animate(1024.25);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
    controller->animate(1024.5);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
}

TEST(CCLayerAnimationControllerImplTest, AbortAGroupedAnimation)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    const int id = 1;
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeTransformTransition(1)), id, CCActiveAnimation::Transform));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(2, 0, 1)), id, CCActiveAnimation::Opacity));
    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 1, 0.75f)), 2, CCActiveAnimation::Opacity));

    controller->animate(0);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5f, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Aborted, 1);
    controller->animate(1);
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2);
    EXPECT_TRUE(!controller->hasActiveAnimation());
    EXPECT_EQ(0.75f, dummy.opacity());
}

// Tests that adding an animation to the controller calls the appropriate callback on the controller client
// (in this case, adding the controller to the list of active controller).
TEST(CCLayerAnimationControllerImplTest, DidActivate)
{
    FakeControllerClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controller(
        CCLayerAnimationControllerImpl::create(&dummy));

    EXPECT_EQ(size_t(0), dummy.activeControllers().size());

    controller->add(CCActiveAnimation::create(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));

    EXPECT_EQ(size_t(1), dummy.activeControllers().size());
    EXPECT_EQ(controller.get(), dummy.activeControllers()[0]);
}

} // namespace
