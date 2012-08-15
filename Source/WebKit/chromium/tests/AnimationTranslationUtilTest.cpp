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

#include "AnimationTranslationUtil.h"

#include "Animation.h"
#include "GraphicsLayer.h" // For KeyframeValueList
#include "IntSize.h"
#include "Matrix3DTransformOperation.h"
#include "RotateTransformOperation.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include <gtest/gtest.h>
#include <public/WebAnimation.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

bool animationCanBeTranslated(const KeyframeValueList& values, Animation* animation)
{
    IntSize boxSize;
    return createWebAnimation(values, animation, 0, 0, 0, boxSize);
}

TEST(AnimationTranslationUtilTest, createOpacityAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyOpacity);
    values.insert(new FloatAnimationValue(0, 0));
    values.insert(new FloatAnimationValue(duration, 1));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithBigRotation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(0, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(270, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_FALSE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithBigRotationAndEmptyTransformOperationList)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(270, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_FALSE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithRotationInvolvingNegativeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(-330, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(-320, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithSmallRotationInvolvingLargeAngles)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(RotateTransformOperation::create(270, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(RotateTransformOperation::create(360, TransformOperation::ROTATE));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createTransformAnimationWithSingularMatrix)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformationMatrix matrix1;
    TransformOperations operations1;
    operations1.operations().append(Matrix3DTransformOperation::create(matrix1));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformationMatrix matrix2;
    matrix2.setM11(0);
    TransformOperations operations2;
    operations2.operations().append(Matrix3DTransformOperation::create(matrix2));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);

    EXPECT_FALSE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createReversedAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionReverse);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternate);
    animation->setIterationCount(2);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

TEST(AnimationTranslationUtilTest, createReversedAlternatingAnimation)
{
    const double duration = 1;
    WebCore::KeyframeValueList values(AnimatedPropertyWebkitTransform);

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(0, &operations1));

    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TRANSLATE_X));
    values.insert(new TransformAnimationValue(duration, &operations2));

    RefPtr<Animation> animation = Animation::create();
    animation->setDuration(duration);
    animation->setDirection(Animation::AnimationDirectionAlternateReverse);
    animation->setIterationCount(2);

    EXPECT_TRUE(animationCanBeTranslated(values, animation.get()));
}

}

