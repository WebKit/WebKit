/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteAnimationUtilities.h"

#if ENABLE(THREADED_ANIMATIONS)

#import <WebCore/AcceleratedEffectOffsetAnchor.h>
#import <WebCore/AcceleratedEffectOffsetDistance.h>
#import <WebCore/AcceleratedEffectOffsetPosition.h>
#import <WebCore/AcceleratedEffectOffsetRotate.h>
#import <WebCore/AcceleratedEffectOpacity.h>
#import <WebCore/AcceleratedEffectValues.h>
#import <WebCore/AnimationEffectTiming.h>
#import <WebCore/CompositeOperation.h>
#import <WebCore/Matrix3DTransformOperation.h>
#import <WebCore/MatrixTransformOperation.h>
#import <WebCore/PerspectiveTransformOperation.h>
#import <WebCore/RotateTransformOperation.h>
#import <WebCore/ScaleTransformOperation.h>
#import <WebCore/SkewTransformOperation.h>
#import <WebCore/TimingFunction.h>
#import <WebCore/TransformOperation.h>
#import <WebCore/TransformOperations.h>
#import <WebCore/TransformationMatrix.h>
#import <WebCore/TranslateTransformOperation.h>
#import <WebCore/WebAnimationTypes.h>

namespace WebKit {

String toStringForTesting(WebCore::AcceleratedEffectProperty property)
{
    switch (property) {
    case WebCore::AcceleratedEffectProperty::Invalid:
        return "invalid"_s;
    case WebCore::AcceleratedEffectProperty::Opacity:
        return "opacity"_s;
    case WebCore::AcceleratedEffectProperty::Transform:
        return "transform"_s;
    case WebCore::AcceleratedEffectProperty::Translate:
        return "translate"_s;
    case WebCore::AcceleratedEffectProperty::Rotate:
        return "rotate"_s;
    case WebCore::AcceleratedEffectProperty::Scale:
        return "scale"_s;
    case WebCore::AcceleratedEffectProperty::OffsetPath:
        return "offsetPath"_s;
    case WebCore::AcceleratedEffectProperty::OffsetDistance:
        return "offsetDistance"_s;
    case WebCore::AcceleratedEffectProperty::OffsetPosition:
        return "offsetPosition"_s;
    case WebCore::AcceleratedEffectProperty::OffsetAnchor:
        return "offsetAnchor"_s;
    case WebCore::AcceleratedEffectProperty::OffsetRotate:
        return "offsetRotate"_s;
    case WebCore::AcceleratedEffectProperty::Filter:
        return "filter"_s;
    case WebCore::AcceleratedEffectProperty::BackdropFilter:
        return "backdropFilter"_s;
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

Ref<JSON::Value> toJSONForTesting(std::optional<WebCore::WebAnimationTime> time)
{
    if (!time)
        return JSON::Value::null();
    TextStream ts;
    if (auto seconds = time->time())
        ts << *seconds << "s";
    else if (auto percentage = time->percentage())
        ts << *percentage << "%";
    else
        ASSERT_NOT_REACHED();
    return JSON::Value::create(ts.release());
}

Ref<JSON::Value> toJSONForTesting(std::optional<WebCore::CompositeOperation> compositeOperation)
{
    if (!compositeOperation)
        return JSON::Value::null();
    switch (*compositeOperation) {
    case WebCore::CompositeOperation::Replace:
        return JSON::Value::create(makeString("replace"_s));
    case WebCore::CompositeOperation::Add:
        return JSON::Value::create(makeString("add"_s));
    case WebCore::CompositeOperation::Accumulate:
        return JSON::Value::create(makeString("accumulate"_s));
    default:
        ASSERT_NOT_REACHED();
        return JSON::Value::null();
    }
}

Ref<JSON::Value> toJSONForTesting(const RefPtr<WebCore::TimingFunction>& timingFunction)
{
    if (timingFunction)
        return JSON::Value::create(timingFunction->cssText());
    return JSON::Value::null();
}

Ref<JSON::Object> toJSONForTesting(const WebCore::AcceleratedEffectValues& values, const OptionSet<WebCore::AcceleratedEffectProperty>& properties)
{
    auto convertTransformOperation = [](const WebCore::TransformOperation& operation) {
        TextStream ts;
        if (RefPtr matrixOperation = dynamicDowncast<WebCore::MatrixTransformOperation>(operation)) {
            auto matrix = matrixOperation->matrix();
            ts << "matrix(" << matrix.a() << ", " << matrix.b() << ", " << matrix.c() << ", " << matrix.d() << ", " << matrix.e() << ", " << matrix.f() << ")";
        } else if (RefPtr matrix3DOperation = dynamicDowncast<WebCore::Matrix3DTransformOperation>(operation)) {
            auto matrix = matrix3DOperation->matrix();
            ts << "matrix3d("
            << matrix.m11() << ", " << matrix.m12() << ", " << matrix.m13() << ", " << matrix.m14() << ", "
            << matrix.m21() << ", " << matrix.m22() << ", " << matrix.m23() << ", " << matrix.m24() << ", "
            << matrix.m31() << ", " << matrix.m32() << ", " << matrix.m33() << ", " << matrix.m34() << ", "
            << matrix.m41() << ", " << matrix.m42() << ", " << matrix.m43() << ", " << matrix.m44() << ")";
        } else if (RefPtr perspective = dynamicDowncast<WebCore::PerspectiveTransformOperation>(operation)) {
            ts << "perspective(";
            if (auto perspectiveValue = perspective->perspective())
                ts << *perspectiveValue;
            ts << ")";
        } else if (RefPtr rotate = dynamicDowncast<WebCore::RotateTransformOperation>(operation))
            ts << "rotate(" << rotate->angle() << "deg," << rotate->x() << "," << rotate->y() << "," << rotate->z() << ")";
        else if (RefPtr scale = dynamicDowncast<WebCore::ScaleTransformOperation>(operation))
            ts << "scale(" << scale->x() << "," << scale->y() << "," << scale->z() << ")";
        else if (RefPtr skew = dynamicDowncast<WebCore::SkewTransformOperation>(operation))
            ts << "skew(" << skew->angleX() << "," << skew->angleY() << ")";
        else if (RefPtr translate = dynamicDowncast<WebCore::TranslateTransformOperation>(operation))
            ts << "translate(" << translate->x() << "," << translate->y() << "," << translate->z() << ")";
        else
            ASSERT_NOT_REACHED();
        return ts.release();
    };

    auto convertFloatPoint = [](const WebCore::FloatPoint& point) {
        Ref object = JSON::Object::create();
        object->setDouble("x"_s, point.x());
        object->setDouble("y"_s, point.y());
        return object;
    };

    Ref convertedValues = JSON::Object::create();
    for (auto property : properties) {
        auto propertyName = toStringForTesting(property);
        switch (property) {
        case WebCore::AcceleratedEffectProperty::Opacity:
            convertedValues->setDouble(propertyName, values.opacity.value);
            break;
        case WebCore::AcceleratedEffectProperty::Rotate: {
            if (RefPtr rotate = dynamicDowncast<WebCore::RotateTransformOperation>(values.rotate)) {
                Ref object = JSON::Object::create();
                object->setDouble("x"_s, rotate->x());
                object->setDouble("y"_s, rotate->y());
                object->setDouble("z"_s, rotate->z());
                object->setDouble("angle"_s, rotate->angle());
                convertedValues->setObject(propertyName, WTF::move(object));
            } else
                convertedValues->setValue(propertyName, JSON::Value::null());
            break;
        }
        case WebCore::AcceleratedEffectProperty::Scale: {
            if (RefPtr scale = downcast<WebCore::ScaleTransformOperation>(values.scale)) {
                Ref object = JSON::Object::create();
                object->setDouble("x"_s, scale->x());
                object->setDouble("y"_s, scale->y());
                object->setDouble("z"_s, scale->z());
                convertedValues->setObject(propertyName, WTF::move(object));
            } else
                convertedValues->setValue(propertyName, JSON::Value::null());
            break;
        }
        case WebCore::AcceleratedEffectProperty::Transform: {
            if (values.transform.isEmpty())
                convertedValues->setValue(propertyName, JSON::Value::null());
            else {
                Ref convertedTransform = JSON::Array::create();
                for (auto& operation : values.transform)
                    convertedTransform->pushString(convertTransformOperation(operation.get()));
                convertedValues->setArray(propertyName, WTF::move(convertedTransform));
            }
            break;
        }
        case WebCore::AcceleratedEffectProperty::Translate: {
            if (RefPtr translate = downcast<WebCore::TranslateTransformOperation>(values.translate)) {
                Ref object = JSON::Object::create();
                object->setDouble("x"_s, translate->x());
                object->setDouble("y"_s, translate->y());
                object->setDouble("z"_s, translate->z());
                convertedValues->setObject(propertyName, WTF::move(object));
            } else
                convertedValues->setValue(propertyName, JSON::Value::null());
            break;
        }
        case WebCore::AcceleratedEffectProperty::OffsetDistance:
            convertedValues->setDouble(propertyName, values.offsetDistance.value);
            break;
        case WebCore::AcceleratedEffectProperty::OffsetPosition:
            WTF::switchOn(values.offsetPosition.value,
                [&](const WebCore::AcceleratedEffectOffsetPosition::Normal&) { convertedValues->setString(propertyName, "normal"_s); },
                [&](const WebCore::AcceleratedEffectOffsetPosition::Auto&) { convertedValues->setString(propertyName, "auto"_s); },
                [&](const WebCore::FloatPoint& point) { convertedValues->setObject(propertyName, convertFloatPoint(point)); }
            );
            break;
        case WebCore::AcceleratedEffectProperty::OffsetAnchor:
            if (auto anchor = values.offsetAnchor.value)
                convertedValues->setObject(propertyName, convertFloatPoint(*anchor));
            else
                convertedValues->setValue(propertyName, JSON::Value::null());
            break;
        case WebCore::AcceleratedEffectProperty::OffsetRotate:
            if (values.offsetRotate.hasAuto)
                convertedValues->setString(propertyName, "auto"_s);
            else
                convertedValues->setDouble(propertyName, values.offsetRotate.angle);
            break;
        case WebCore::AcceleratedEffectProperty::OffsetPath:
        case WebCore::AcceleratedEffectProperty::Filter:
        case WebCore::AcceleratedEffectProperty::BackdropFilter:
            // Not implemented.
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return convertedValues;
}

Ref<JSON::Object> toJSONForTesting(const WebCore::AnimationEffectTiming& timing)
{
    auto convertDirection = [](WebCore::PlaybackDirection direction) {
        switch (direction) {
        case WebCore::PlaybackDirection::Normal:
            return "normal"_s;
        case WebCore::PlaybackDirection::Reverse:
            return "reverse"_s;
        case WebCore::PlaybackDirection::Alternate:
            return "alternate"_s;
        case WebCore::PlaybackDirection::AlternateReverse:
            return "alternate-reverse"_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    auto convertFill = [](WebCore::FillMode fillMode) {
        switch (fillMode) {
        case WebCore::FillMode::None:
            return "none"_s;
        case WebCore::FillMode::Forwards:
            return "forwards"_s;
        case WebCore::FillMode::Backwards:
            return "backwards"_s;
        case WebCore::FillMode::Both:
            return "both"_s;
        case WebCore::FillMode::Auto:
            return "auto"_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    Ref object = JSON::Object::create();
    object->setString("direction"_s, convertDirection(timing.direction));
    object->setValue("easing"_s, toJSONForTesting(timing.timingFunction));
    object->setString("fill"_s, convertFill(timing.fill));
    object->setDouble("iterationStart"_s, timing.iterationStart);
    object->setDouble("iterations"_s, timing.iterations);
    object->setValue("startDelay"_s, WebKit::toJSONForTesting(timing.startDelay));
    object->setValue("endDelay"_s, WebKit::toJSONForTesting(timing.endDelay));
    object->setValue("iterationDuration"_s, WebKit::toJSONForTesting(timing.iterationDuration));
    object->setValue("activeDuration"_s, WebKit::toJSONForTesting(timing.activeDuration));
    object->setValue("endTime"_s, WebKit::toJSONForTesting(timing.endTime));
    return object;
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
