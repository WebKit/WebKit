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

#include "cc/CCLayerAnimationController.h"

#include "CCAnimationTestCommon.h"
#include "GraphicsLayer.h"
#include "Length.h"
#include "TranslateTransformOperation.h"
#include "cc/CCActiveAnimation.h"
#include "cc/CCAnimationCurve.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKitTests;

namespace {

void expectTranslateX(double translateX, const TransformationMatrix& matrix)
{
    TransformationMatrix::DecomposedType decomposedType;
    matrix.decompose(decomposedType);
    EXPECT_FLOAT_EQ(translateX, decomposedType.translateX);
}

PassOwnPtr<CCActiveAnimation> createActiveAnimation(PassOwnPtr<CCAnimationCurve> curve, int id, CCActiveAnimation::TargetProperty property)
{
    return CCActiveAnimation::create(curve, 0, id, property);
}

TEST(CCLayerAnimationControllerTest, createOpacityAnimation)
{
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyOpacity);
    values.insert(new FloatAnimationValue(0, 0));
    values.insert(new FloatAnimationValue(duration, 1));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    controller->addAnimation(values, boxSize, animation.get(), 0, 0, 0);

    EXPECT_TRUE(controller->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = controller->getActiveAnimation(0, CCActiveAnimation::Opacity);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Opacity, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Float, activeAnimation->curve()->type());

    const CCFloatAnimationCurve* curve = activeAnimation->curve()->toFloatAnimationCurve();
    EXPECT_TRUE(curve);

    EXPECT_EQ(0, curve->getValue(0));
    EXPECT_EQ(1, curve->getValue(duration));
}

TEST(CCLayerAnimationControllerTest, createTransformAnimation)
{
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    IntSize boxSize;
    controller->addAnimation(values, boxSize, animation.get(), 0, 0, 0);

    EXPECT_TRUE(controller->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = controller->getActiveAnimation(0, CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(2, curve->getValue(0, boxSize));
    expectTranslateX(4, curve->getValue(duration, boxSize));
}

TEST(CCLayerAnimationControllerTest, createReversedAnimation)
{
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionReverse);

    IntSize boxSize;
    controller->addAnimation(values, boxSize, animation.get(), 0, 0, 0);

    EXPECT_TRUE(controller->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = controller->getActiveAnimation(0, CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);

    EXPECT_EQ(1, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(4, curve->getValue(0, boxSize));
    expectTranslateX(2, curve->getValue(duration, boxSize));
}

TEST(CCLayerAnimationControllerTest, createAlternatingAnimation)
{
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternate);
    animation->setIterationCount(2);

    IntSize boxSize;
    controller->addAnimation(values, boxSize, animation.get(), 0, 0, 0);

    EXPECT_TRUE(controller->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = controller->getActiveAnimation(0, CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->alternatesDirection());

    EXPECT_EQ(2, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(2, curve->getValue(0, boxSize));
    expectTranslateX(4, curve->getValue(duration, boxSize));
}

TEST(CCLayerAnimationControllerTest, createReversedAlternatingAnimation)
{
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternateReverse);
    animation->setIterationCount(2);

    IntSize boxSize;
    controller->addAnimation(values, boxSize, animation.get(), 0, 0, 0);

    EXPECT_TRUE(controller->hasActiveAnimation());

    CCActiveAnimation* activeAnimation = controller->getActiveAnimation(0, CCActiveAnimation::Transform);
    EXPECT_TRUE(activeAnimation);
    EXPECT_TRUE(activeAnimation->alternatesDirection());

    EXPECT_EQ(2, activeAnimation->iterations());
    EXPECT_EQ(CCActiveAnimation::Transform, activeAnimation->targetProperty());

    EXPECT_EQ(CCAnimationCurve::Transform, activeAnimation->curve()->type());

    const CCTransformAnimationCurve* curve = activeAnimation->curve()->toTransformAnimationCurve();
    EXPECT_TRUE(curve);

    expectTranslateX(4, curve->getValue(0, boxSize));
    expectTranslateX(2, curve->getValue(duration, boxSize));
}

TEST(CCLayerAnimationControllerTest, syncNewAnimation)
{
    FakeLayerAnimationControllerClient dummyImpl;
    OwnPtr<CCLayerAnimationController> controllerImpl(CCLayerAnimationController::create(&dummyImpl));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());
}

// If an animation is started on the impl thread before it is ticked on the main
// thread, we must be sure to respect the synchronized start time.
TEST(CCLayerAnimationControllerTest, doNotClobberStartTimes)
{
    FakeLayerAnimationControllerClient dummyImpl;
    OwnPtr<CCLayerAnimationController> controllerImpl(CCLayerAnimationController::create(&dummyImpl));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    CCAnimationEventsVector events;
    controllerImpl->animate(1, &events);

    // Synchronize the start times.
    EXPECT_EQ(1u, events.size());
    controller->notifyAnimationStarted(events[0]);
    EXPECT_EQ(controller->getActiveAnimation(0, CCActiveAnimation::Opacity)->startTime(), controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->startTime());

    // Start the animation on the main thread. Should not affect the start time.
    controller->animate(1.5, 0);
    EXPECT_EQ(controller->getActiveAnimation(0, CCActiveAnimation::Opacity)->startTime(), controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->startTime());
}

TEST(CCLayerAnimationControllerTest, syncPauseAndResume)
{
    FakeLayerAnimationControllerClient dummyImpl;
    OwnPtr<CCLayerAnimationController> controllerImpl(CCLayerAnimationController::create(&dummyImpl));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Start the animations on each controller.
    CCAnimationEventsVector events;
    controllerImpl->animate(0, &events);
    controller->animate(0, 0);
    EXPECT_EQ(CCActiveAnimation::Running, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());
    EXPECT_EQ(CCActiveAnimation::Running, controller->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Pause the main-thread animation.
    controller->suspendAnimations(1);
    EXPECT_EQ(CCActiveAnimation::Paused, controller->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(CCActiveAnimation::Paused, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Resume the main-thread animation.
    controller->resumeAnimations(2);
    EXPECT_EQ(CCActiveAnimation::Running, controller->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // The pause run state change should make it to the impl thread controller.
    controller->pushAnimationUpdatesTo(controllerImpl.get());
    EXPECT_EQ(CCActiveAnimation::Running, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());
}


TEST(CCLayerAnimationControllerTest, doNotSyncFinishedAnimation)
{
    FakeLayerAnimationControllerClient dummyImpl;
    OwnPtr<CCLayerAnimationController> controllerImpl(CCLayerAnimationController::create(&dummyImpl));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create(&dummy));

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1, false);

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Notify main thread controller that the animation has started.
    CCAnimationEvent animationStartedEvent(CCAnimationEvent::Started, 0, 0, CCActiveAnimation::Opacity, 0);
    controller->notifyAnimationStarted(animationStartedEvent);

    // Force animation to complete on impl thread.
    controllerImpl->removeAnimation(0);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    controller->pushAnimationUpdatesTo(controllerImpl.get());

    // Even though the main thread has a 'new' animation, it should not be pushed because the animation has already completed on the impl thread.
    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
}

// Tests that transitioning opacity from 0 to 1 works as expected.
TEST(CCLayerAnimationControllerTest, TrivialTransition)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));

    controller->add(toAdd.release());
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests animations that are waiting for a synchronized start time do not finish.
TEST(CCLayerAnimationControllerTest, AnimationsWaitingForStartTimeDoNotFinishIfTheyWaitLongerToStartThanTheirDuration)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setNeedsSynchronizedStartTime(true);

    // We should pause at the first keyframe indefinitely waiting for that animation to start.
    controller->add(toAdd.release());
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    // Send the synchronized start time.
    controller->notifyAnimationStarted(CCAnimationEvent(CCAnimationEvent::Started, 0, 1, CCActiveAnimation::Opacity, 2));
    controller->animate(5, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests that two queued animations affecting the same property run in sequence.
TEST(CCLayerAnimationControllerTest, TrivialQueuing)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests interrupting a transition with another transition.
TEST(CCLayerAnimationControllerTest, Interrupt)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForNextTick, 0);
    controller->add(toAdd.release());

    // Since the animation was in the WaitingForNextTick state, it should start right in
    // this call to animate.
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(1.5, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together when only one property is free.
TEST(CCLayerAnimationControllerTest, ScheduleTogetherWhenAPropertyIsBlocked)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), 1, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), 2, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, events.get());
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The float animation should have started at time 1 and should be done.
    controller->animate(2, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling two animations to run together with different lengths and another
// animation queued to start when the shorter animation finishes (should wait
// for both to finish).
TEST(CCLayerAnimationControllerTest, ScheduleTogetherWithAnAnimWaiting)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(2)), 1, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.5)), 2, CCActiveAnimation::Opacity));

    // Animations with id 1 should both start now.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    // The opacity animation should have finished at time 1, but the group
    // of animations with id 1 don't finish until time 2 because of the length
    // of the transform animation.
    controller->animate(2, events.get());
    // Should not have started the float transition yet.
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // The second opacity animation should start at time 2 and should be done by time 3
    controller->animate(3, events.get());
    EXPECT_EQ(0.5, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future.
TEST(CCLayerAnimationControllerTest, ScheduleAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(1, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that's interrupting a running animation.
TEST(CCLayerAnimationControllerTest, ScheduledAnimationInterruptsRunningAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0.5, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    // First 2s opacity transition should start immediately.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_EQ(0, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Tests scheduling an animation to start in the future that interrupts a running animation
// and there is yet another animation queued to start later.
TEST(CCLayerAnimationControllerTest, ScheduledAnimationInterruptsRunningAnimationWithAnimInQueue)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), 1, CCActiveAnimation::Opacity));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0.5, 0)), 2, CCActiveAnimation::Opacity));
    toAdd->setRunState(CCActiveAnimation::WaitingForStartTime, 0);
    toAdd->setStartTime(1);
    controller->add(toAdd.release());

    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 0.75)), 3, CCActiveAnimation::Opacity));

    // First 2s opacity transition should start immediately.
    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    EXPECT_TRUE(controller->hasActiveAnimation());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());
    controller->animate(3, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(4, events.get());
    EXPECT_EQ(0.75, dummy.opacity());
    EXPECT_FALSE(controller->hasActiveAnimation());
}

// Test that a looping animation loops and for the correct number of iterations.
TEST(CCLayerAnimationControllerTest, TrivialLooping)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), 1, CCActiveAnimation::Opacity));
    toAdd->setIterations(3);
    controller->add(toAdd.release());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(2.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(2.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(3, events.get());
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());

    // Just be extra sure.
    controller->animate(4, events.get());
    EXPECT_EQ(1, dummy.opacity());
}

// Test that an infinitely looping animation does indeed go until aborted.
TEST(CCLayerAnimationControllerTest, InfiniteLooping)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    const int id = 1;
    OwnPtr<CCActiveAnimation> toAdd(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));
    toAdd->setIterations(-1);
    controller->add(toAdd.release());

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());

    controller->animate(1073741824.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.25, dummy.opacity());
    controller->animate(1073741824.75, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Aborted, 0.75);
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
}

// Test that pausing and resuming work as expected.
TEST(CCLayerAnimationControllerTest, PauseResume)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    const int id = 1;
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 0, 1)), id, CCActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(0.5, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Paused, 0.5);

    controller->animate(1024, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Running, 1024);

    controller->animate(1024.25, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
    controller->animate(1024.5, events.get());
    EXPECT_FALSE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
}

TEST(CCLayerAnimationControllerTest, AbortAGroupedAnimation)
{
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    FakeLayerAnimationControllerClient dummy;
    OwnPtr<CCLayerAnimationController> controller(
        CCLayerAnimationController::create(&dummy));

    const int id = 1;
    controller->add(createActiveAnimation(adoptPtr(new FakeTransformTransition(1)), id, CCActiveAnimation::Transform));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(2, 0, 1)), id, CCActiveAnimation::Opacity));
    controller->add(createActiveAnimation(adoptPtr(new FakeFloatTransition(1, 1, 0.75)), 2, CCActiveAnimation::Opacity));

    controller->animate(0, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0, dummy.opacity());
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(0.5, dummy.opacity());

    EXPECT_TRUE(controller->getActiveAnimation(id, CCActiveAnimation::Opacity));
    controller->getActiveAnimation(id, CCActiveAnimation::Opacity)->setRunState(CCActiveAnimation::Aborted, 1);
    controller->animate(1, events.get());
    EXPECT_TRUE(controller->hasActiveAnimation());
    EXPECT_EQ(1, dummy.opacity());
    controller->animate(2, events.get());
    EXPECT_TRUE(!controller->hasActiveAnimation());
    EXPECT_EQ(0.75, dummy.opacity());
}

} // namespace
