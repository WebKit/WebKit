/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleTransformFunction.h"

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
#include "StyleBuilderChecking.h"
#include "StyleBuilderConverter.h"
#include "StyleExtractorConverter.h"
#include "StyleExtractorSerializer.h"
#include "StyleInterpolationContext.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: Matrix

static RefPtr<TransformOperation> createMatrixTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
    // matrix() = matrix( <number>#{6} )

    auto function = requiredFunctionDowncast<CSSValueMatrix, CSSPrimitiveValue, 6>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    auto zoom = conversionData.zoom();
    return MatrixTransformOperation::create(
        function->item(0).resolveAsNumber(conversionData),
        function->item(1).resolveAsNumber(conversionData),
        function->item(2).resolveAsNumber(conversionData),
        function->item(3).resolveAsNumber(conversionData),
        function->item(4).resolveAsNumber(conversionData) * zoom,
        function->item(5).resolveAsNumber(conversionData) * zoom
    );
}

static RefPtr<TransformOperation> createMatrix3dTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
    // matrix3d() = matrix3d( <number>#{16} )

    auto function = requiredFunctionDowncast<CSSValueMatrix3d, CSSPrimitiveValue, 16>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    TransformationMatrix matrix(
        function->item(0).resolveAsNumber(conversionData),
        function->item(1).resolveAsNumber(conversionData),
        function->item(2).resolveAsNumber(conversionData),
        function->item(3).resolveAsNumber(conversionData),
        function->item(4).resolveAsNumber(conversionData),
        function->item(5).resolveAsNumber(conversionData),
        function->item(6).resolveAsNumber(conversionData),
        function->item(7).resolveAsNumber(conversionData),
        function->item(8).resolveAsNumber(conversionData),
        function->item(9).resolveAsNumber(conversionData),
        function->item(10).resolveAsNumber(conversionData),
        function->item(11).resolveAsNumber(conversionData),
        function->item(12).resolveAsNumber(conversionData),
        function->item(13).resolveAsNumber(conversionData),
        function->item(14).resolveAsNumber(conversionData),
        function->item(15).resolveAsNumber(conversionData)
    );
    matrix.zoom(conversionData.zoom());

    return Matrix3DTransformOperation::create(WTFMove(matrix));
}

// MARK: Rotate

static RefPtr<TransformOperation> createRotateTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
    // rotate() = rotate( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double x = 0;
    double y = 0;
    double z = 1;
    double angle = function->item(0).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::Rotate);
}

static RefPtr<TransformOperation> createRotate3dTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotate3d, CSSPrimitiveValue, 4>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double x = function->item(0).resolveAsNumber(conversionData);
    double y = function->item(1).resolveAsNumber(conversionData);
    double z = function->item(2).resolveAsNumber(conversionData);
    double angle = function->item(3).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::Rotate3D);
}

static RefPtr<TransformOperation> createRotateXTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
    // rotateX() = rotateX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double x = 1;
    double y = 0;
    double z = 0;
    double angle = function->item(0).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateX);
}

static RefPtr<TransformOperation> createRotateYTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
    // rotateY() = rotateY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double x = 0;
    double y = 1;
    double z = 0;
    double angle = function->item(0).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateY);
}

static RefPtr<TransformOperation> createRotateZTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
    // rotateZ() = rotateZ( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double x = 0;
    double y = 0;
    double z = 1;
    double angle = function->item(0).resolveAsAngle(conversionData);

    return RotateTransformOperation::create(x, y, z, angle, TransformOperation::Type::RotateZ);
}

// MARK: Skew

static RefPtr<TransformOperation> createSkewTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )

    auto function = requiredFunctionDowncast<CSSValueSkew, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double angleX = function->item(0).resolveAsAngle(conversionData);
    double angleY = function->size() > 1 ? function->item(1).resolveAsAngle(conversionData) : 0;

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::Skew);
}

static RefPtr<TransformOperation> createSkewXTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
    // skewX() = skewX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueSkewX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double angleX = function->item(0).resolveAsAngle(conversionData);
    double angleY = 0;

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::SkewX);
}

static RefPtr<TransformOperation> createSkewYTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
    // skewY() = skewY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueSkewY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double angleX = 0;
    double angleY = function->item(0).resolveAsAngle(conversionData);

    return SkewTransformOperation::create(angleX, angleY, TransformOperation::Type::SkewY);
}

// MARK: Scale

static RefPtr<TransformOperation> createScaleTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )

    auto function = requiredFunctionDowncast<CSSValueScale, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double sx = function->item(0).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = function->size() > 1 ? function->item(1).valueDividingBy100IfPercentage<double>(conversionData) : sx;
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::Scale);
}

static RefPtr<TransformOperation> createScale3dTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    auto function = requiredFunctionDowncast<CSSValueScale3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double sx = function->item(0).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = function->item(1).valueDividingBy100IfPercentage<double>(conversionData);
    double sz = function->item(2).valueDividingBy100IfPercentage<double>(conversionData);

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::Scale3D);
}

static RefPtr<TransformOperation> createScaleXTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
    // scaleX() = scaleX( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double sx = function->item(0).valueDividingBy100IfPercentage<double>(conversionData);
    double sy = 1;
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleX);
}

static RefPtr<TransformOperation> createScaleYTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
    // scaleY() = scaleY( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double sx = 1;
    double sy = function->item(0).valueDividingBy100IfPercentage<double>(conversionData);
    double sz = 1;

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleY);
}

static RefPtr<TransformOperation> createScaleZTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
    // scaleZ() = scaleZ( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& conversionData = state.cssToLengthConversionData();

    double sx = 1.0;
    double sy = 1.0;
    double sz = function->item(0).valueDividingBy100IfPercentage<double>(conversionData);

    return ScaleTransformOperation::create(sx, sy, sz, TransformOperation::Type::ScaleZ);
}

// MARK: Translate

static RefPtr<TransformOperation> createTranslateTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )

    auto function = requiredFunctionDowncast<CSSValueTranslate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(0));
    auto ty = function->size() > 1 ? BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(1)) : WebCore::Length(0, LengthType::Fixed);
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::Translate);
}

static RefPtr<TransformOperation> createTranslate3dTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto function = requiredFunctionDowncast<CSSValueTranslate3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto tx = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(0));
    auto ty = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(1));
    auto tz = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(2));

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::Translate3D);
}

static RefPtr<TransformOperation> createTranslateXTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
    // translateX() = translateX( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueTranslateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(0));
    auto ty = WebCore::Length(0, LengthType::Fixed);
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateX);
}

static RefPtr<TransformOperation> createTranslateYTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
    // translateY() = translateY( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueTranslateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = WebCore::Length(0, LengthType::Fixed);
    auto ty = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(0));
    auto tz = WebCore::Length(0, LengthType::Fixed);

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateY);
}

static RefPtr<TransformOperation> createTranslateZTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
    // translateZ() = translateZ( <length> )

    auto function = requiredFunctionDowncast<CSSValueTranslateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = WebCore::Length(0, LengthType::Fixed);
    auto ty = WebCore::Length(0, LengthType::Fixed);
    auto tz = BuilderConverter::convertLengthUsingDoubleNoClamping(state, function->item(0));

    return TranslateTransformOperation::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformOperation::Type::TranslateZ);
}

// MARK: Perspective

static RefPtr<TransformOperation> createPerspectiveTransformOperation(BuilderState& state, const CSSFunctionValue& value)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
    // perspective() = perspective( [ <length [0,∞]> | none ] )

    auto function = requiredFunctionDowncast<CSSValuePerspective, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& parameter = function->item(0);
    if (parameter.isValueID()) {
        ASSERT(parameter.valueID() == CSSValueNone);
        return PerspectiveTransformOperation::create(std::nullopt);
    }

    if (parameter.isLength())
        return PerspectiveTransformOperation::create(parameter.resolveAsLength<float>(state.cssToLengthConversionData()));

    // FIXME: Support for <number> parameters for `perspective` is a quirk that should go away when 3d transforms are finalized.
    return PerspectiveTransformOperation::create(clampToPositiveInteger(parameter.resolveAsNumber<double>(state.cssToLengthConversionData())));
}

// MARK: - Conversion

auto CSSValueConversion<TransformFunction>::operator()(BuilderState& state, const CSSValue& value) -> TransformFunction
{
    auto transform = requiredDowncast<CSSFunctionValue>(state, value);
    if (!transform)
        return TransformFunction { MatrixTransformOperation::createIdentity() };

    auto makeFunction = [](RefPtr<TransformOperation>&& operation) {
        if (!operation)
            return TransformFunction { MatrixTransformOperation::createIdentity() };
        return TransformFunction { operation.releaseNonNull() };
    };

    switch (transform->name()) {
    case CSSValueMatrix:
        return makeFunction(createMatrixTransformOperation(state, *transform));
    case CSSValueMatrix3d:
        return makeFunction(createMatrix3dTransformOperation(state, *transform));
    case CSSValueRotate:
        return makeFunction(createRotateTransformOperation(state, *transform));
    case CSSValueRotate3d:
        return makeFunction(createRotate3dTransformOperation(state, *transform));
    case CSSValueRotateX:
        return makeFunction(createRotateXTransformOperation(state, *transform));
    case CSSValueRotateY:
        return makeFunction(createRotateYTransformOperation(state, *transform));
    case CSSValueRotateZ:
        return makeFunction(createRotateZTransformOperation(state, *transform));
    case CSSValueSkew:
        return makeFunction(createSkewTransformOperation(state, *transform));
    case CSSValueSkewX:
        return makeFunction(createSkewXTransformOperation(state, *transform));
    case CSSValueSkewY:
        return makeFunction(createSkewYTransformOperation(state, *transform));
    case CSSValueScale:
        return makeFunction(createScaleTransformOperation(state, *transform));
    case CSSValueScale3d:
        return makeFunction(createScale3dTransformOperation(state, *transform));
    case CSSValueScaleX:
        return makeFunction(createScaleXTransformOperation(state, *transform));
    case CSSValueScaleY:
        return makeFunction(createScaleYTransformOperation(state, *transform));
    case CSSValueScaleZ:
        return makeFunction(createScaleZTransformOperation(state, *transform));
    case CSSValueTranslate:
        return makeFunction(createTranslateTransformOperation(state, *transform));
    case CSSValueTranslate3d:
        return makeFunction(createTranslate3dTransformOperation(state, *transform));
    case CSSValueTranslateX:
        return makeFunction(createTranslateXTransformOperation(state, *transform));
    case CSSValueTranslateY:
        return makeFunction(createTranslateYTransformOperation(state, *transform));
    case CSSValueTranslateZ:
        return makeFunction(createTranslateZTransformOperation(state, *transform));
    case CSSValuePerspective:
        return makeFunction(createPerspectiveTransformOperation(state, *transform));
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto CSSValueCreation<TransformFunction>::operator()(CSSValuePool& pool, const RenderStyle& style, const TransformFunction& value) -> Ref<CSSValue>
{
    auto translateLength = [&](const auto& length) -> Ref<CSSValue> {
        if (length.isZero())
            return CSSPrimitiveValue::create(0, CSSUnitType::CSS_PX);
        return ExtractorConverter::convertLength(pool, style, length);
    };

    auto includeLength = [](const auto& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    auto& operation = value.platform();
    switch (operation.type()) {
    case TransformOperation::Type::TranslateX:
        return CSSFunctionValue::create(CSSValueTranslateX, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).x()));
    case TransformOperation::Type::TranslateY:
        return CSSFunctionValue::create(CSSValueTranslateY, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).y()));
    case TransformOperation::Type::TranslateZ:
        return CSSFunctionValue::create(CSSValueTranslateZ, translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).z()));
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y()))
                return CSSFunctionValue::create(CSSValueTranslate, translateLength(translate.x()));
            return CSSFunctionValue::create(CSSValueTranslate, translateLength(translate.x()),
                translateLength(translate.y()));
        }
        return CSSFunctionValue::create(CSSValueTranslate3d,
            translateLength(translate.x()),
            translateLength(translate.y()),
            translateLength(translate.z()));
    }
    case TransformOperation::Type::ScaleX:
        return CSSFunctionValue::create(CSSValueScaleX, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).x()));
    case TransformOperation::Type::ScaleY:
        return CSSFunctionValue::create(CSSValueScaleY, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).y()));
    case TransformOperation::Type::ScaleZ:
        return CSSFunctionValue::create(CSSValueScaleZ, CSSPrimitiveValue::create(uncheckedDowncast<ScaleTransformOperation>(operation).z()));
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y())
                return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()));
            return CSSFunctionValue::create(CSSValueScale, CSSPrimitiveValue::create(scale.x()),
                CSSPrimitiveValue::create(scale.y()));
        }
        return CSSFunctionValue::create(CSSValueScale3d,
            CSSPrimitiveValue::create(scale.x()),
            CSSPrimitiveValue::create(scale.y()),
            CSSPrimitiveValue::create(scale.z()));
    }
    case TransformOperation::Type::RotateX:
        return CSSFunctionValue::create(CSSValueRotateX, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateY:
        return CSSFunctionValue::create(CSSValueRotateY, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::RotateZ:
        return CSSFunctionValue::create(CSSValueRotateZ, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate:
        return CSSFunctionValue::create(CSSValueRotate, CSSPrimitiveValue::create(uncheckedDowncast<RotateTransformOperation>(operation).angle(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        return CSSFunctionValue::create(CSSValueRotate3d, CSSPrimitiveValue::create(rotate.x()), CSSPrimitiveValue::create(rotate.y()), CSSPrimitiveValue::create(rotate.z()), CSSPrimitiveValue::create(rotate.angle(), CSSUnitType::CSS_DEG));
    }
    case TransformOperation::Type::SkewX:
        return CSSFunctionValue::create(CSSValueSkewX, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleX(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::SkewY:
        return CSSFunctionValue::create(CSSValueSkewY, CSSPrimitiveValue::create(uncheckedDowncast<SkewTransformOperation>(operation).angleY(), CSSUnitType::CSS_DEG));
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY())
            return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG));
        return CSSFunctionValue::create(CSSValueSkew, CSSPrimitiveValue::create(skew.angleX(), CSSUnitType::CSS_DEG),
            CSSPrimitiveValue::create(skew.angleY(), CSSUnitType::CSS_DEG));
    }
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective())
            return CSSFunctionValue::create(CSSValuePerspective, createCSSValue(pool, style, Length<CSS::Nonnegative> { *perspective }));
        return CSSFunctionValue::create(CSSValuePerspective, createCSSValue(pool, style, CSS::Keyword::None { }));
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        return ExtractorConverter::convertTransformationMatrix(pool, style, transform);
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        break;
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueNone);
}

void Serialize<TransformFunction>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const TransformFunction& value)
{
    auto translateLength = [&](const auto& length) {
        if (length.isZero()) {
            builder.append("0px"_s);
            return;
        }
        ExtractorSerializer::serializeLength(style, builder, context, length);
    };

    auto translateAngle = [&](auto angle) {
        serializationForCSS(builder, context, style, Angle<> { angle });
    };

    auto translateNumber = [&](auto number) {
        serializationForCSS(builder, context, style, Number<> { number });
    };

    auto includeLength = [](const auto& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    auto& operation = value.platform();
    switch (operation.type()) {
    case TransformOperation::Type::TranslateX:
        builder.append(nameLiteral(CSSValueTranslateX), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateY:
        builder.append(nameLiteral(CSSValueTranslateY), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateZ:
        builder.append(nameLiteral(CSSValueTranslateZ), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y())) {
                builder.append(nameLiteral(CSSValueTranslate), '(');
                translateLength(translate.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueTranslate), '(');
            translateLength(translate.x());
            builder.append(", "_s);
            translateLength(translate.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueTranslate3d), '(');
        translateLength(translate.x());
        builder.append(", "_s);
        translateLength(translate.y());
        builder.append(", "_s);
        translateLength(translate.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::ScaleX:
        builder.append(nameLiteral(CSSValueScaleX), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleY:
        builder.append(nameLiteral(CSSValueScaleY), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleZ:
        builder.append(nameLiteral(CSSValueScaleZ), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y()) {
                builder.append(nameLiteral(CSSValueScale), '(');
                translateNumber(scale.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueScale), '(');
            translateNumber(scale.x());
            builder.append(", "_s);
            translateNumber(scale.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueScale3d), '(');
        translateNumber(scale.x());
        builder.append(", "_s);
        translateNumber(scale.y());
        builder.append(", "_s);
        translateNumber(scale.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::RotateX:
        builder.append(nameLiteral(CSSValueRotateX), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateY:
        builder.append(nameLiteral(CSSValueRotateY), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateZ:
        builder.append(nameLiteral(CSSValueRotateZ), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate:
        builder.append(nameLiteral(CSSValueRotate), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        builder.append(nameLiteral(CSSValueRotate3d), '(');
        translateNumber(rotate.x());
        builder.append(", "_s);
        translateNumber(rotate.y());
        builder.append(", "_s);
        translateNumber(rotate.z());
        builder.append(", "_s);
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::SkewX:
        builder.append(nameLiteral(CSSValueSkewX), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleX());
        builder.append(')');
        return;
    case TransformOperation::Type::SkewY:
        builder.append(nameLiteral(CSSValueSkewY), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleY());
        builder.append(')');
        return;
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY()) {
            builder.append(nameLiteral(CSSValueSkew), '(');
            translateAngle(skew.angleX());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueSkew), '(');
        translateAngle(skew.angleX());
        builder.append(", "_s);
        translateAngle(skew.angleY());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective()) {
            builder.append(nameLiteral(CSSValuePerspective), '(');
            serializationForCSS(builder, context, style, Length<CSS::Nonnegative> { *perspective });
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValuePerspective), '(', nameLiteralForSerialization(CSSValueNone), ')');
        return;
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        ExtractorSerializer::serializeTransformationMatrix(style, builder, context, transform);
        return;
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Blending

auto Blending<TransformFunction>::blend(const TransformFunction& from, const TransformFunction& to, const Interpolation::Context& context) -> TransformFunction
{
    return TransformFunction { to->blend(&from.platform(), context) };
}

// MARK: - Platform

auto ToPlatform<TransformFunction>::operator()(const TransformFunction& value) -> Ref<TransformOperation>
{
    return value.value;
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const TransformFunction& value)
{
    return ts << value.platform();
}

} // namespace Style
} // namespace WebCore
