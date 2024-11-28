/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "TransformOperationsBuilder.h"

#include "CSSFunctionValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSTransformListValue.h"
#include "CSSValueList.h"
#include "CalculationValue.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PerspectiveTransformOperation.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "SkewTransformOperation.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"

namespace WebCore {
namespace Style {

static WebCore::Length resolveAsFloatPercentOrCalculatedLength(const CSSPrimitiveValue& primitiveValue, const CSSToLengthConversionData& conversionData)
{
    return primitiveValue.convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
}

static double resolveAsNumberAtIndex(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData, unsigned index)
{
    return downcast<CSSPrimitiveValue>(value[index]).resolveAsNumber(conversionData);
}

// MARK: Matrix

static Ref<TransformOperation> createMatrixTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
    // matrix() = matrix( <number>#{6} )

    auto zoom = conversionData.zoom();
    return MatrixTransformOperation::create(
        resolveAsNumberAtIndex(value, conversionData, 0),
        resolveAsNumberAtIndex(value, conversionData, 1),
        resolveAsNumberAtIndex(value, conversionData, 2),
        resolveAsNumberAtIndex(value, conversionData, 3),
        resolveAsNumberAtIndex(value, conversionData, 4) * zoom,
        resolveAsNumberAtIndex(value, conversionData, 5) * zoom
    );
}

static Ref<TransformOperation> createMatrix3dTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
    // matrix3d() = matrix3d( <number>#{16} )

    TransformationMatrix matrix(
        resolveAsNumberAtIndex(value, conversionData, 0),
        resolveAsNumberAtIndex(value, conversionData, 1),
        resolveAsNumberAtIndex(value, conversionData, 2),
        resolveAsNumberAtIndex(value, conversionData, 3),
        resolveAsNumberAtIndex(value, conversionData, 4),
        resolveAsNumberAtIndex(value, conversionData, 5),
        resolveAsNumberAtIndex(value, conversionData, 6),
        resolveAsNumberAtIndex(value, conversionData, 7),
        resolveAsNumberAtIndex(value, conversionData, 8),
        resolveAsNumberAtIndex(value, conversionData, 9),
        resolveAsNumberAtIndex(value, conversionData, 10),
        resolveAsNumberAtIndex(value, conversionData, 11),
        resolveAsNumberAtIndex(value, conversionData, 12),
        resolveAsNumberAtIndex(value, conversionData, 13),
        resolveAsNumberAtIndex(value, conversionData, 14),
        resolveAsNumberAtIndex(value, conversionData, 15)
    );
    matrix.zoom(conversionData.zoom());

    return Matrix3DTransformOperation::create(WTFMove(matrix));
}

// MARK: Rotate

static Ref<TransformOperation> createRotateTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
    // rotate() = rotate( [ <angle> | <zero> ] )

    double x = 0;
    double y = 0;
    double z = 1;
    double angle = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::Rotate);
}

static Ref<TransformOperation> createRotate3dTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    double x = downcast<CSSPrimitiveValue>(value[0]).resolveAsNumber(conversionData);
    double y = downcast<CSSPrimitiveValue>(value[1]).resolveAsNumber(conversionData);
    double z = downcast<CSSPrimitiveValue>(value[2]).resolveAsNumber(conversionData);
    double angle = downcast<CSSPrimitiveValue>(value[3]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::Rotate3D);
}

static Ref<TransformOperation> createRotateXTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
    // rotateX() = rotateX( [ <angle> | <zero> ] )

    double x = 1;
    double y = 0;
    double z = 0;
    double angle = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateX);
}

static Ref<TransformOperation> createRotateYTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
    // rotateY() = rotateY( [ <angle> | <zero> ] )

    double x = 0;
    double y = 1;
    double z = 0;
    double angle = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateY);
}

static Ref<TransformOperation> createRotateZTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
    // rotateZ() = rotateZ( [ <angle> | <zero> ] )

    double x = 0;
    double y = 0;
    double z = 1;
    double angle = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateZ);
}

// MARK: Skew

static Ref<TransformOperation> createSkewTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )

    double angleX = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);
    double angleY = value.length() > 1 ? downcast<CSSPrimitiveValue>(value[1]).resolveAsAngle(conversionData) : 0;

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::Skew);
}

static Ref<TransformOperation> createSkewXTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
    // skewX() = skewX( [ <angle> | <zero> ] )

    double angleX = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);
    double angleY = 0;

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::SkewX);
}

static Ref<TransformOperation> createSkewYTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
    // skewY() = skewY( [ <angle> | <zero> ] )

    double angleX = 0;
    double angleY = downcast<CSSPrimitiveValue>(value[0]).resolveAsAngle(conversionData);

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::SkewY);
}

// MARK: Scale

static Ref<TransformOperation> createScaleTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )

    double sx = downcast<CSSPrimitiveValue>(value[0]).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = value.length() > 1 ? downcast<CSSPrimitiveValue>(value[1]).valueDividingBy100IfPercentage<double>(conversionData) : sx;
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::Scale);
}

static Ref<TransformOperation> createScale3dTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    double sx = downcast<CSSPrimitiveValue>(value[0]).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = downcast<CSSPrimitiveValue>(value[1]).valueDividingBy100IfPercentage<double>(conversionData);
    double sz = downcast<CSSPrimitiveValue>(value[2]).valueDividingBy100IfPercentage<double>(conversionData);

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::Scale3D);
}

static Ref<TransformOperation> createScaleXTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
    // scaleX() = scaleX( [ <number> | <percentage> ] )

    double sx = downcast<CSSPrimitiveValue>(value[0]).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = 1;
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleX);
}

static Ref<TransformOperation> createScaleYTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
    // scaleY() = scaleY( [ <number> | <percentage> ] )

    double sx = 1;
    double sy = downcast<CSSPrimitiveValue>(value[0]).valueDividingBy100IfPercentage<double>(conversionData);
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleY);
}

static Ref<TransformOperation> createScaleZTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
    // scaleZ() = scaleZ( [ <number> | <percentage> ] )

    double sx = 1.0;
    double sy = 1.0;
    double sz = downcast<CSSPrimitiveValue>(value[0]).valueDividingBy100IfPercentage<double>(conversionData);

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleZ);
}

// MARK: Translate

static Ref<TransformOperation> createTranslateTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )

    auto tx = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[0]), conversionData);
    auto ty = value.length() > 1 ? resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[1]), conversionData) : WebCore::Length(0, LengthType::Fixed);
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::Translate);
}

static Ref<TransformOperation> createTranslate3dTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto tx = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[0]), conversionData);
    auto ty = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[1]), conversionData);
    auto tz = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[2]), conversionData);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::Translate3D);
}

static Ref<TransformOperation> createTranslateXTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
    // translateX() = translateX( <length-percentage> )

    auto tx = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[0]), conversionData);
    auto ty = WebCore::Length(0, LengthType::Fixed);
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateX);
}

static Ref<TransformOperation> createTranslateYTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
    // translateY() = translateY( <length-percentage> )

    auto tx = WebCore::Length(0, LengthType::Fixed);
    auto ty = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[0]), conversionData);
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateY);
}

static Ref<TransformOperation> createTranslateZTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
    // translateZ() = translateZ( <length> )

    auto tx = WebCore::Length(0, LengthType::Fixed);
    auto ty = WebCore::Length(0, LengthType::Fixed);
    auto tz = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(value[0]), conversionData);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateZ);
}

// MARK: Perspective

static Ref<TransformOperation> createPerspectiveTransformOperation(const CSSFunctionValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
    // perspective() = perspective( [ <length [0,∞]> | none ] )

    auto& parameter = downcast<CSSPrimitiveValue>(value[0]);
    if (parameter.isValueID()) {
        ASSERT(parameter.valueID() == CSSValueNone);
        return PerspectiveTransformOperation::create(std::nullopt);
    }

    if (parameter.isLength())
        return PerspectiveTransformOperation::create(resolveAsFloatPercentOrCalculatedLength(parameter, conversionData));

    // FIXME: Support for <number> parameters for `perspective` is a quirk that should go away when 3d transforms are finalized.
    return PerspectiveTransformOperation::create(WebCore::Length(clampToPositiveInteger(parameter.resolveAsNumber<double>(conversionData)), LengthType::Fixed));
}

// MARK: <transform-operation>

Ref<TransformOperation> createTransformOperation(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    auto& transform = downcast<CSSFunctionValue>(value);

    switch (transform.name()) {
    case CSSValueMatrix:
        return createMatrixTransformOperation(transform, conversionData);
    case CSSValueMatrix3d:
        return createMatrix3dTransformOperation(transform, conversionData);
    case CSSValueRotate:
        return createRotateTransformOperation(transform, conversionData);
    case CSSValueRotate3d:
        return createRotate3dTransformOperation(transform, conversionData);
    case CSSValueRotateX:
        return createRotateXTransformOperation(transform, conversionData);
    case CSSValueRotateY:
        return createRotateYTransformOperation(transform, conversionData);
    case CSSValueRotateZ:
        return createRotateZTransformOperation(transform, conversionData);
    case CSSValueSkew:
        return createSkewTransformOperation(transform, conversionData);
    case CSSValueSkewX:
        return createSkewXTransformOperation(transform, conversionData);
    case CSSValueSkewY:
        return createSkewYTransformOperation(transform, conversionData);
    case CSSValueScale:
        return createScaleTransformOperation(transform, conversionData);
    case CSSValueScale3d:
        return createScale3dTransformOperation(transform, conversionData);
    case CSSValueScaleX:
        return createScaleXTransformOperation(transform, conversionData);
    case CSSValueScaleY:
        return createScaleYTransformOperation(transform, conversionData);
    case CSSValueScaleZ:
        return createScaleZTransformOperation(transform, conversionData);
    case CSSValueTranslate:
        return createTranslateTransformOperation(transform, conversionData);
    case CSSValueTranslate3d:
        return createTranslate3dTransformOperation(transform, conversionData);
    case CSSValueTranslateX:
        return createTranslateXTransformOperation(transform, conversionData);
    case CSSValueTranslateY:
        return createTranslateYTransformOperation(transform, conversionData);
    case CSSValueTranslateZ:
        return createTranslateZTransformOperation(transform, conversionData);
    case CSSValuePerspective:
        return createPerspectiveTransformOperation(transform, conversionData);
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

TransformOperations createTransformOperations(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return TransformOperations { };
    }

    return TransformOperations { WTF::map(downcast<CSSTransformListValue>(value), [&](const auto& value) {
        return createTransformOperation(value, conversionData);
    }) };
}

// MARK: <translate>

RefPtr<TranslateTransformOperation> createTranslate(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-translate
    // none | <length-percentage> [ <length-percentage> <length>? ]?

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return nullptr;
    }

    auto& valueList = downcast<CSSValueList>(value);

    auto type = valueList.length() > 2 ? TransformOperation::Type::Translate3D : TransformOperation::Type::Translate;
    auto tx = resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(valueList[0]), conversionData);
    auto ty = valueList.length() > 1 ? resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(valueList[1]), conversionData) : WebCore::Length(0, LengthType::Fixed);
    auto tz = valueList.length() > 2 ? resolveAsFloatPercentOrCalculatedLength(downcast<CSSPrimitiveValue>(valueList[2]), conversionData) : WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), type);
}

// MARK: <rotate>

RefPtr<RotateTransformOperation> createRotate(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-rotate
    // none | <angle> | [ x | y | z | <number>{3} ] && <angle>

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return nullptr;
    }

    auto& valueList = downcast<CSSValueList>(value);

    // Only an angle was specified.
    if (valueList.length() == 1)
        return RotateTransformOperation::create(downcast<CSSPrimitiveValue>(valueList[0]).resolveAsAngle(conversionData), TransformOperation::Type::Rotate);

    // An axis identifier and angle were specified.
    if (valueList.length() == 2) {
        auto axis = downcast<CSSPrimitiveValue>(valueList[0]).valueID();
        auto angle = downcast<CSSPrimitiveValue>(valueList[1]).resolveAsAngle(conversionData);

        switch (axis) {
        case CSSValueX:
            return RotateTransformOperation::create(1, 0, 0, angle, TransformOperation::Type::RotateX);
        case CSSValueY:
            return RotateTransformOperation::create(0, 1, 0, angle, TransformOperation::Type::RotateY);
        case CSSValueZ:
            return RotateTransformOperation::create(0, 0, 1, angle, TransformOperation::Type::RotateZ);
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return RotateTransformOperation::create(angle, TransformOperation::Type::Rotate);
    }

    ASSERT(valueList.length() == 4);

    // An axis vector and angle were specified.
    double x = downcast<CSSPrimitiveValue>(valueList[0]).resolveAsNumber(conversionData);
    double y = downcast<CSSPrimitiveValue>(valueList[1]).resolveAsNumber(conversionData);
    double z = downcast<CSSPrimitiveValue>(valueList[2]).resolveAsNumber(conversionData);
    auto angle = downcast<CSSPrimitiveValue>(valueList[3]).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::Rotate3D);
}

// MARK: <scale>

RefPtr<ScaleTransformOperation> createScale(const CSSValue& value, const CSSToLengthConversionData& conversionData)
{
    // https://drafts.csswg.org/css-transforms-2/#propdef-scale
    // none | [ <number> | <percentage> ]{1,3}

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT_UNUSED(primitiveValue, primitiveValue->valueID() == CSSValueNone);
        return nullptr;
    }

    auto& valueList = downcast<CSSValueList>(value);

    auto sx = downcast<CSSPrimitiveValue>(valueList[0]).valueDividingBy100IfPercentage<double>(conversionData);
    auto sy = valueList.length() > 1 ? downcast<CSSPrimitiveValue>(valueList[1]).valueDividingBy100IfPercentage<double>(conversionData) : sx;
    auto sz = valueList.length() > 2 ? downcast<CSSPrimitiveValue>(valueList[2]).valueDividingBy100IfPercentage<double>(conversionData) : 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::Scale);
}

} // namespace Style
} // namespace WebCore
