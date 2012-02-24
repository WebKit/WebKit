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
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());
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
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());
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

TEST(CCLayerAnimationControllerTest, syncNewAnimation)
{
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controllerImpl(CCLayerAnimationControllerImpl::create(&dummy));
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1);

    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());
}

TEST(CCLayerAnimationControllerTest, syncAnimationProperties)
{
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controllerImpl(CCLayerAnimationControllerImpl::create(&dummy));
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1);

    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Push an animation property change to the impl thread (should not cause an animation to be added).
    controller->pauseAnimation(0, 0);
    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::Paused, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());
}

TEST(CCLayerAnimationControllerTest, syncAbortedAnimation)
{
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controllerImpl(CCLayerAnimationControllerImpl::create(&dummy));
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1);

    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    controller->removeAnimation(0);

    // Abort an animation from the main thread.
    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
}

TEST(CCLayerAnimationControllerTest, syncCompletedAnimation)
{
    FakeLayerAnimationControllerImplClient dummy;
    OwnPtr<CCLayerAnimationControllerImpl> controllerImpl(CCLayerAnimationControllerImpl::create(&dummy));
    OwnPtr<CCLayerAnimationController> controller(CCLayerAnimationController::create());

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));

    addOpacityTransitionToController(*controller, 1, 0, 1);

    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_TRUE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_EQ(CCActiveAnimation::WaitingForTargetAvailability, controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity)->runState());

    // Step through the animation until it is finished. At the next sync, the main thread's animation should be cleared.
    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));
    controllerImpl->animate(0, *events);
    controllerImpl->animate(2, *events);

    EXPECT_FALSE(controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity));
    EXPECT_TRUE(controller->hasActiveAnimation());

    controller->synchronizeAnimations(controllerImpl.get());

    EXPECT_FALSE(controller->hasActiveAnimation());
}

} // namespace
