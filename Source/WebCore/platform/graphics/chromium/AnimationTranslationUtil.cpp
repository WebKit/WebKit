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

#include "FloatSize.h"
#include "GraphicsLayer.h"
#include "IdentityTransformOperation.h"
#include "Length.h"
#include "LengthFunctions.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SkewTransformOperation.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"

#include <public/Platform.h>
#include <public/WebAnimation.h>
#include <public/WebAnimationCurve.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFloatAnimationCurve.h>
#include <public/WebTransformAnimationCurve.h>
#include <public/WebTransformOperations.h>
#include <public/WebTransformationMatrix.h>

#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

using namespace std;
using namespace WebKit;

namespace WebCore {

PassOwnPtr<WebTransformOperations> toWebTransformOperations(const TransformOperations& transformOperations, const FloatSize& boxSize)
{
    // We need to do a deep copy the transformOperations may contain ref pointers to TransformOperation objects.
    OwnPtr<WebTransformOperations> webTransformOperations = adoptPtr(Platform::current()->compositorSupport()->createTransformOperations());
    if (!webTransformOperations)
        return nullptr;
    for (size_t j = 0; j < transformOperations.size(); ++j) {
        TransformOperation::OperationType operationType = transformOperations.operations()[j]->getOperationType();
        switch (operationType) {
        case TransformOperation::SCALE_X:
        case TransformOperation::SCALE_Y:
        case TransformOperation::SCALE_Z:
        case TransformOperation::SCALE_3D:
        case TransformOperation::SCALE: {
            ScaleTransformOperation* transform = static_cast<ScaleTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendScale(transform->x(), transform->y(), transform->z());
            break;
        }
        case TransformOperation::TRANSLATE_X:
        case TransformOperation::TRANSLATE_Y:
        case TransformOperation::TRANSLATE_Z:
        case TransformOperation::TRANSLATE_3D:
        case TransformOperation::TRANSLATE: {
            TranslateTransformOperation* transform = static_cast<TranslateTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendTranslate(floatValueForLength(transform->x(), boxSize.width()), floatValueForLength(transform->y(), boxSize.height()), floatValueForLength(transform->z(), 1));
            break;
        }
        case TransformOperation::ROTATE_X:
        case TransformOperation::ROTATE_Y:
        case TransformOperation::ROTATE_3D:
        case TransformOperation::ROTATE: {
            RotateTransformOperation* transform = static_cast<RotateTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendRotate(transform->x(), transform->y(), transform->z(), transform->angle());
            break;
        }
        case TransformOperation::SKEW_X:
        case TransformOperation::SKEW_Y:
        case TransformOperation::SKEW: {
            SkewTransformOperation* transform = static_cast<SkewTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendSkew(transform->angleX(), transform->angleY());
            break;
        }
        case TransformOperation::MATRIX: {
            MatrixTransformOperation* transform = static_cast<MatrixTransformOperation*>(transformOperations.operations()[j].get());
            TransformationMatrix m = transform->matrix();
            webTransformOperations->appendMatrix(WebTransformationMatrix(m));
            break;
        }
        case TransformOperation::MATRIX_3D: {
            Matrix3DTransformOperation* transform = static_cast<Matrix3DTransformOperation*>(transformOperations.operations()[j].get());
            TransformationMatrix m = transform->matrix();
            webTransformOperations->appendMatrix(WebTransformationMatrix(m));
            break;
        }
        case TransformOperation::PERSPECTIVE: {
            PerspectiveTransformOperation* transform = static_cast<PerspectiveTransformOperation*>(transformOperations.operations()[j].get());
            webTransformOperations->appendPerspective(floatValueForLength(transform->perspective(), 0));
            break;
        }
        case TransformOperation::IDENTITY:
            webTransformOperations->appendIdentity();
            break;
        case TransformOperation::NONE:
            // Do nothing.
            break;
        } // switch
    } // for each operation

    return webTransformOperations.release();
}

template <class Value, class Keyframe, class Curve>
bool appendKeyframeWithStandardTimingFunction(Curve* curve, double keyTime, const Value* value, const Value* lastValue, WebKit::WebAnimationCurve::TimingFunctionType timingFunctionType, const FloatSize&)
{
    curve->add(Keyframe(keyTime, value->value()), timingFunctionType);
    return true;
}

template <class Value, class Keyframe, class Curve>
bool appendKeyframeWithCustomBezierTimingFunction(Curve* curve, double keyTime, const Value* value, const Value* lastValue, double x1, double y1, double x2, double y2, const FloatSize&)
{
    curve->add(Keyframe(keyTime, value->value()), x1, y1, x2, y2);
    return true;
}

bool isRotationType(TransformOperation::OperationType transformType)
{
    return transformType == TransformOperation::ROTATE
        || transformType == TransformOperation::ROTATE_X
        || transformType == TransformOperation::ROTATE_Y
        || transformType == TransformOperation::ROTATE_Z
        || transformType == TransformOperation::ROTATE_3D;
}

template <>
bool appendKeyframeWithStandardTimingFunction<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(WebTransformAnimationCurve* curve, double keyTime, const TransformAnimationValue* value, const TransformAnimationValue* lastValue, WebKit::WebAnimationCurve::TimingFunctionType timingFunctionType, const FloatSize& boxSize)
{
    bool canBlend = !lastValue;
    OwnPtr<WebTransformOperations> operations(toWebTransformOperations(*value->value(), boxSize));
    if (!operations)
        return false;
    if (!canBlend) {
        OwnPtr<WebTransformOperations> lastOperations(toWebTransformOperations(*lastValue->value(), boxSize));
        if (!lastOperations)
            return false;
        canBlend = lastOperations->canBlendWith(*operations);
    }
    if (canBlend) {
        curve->add(WebTransformKeyframe(keyTime, operations.leakPtr()), timingFunctionType);
        return true;
    }
    return false;
}

template <>
bool appendKeyframeWithCustomBezierTimingFunction<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(WebTransformAnimationCurve* curve, double keyTime, const TransformAnimationValue* value, const TransformAnimationValue* lastValue, double x1, double y1, double x2, double y2, const FloatSize& boxSize)
{
    bool canBlend = !lastValue;
    OwnPtr<WebTransformOperations> operations(toWebTransformOperations(*value->value(), boxSize));
    if (!operations)
        return false;
    if (!canBlend) {
        OwnPtr<WebTransformOperations> lastOperations(toWebTransformOperations(*lastValue->value(), boxSize));
        if (!lastOperations)
            return false;
        canBlend = lastOperations->canBlendWith(*operations);
    }
    if (canBlend) {
        curve->add(WebTransformKeyframe(keyTime, operations.leakPtr()), x1, y1, x2, y2);
        return true;
    }
    return false;
}

template <class Value, class Keyframe, class Curve>
PassOwnPtr<WebKit::WebAnimation> createWebAnimation(const KeyframeValueList& valueList, const Animation* animation, int animationId, double timeOffset, Curve* curve, WebKit::WebAnimation::TargetProperty targetProperty, const FloatSize& boxSize)
{
    bool alternate = false;
    bool reverse = false;
    if (animation && animation->isDirectionSet()) {
        Animation::AnimationDirection direction = animation->direction();
        if (direction == Animation::AnimationDirectionAlternate || direction == Animation::AnimationDirectionAlternateReverse)
            alternate = true;
        if (direction == Animation::AnimationDirectionReverse || direction == Animation::AnimationDirectionAlternateReverse)
            reverse = true;
    }

    for (size_t i = 0; i < valueList.size(); i++) {
        size_t index = reverse ? valueList.size() - i - 1 : i;
        const Value* originalValue = static_cast<const Value*>(valueList.at(index));
        const Value* lastOriginalValue = 0;
        if (valueList.size() > 1 && ((reverse && index + 1 < valueList.size()) || (!reverse && index > 0)))
            lastOriginalValue = static_cast<const Value*>(valueList.at(reverse ? index + 1 : index - 1));

        const TimingFunction* originalTimingFunction = originalValue->timingFunction();

        // If there hasn't been a timing function associated with this keyframe, use the
        // animation's timing function, if we have one.
        if (!originalTimingFunction && animation->isTimingFunctionSet())
            originalTimingFunction = animation->timingFunction().get();

        // Ease is the default timing function.
        WebKit::WebAnimationCurve::TimingFunctionType timingFunctionType = WebKit::WebAnimationCurve::TimingFunctionTypeEase;

        bool isUsingCustomBezierTimingFunction = false;
        double x1 = 0;
        double y1 = 0;
        double x2 = 1;
        double y2 = 1;

        if (originalTimingFunction) {
            switch (originalTimingFunction->type()) {
            case TimingFunction::StepsFunction:
                // FIXME: add support for steps timing function.
                return nullptr;
            case TimingFunction::LinearFunction:
                timingFunctionType = WebKit::WebAnimationCurve::TimingFunctionTypeLinear;
                break;
            case TimingFunction::CubicBezierFunction:
                const CubicBezierTimingFunction* originalBezierTimingFunction = static_cast<const CubicBezierTimingFunction*>(originalTimingFunction);
                isUsingCustomBezierTimingFunction = true;
                x1 = originalBezierTimingFunction->x1();
                y1 = originalBezierTimingFunction->y1();
                x2 = originalBezierTimingFunction->x2();
                y2 = originalBezierTimingFunction->y2();
                break;
            } // switch
        }

        double duration = (animation && animation->isDurationSet()) ? animation->duration() : 1;
        double keyTime = originalValue->keyTime() * duration;

        if (reverse)
            keyTime = duration - keyTime;

        bool addedKeyframe = false;
        if (isUsingCustomBezierTimingFunction)
            addedKeyframe = appendKeyframeWithCustomBezierTimingFunction<Value, Keyframe, Curve>(curve, keyTime, originalValue, lastOriginalValue, x1, y1, x2, y2, boxSize);
        else
            addedKeyframe = appendKeyframeWithStandardTimingFunction<Value, Keyframe, Curve>(curve, keyTime, originalValue, lastOriginalValue, timingFunctionType, boxSize);
        if (!addedKeyframe)
            return nullptr;
    }

    OwnPtr<WebKit::WebAnimation> webAnimation = adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, targetProperty, animationId));

    int iterations = (animation && animation->isIterationCountSet()) ? animation->iterationCount() : 1;
    webAnimation->setIterations(iterations);
    webAnimation->setAlternatesDirection(alternate);

    // If timeOffset > 0, then the animation has started in the past.
    webAnimation->setTimeOffset(timeOffset);

    return webAnimation.release();
}

PassOwnPtr<WebKit::WebAnimation> createWebAnimation(const KeyframeValueList& values, const Animation* animation, int animationId, double timeOffset, const FloatSize& boxSize)
{


    if (values.property() == AnimatedPropertyWebkitTransform) {
        OwnPtr<WebTransformAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createTransformAnimationCurve());
        return createWebAnimation<TransformAnimationValue, WebTransformKeyframe, WebTransformAnimationCurve>(values, animation, animationId, timeOffset, curve.get(), WebKit::WebAnimation::TargetPropertyTransform, FloatSize(boxSize));
    }

    if (values.property() == AnimatedPropertyOpacity) {
        OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFloatAnimationCurve());
        return createWebAnimation<FloatAnimationValue, WebFloatKeyframe, WebFloatAnimationCurve>(values, animation, animationId, timeOffset, curve.get(), WebKit::WebAnimation::TargetPropertyOpacity, FloatSize());
    }

    return nullptr;
}

} // namespace WebCore
