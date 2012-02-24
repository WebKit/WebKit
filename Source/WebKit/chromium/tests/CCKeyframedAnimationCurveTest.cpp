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

#include "cc/CCKeyframedAnimationCurve.h"

#include "Length.h"
#include "TranslateTransformOperation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace {

void expectTranslateX(double translateX, const TransformationMatrix& matrix)
{
    TransformationMatrix::DecomposedType decomposedType;
    matrix.decompose(decomposedType);
    EXPECT_FLOAT_EQ(translateX, decomposedType.translateX);
}

// Tests that a float animation with one keyframe works as expected.
TEST(CCKeyframedAnimationCurveTest, OneFloatKeyframe)
{
    Vector<CCFloatKeyframe> keyframes;
    keyframes.append(CCFloatKeyframe(0, 2));
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create(keyframes));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(2, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(2, curve->getValue(1));
    EXPECT_FLOAT_EQ(2, curve->getValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(CCKeyframedAnimationCurveTest, TwoFloatKeyframe)
{
    Vector<CCFloatKeyframe> keyframes;
    keyframes.append(CCFloatKeyframe(0, 2));
    keyframes.append(CCFloatKeyframe(1, 4));
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create(keyframes));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(4, curve->getValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(CCKeyframedAnimationCurveTest, ThreeFloatKeyframe)
{
    Vector<CCFloatKeyframe> keyframes;
    keyframes.append(CCFloatKeyframe(0, 2));
    keyframes.append(CCFloatKeyframe(1, 4));
    keyframes.append(CCFloatKeyframe(2, 8));
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create(keyframes));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve->getValue(2));
    EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(CCKeyframedAnimationCurveTest, RepeatedFloatKeyTimes)
{
    Vector<CCFloatKeyframe> keyframes;
    // A step function.
    keyframes.append(CCFloatKeyframe(0, 4));
    keyframes.append(CCFloatKeyframe(1, 4));
    keyframes.append(CCFloatKeyframe(1, 6));
    keyframes.append(CCFloatKeyframe(2, 6));
    OwnPtr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create(keyframes));

    EXPECT_FLOAT_EQ(4, curve->getValue(-1));
    EXPECT_FLOAT_EQ(4, curve->getValue(0));
    EXPECT_FLOAT_EQ(4, curve->getValue(0.5));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    float value = curve->getValue(1);
    EXPECT_TRUE(value >= 4 && value <= 6);

    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(6, curve->getValue(2));
    EXPECT_FLOAT_EQ(6, curve->getValue(3));
}


// Tests that a transform animation with one keyframe works as expected.
TEST(CCKeyframedAnimationCurveTest, OneTransformKeyframe)
{
    Vector<CCTransformKeyframe> keyframes;
    TransformOperations operations;
    operations.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    keyframes.append(CCTransformKeyframe(0, operations));
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create(keyframes));
    IntSize layerSize; // ignored
    expectTranslateX(2, curve->getValue(-1, layerSize));
    expectTranslateX(2, curve->getValue(0, layerSize));
    expectTranslateX(2, curve->getValue(0.5, layerSize));
    expectTranslateX(2, curve->getValue(1, layerSize));
    expectTranslateX(2, curve->getValue(2, layerSize));
}

// Tests that a transform animation with two keyframes works as expected.
TEST(CCKeyframedAnimationCurveTest, TwoTransformKeyframe)
{
    Vector<CCTransformKeyframe> keyframes;
    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    keyframes.append(CCTransformKeyframe(0, operations1));
    keyframes.append(CCTransformKeyframe(1, operations2));
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create(keyframes));
    IntSize layerSize; // ignored
    expectTranslateX(2, curve->getValue(-1, layerSize));
    expectTranslateX(2, curve->getValue(0, layerSize));
    expectTranslateX(3, curve->getValue(0.5, layerSize));
    expectTranslateX(4, curve->getValue(1, layerSize));
    expectTranslateX(4, curve->getValue(2, layerSize));
}

// Tests that a transform animation with three keyframes works as expected.
TEST(CCKeyframedAnimationCurveTest, ThreeTransformKeyframe)
{
    Vector<CCTransformKeyframe> keyframes;
    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations3;
    operations3.operations().append(TranslateTransformOperation::create(Length(8, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    keyframes.append(CCTransformKeyframe(0, operations1));
    keyframes.append(CCTransformKeyframe(1, operations2));
    keyframes.append(CCTransformKeyframe(2, operations3));
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create(keyframes));
    IntSize layerSize; // ignored
    expectTranslateX(2, curve->getValue(-1, layerSize));
    expectTranslateX(2, curve->getValue(0, layerSize));
    expectTranslateX(3, curve->getValue(0.5, layerSize));
    expectTranslateX(4, curve->getValue(1, layerSize));
    expectTranslateX(6, curve->getValue(1.5, layerSize));
    expectTranslateX(8, curve->getValue(2, layerSize));
    expectTranslateX(8, curve->getValue(3, layerSize));
}

// Tests that a transform animation with multiple keys at a given time works sanely.
TEST(CCKeyframedAnimationCurveTest, RepeatedTransformKeyTimes)
{
    Vector<CCTransformKeyframe> keyframes;
    // A step function.
    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations3;
    operations3.operations().append(TranslateTransformOperation::create(Length(6, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations4;
    operations4.operations().append(TranslateTransformOperation::create(Length(6, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    keyframes.append(CCTransformKeyframe(0, operations1));
    keyframes.append(CCTransformKeyframe(1, operations2));
    keyframes.append(CCTransformKeyframe(1, operations3));
    keyframes.append(CCTransformKeyframe(2, operations4));
    OwnPtr<CCKeyframedTransformAnimationCurve> curve(CCKeyframedTransformAnimationCurve::create(keyframes));

    IntSize layerSize; // ignored

    expectTranslateX(4, curve->getValue(-1, layerSize));
    expectTranslateX(4, curve->getValue(0, layerSize));
    expectTranslateX(4, curve->getValue(0.5, layerSize));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    TransformationMatrix value = curve->getValue(1, layerSize);
    TransformationMatrix::DecomposedType decomposedType;
    value.decompose(decomposedType);
    EXPECT_TRUE(decomposedType.translateX >= 4 && decomposedType.translateX <= 6);

    expectTranslateX(6, curve->getValue(1.5, layerSize));
    expectTranslateX(6, curve->getValue(2, layerSize));
    expectTranslateX(6, curve->getValue(3, layerSize));
}

// Tests that invalid lists of keyframes result in nothing being returned from ::create.
TEST(CCKeyframedAnimationCurveTest, InvalidKeyframes)
{
    // It is invalid to pass an empty vector of keyframes to create.
    Vector<CCTransformKeyframe> transformKeyframes;
    OwnPtr<CCKeyframedTransformAnimationCurve> transformCurve = CCKeyframedTransformAnimationCurve::create(transformKeyframes);
    EXPECT_FALSE(transformCurve.get());

    Vector<CCFloatKeyframe> floatKeyframes;
    OwnPtr<CCKeyframedFloatAnimationCurve> floatCurve = CCKeyframedFloatAnimationCurve::create(floatKeyframes);
    EXPECT_FALSE(floatCurve.get());

    // It is invalid to pass a vector of unsorted keyframes to create;
    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    TransformOperations operations2;
    operations2.operations().append(TranslateTransformOperation::create(Length(4, Fixed), Length(0, Fixed), TransformOperation::TRANSLATE_X));
    transformKeyframes.append(CCTransformKeyframe(1, operations1));
    transformKeyframes.append(CCTransformKeyframe(0, operations2));
    transformCurve = CCKeyframedTransformAnimationCurve::create(transformKeyframes);
    EXPECT_FALSE(transformCurve.get());

    floatKeyframes.append(CCFloatKeyframe(1, 2));
    floatKeyframes.append(CCFloatKeyframe(0, 4));
    floatCurve = CCKeyframedFloatAnimationCurve::create(floatKeyframes);
    EXPECT_FALSE(floatCurve.get());
}


} // namespace
