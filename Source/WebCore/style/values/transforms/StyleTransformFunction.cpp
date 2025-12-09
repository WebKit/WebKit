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
#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleCalculationValue.h"
#include "StyleInterpolationContext.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleMatrix3DTransformFunction.h"
#include "StyleMatrixTransformFunction.h"
#include "StylePerspectiveTransformFunction.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "StyleRotateTransformFunction.h"
#include "StyleScaleTransformFunction.h"
#include "StyleSkewTransformFunction.h"
#include "StyleTranslateTransformFunction.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

static TranslateTransformFunction::LengthPercentage resolveAsTranslateLengthPercentage(const CSSPrimitiveValue& primitiveValue, BuilderState& state)
{
    // FIXME: This should use `toStyleFromCSSValue<TranslateTransformFunction::LengthPercentage>`, but doing so breaks transforms/hittest-translated-content-off-to-infinity-and-back.html, due to it clamping between minValueForCssLength/maxValueForCssLength.

    auto& conversionData = state.cssToLengthConversionData();
    if (primitiveValue.isLength())
        return TranslateTransformFunction::LengthPercentage::Fixed { static_cast<float>(primitiveValue.resolveAsLength<double>(conversionData)) };
    if (primitiveValue.isPercentage())
        return TranslateTransformFunction::LengthPercentage::Percentage { static_cast<float>(primitiveValue.resolveAsPercentage<double>(conversionData)) };
    if (primitiveValue.isCalculated())
        return TranslateTransformFunction::LengthPercentage::Calc { primitiveValue.protectedCssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { }) };

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return 0_css_px;
}

static TranslateTransformFunction::Length resolveAsTranslateLength(const CSSPrimitiveValue& primitiveValue, BuilderState& state)
{
    // FIXME: This should use `toStyleFromCSSValue<TranslateTransformFunction::Length>`, but doing so breaks transforms/hittest-translated-content-off-to-infinity-and-back.html, due to it clamping between minValueForCssLength/maxValueForCssLength.

    auto& conversionData = state.cssToLengthConversionData();
    if (primitiveValue.isLength())
        return TranslateTransformFunction::Length { static_cast<float>(primitiveValue.resolveAsLength<double>(conversionData)) };

    state.setCurrentPropertyInvalidAtComputedValueTime();
    return 0_css_px;
}

// MARK: Matrix

static RefPtr<const TransformFunctionBase> createMatrixTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
    // matrix() = matrix( <number>#{6} )

    auto function = requiredFunctionDowncast<CSSValueMatrix, CSSPrimitiveValue, 6>(state, value);
    if (!function)
        return { };

    auto zoom = state.cssToLengthConversionData().zoom();
    return MatrixTransformFunction::create(
        toStyleFromCSSValue<Number<>>(state, function->item(0)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(1)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(2)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(3)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(4)).value * zoom,
        toStyleFromCSSValue<Number<>>(state, function->item(5)).value * zoom
    );
}

static RefPtr<const TransformFunctionBase> createMatrix3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
    // matrix3d() = matrix3d( <number>#{16} )

    auto function = requiredFunctionDowncast<CSSValueMatrix3d, CSSPrimitiveValue, 16>(state, value);
    if (!function)
        return { };

    TransformationMatrix matrix(
        toStyleFromCSSValue<Number<>>(state, function->item(0)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(1)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(2)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(3)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(4)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(5)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(6)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(7)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(8)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(9)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(10)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(11)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(12)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(13)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(14)).value,
        toStyleFromCSSValue<Number<>>(state, function->item(15)).value
    );
    matrix.zoom(state.cssToLengthConversionData().zoom());

    return Matrix3DTransformFunction::create(WTFMove(matrix));
}

// MARK: Rotate

static RefPtr<const TransformFunctionBase> createRotateTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
    // rotate() = rotate( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 0_css_number;
    auto z = 1_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, function->item(0));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::Rotate);
}

static RefPtr<const TransformFunctionBase> createRotate3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotate3d, CSSPrimitiveValue, 4>(state, value);
    if (!function)
        return { };

    auto x = toStyleFromCSSValue<Number<>>(state, function->item(0));
    auto y = toStyleFromCSSValue<Number<>>(state, function->item(1));
    auto z = toStyleFromCSSValue<Number<>>(state, function->item(2));
    auto angle = toStyleFromCSSValue<Angle<>>(state, function->item(3));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::Rotate3D);
}

static RefPtr<const TransformFunctionBase> createRotateXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
    // rotateX() = rotateX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 1_css_number;
    auto y = 0_css_number;
    auto z = 0_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, function->item(0));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateX);
}

static RefPtr<const TransformFunctionBase> createRotateYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
    // rotateY() = rotateY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 1_css_number;
    auto z = 0_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, function->item(0));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateY);
}

static RefPtr<const TransformFunctionBase> createRotateZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
    // rotateZ() = rotateZ( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueRotateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 0_css_number;
    auto z = 1_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, function->item(0));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateZ);
}

// MARK: Skew

static RefPtr<const TransformFunctionBase> createSkewTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )

    auto function = requiredFunctionDowncast<CSSValueSkew, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = toStyleFromCSSValue<Angle<>>(state, function->item(0));
    auto angleY = function->size() > 1 ? toStyleFromCSSValue<Angle<>>(state, function->item(1)) : Angle<> { 0_css_deg };

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::Skew);
}

static RefPtr<const TransformFunctionBase> createSkewXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
    // skewX() = skewX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueSkewX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = toStyleFromCSSValue<Angle<>>(state, function->item(0));
    auto angleY = 0_css_deg;

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::SkewX);
}

static RefPtr<const TransformFunctionBase> createSkewYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
    // skewY() = skewY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueSkewY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = 0_css_deg;
    auto angleY = toStyleFromCSSValue<Angle<>>(state, function->item(0));

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::SkewY);
}

// MARK: Scale

static RefPtr<const TransformFunctionBase> createScaleTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )

    auto function = requiredFunctionDowncast<CSSValueScale, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(0));
    auto sy = function->size() > 1 ? toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(1)) : sx;
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::Scale);
}

static RefPtr<const TransformFunctionBase> createScale3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    auto function = requiredFunctionDowncast<CSSValueScale3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(0));
    auto sy = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(1));
    auto sz = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(2));

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::Scale3D);
}

static RefPtr<const TransformFunctionBase> createScaleXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
    // scaleX() = scaleX( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(0));
    auto sy = 1_css_number;
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleX);
}

static RefPtr<const TransformFunctionBase> createScaleYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
    // scaleY() = scaleY( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };


    auto sx = 1_css_number;
    auto sy = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(0));
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleY);
}

static RefPtr<const TransformFunctionBase> createScaleZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
    // scaleZ() = scaleZ( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueScaleZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };


    auto sx = 1_css_number;
    auto sy = 1_css_number;
    auto sz = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, function->item(0));

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleZ);
}

// MARK: Translate

static RefPtr<const TransformFunctionBase> createTranslateTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )

    auto function = requiredFunctionDowncast<CSSValueTranslate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = resolveAsTranslateLengthPercentage(function->item(0), state);
    auto ty = function->size() > 1 ? resolveAsTranslateLengthPercentage(function->item(1), state) : TranslateTransformFunction::LengthPercentage { 0_css_px };
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformFunctionType::Translate);
}

static RefPtr<const TransformFunctionBase> createTranslate3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto function = requiredFunctionDowncast<CSSValueTranslate3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto tx = resolveAsTranslateLengthPercentage(function->item(0), state);
    auto ty = resolveAsTranslateLengthPercentage(function->item(1), state);
    auto tz = resolveAsTranslateLength(function->item(2), state);

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformFunctionType::Translate3D);
}

static RefPtr<const TransformFunctionBase> createTranslateXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
    // translateX() = translateX( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueTranslateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = resolveAsTranslateLengthPercentage(function->item(0), state);
    auto ty = 0_css_px;
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformFunctionType::TranslateX);
}

static RefPtr<const TransformFunctionBase> createTranslateYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
    // translateY() = translateY( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueTranslateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = 0_css_px;
    auto ty = resolveAsTranslateLengthPercentage(function->item(0), state);
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformFunctionType::TranslateY);
}

static RefPtr<const TransformFunctionBase> createTranslateZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
    // translateZ() = translateZ( <length> )

    auto function = requiredFunctionDowncast<CSSValueTranslateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = 0_css_px;
    auto ty = 0_css_px;
    auto tz = resolveAsTranslateLength(function->item(0), state);

    return TranslateTransformFunction::create(WTFMove(tx), WTFMove(ty), WTFMove(tz), TransformFunctionType::TranslateZ);
}

// MARK: Perspective

static RefPtr<const TransformFunctionBase> createPerspectiveTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
    // perspective() = perspective( [ <length [0,∞]> | none ] )

    auto function = requiredFunctionDowncast<CSSValuePerspective, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto& parameter = function->item(0);
    if (parameter.isValueID()) {
        ASSERT(parameter.valueID() == CSSValueNone);
        return PerspectiveTransformFunction::create(CSS::Keyword::None { });
    }

    if (parameter.isLength())
        return PerspectiveTransformFunction::create(toStyleFromCSSValue<Length<CSS::Nonnegative>>(state, parameter));

    // FIXME: Support for <number> parameters for `perspective` is a quirk that should go away when 3d transforms are finalized.
    return PerspectiveTransformFunction::create(
        Length<CSS::Nonnegative> {
            static_cast<float>(toStyleFromCSSValue<Number<CSS::Nonnegative>>(state, parameter).value)
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<TransformFunction>::operator()(BuilderState& state, const CSSValue& value) -> TransformFunction
{
    auto transform = requiredDowncast<CSSFunctionValue>(state, value);
    if (!transform)
        return TransformFunction { MatrixTransformFunction::createIdentity() };

    auto makeFunction = [](RefPtr<const TransformFunctionBase>&& function) {
        if (!function)
            return TransformFunction { MatrixTransformFunction::createIdentity() };
        return TransformFunction { function.releaseNonNull() };
    };

    switch (transform->name()) {
    case CSSValueMatrix:
        return makeFunction(createMatrixTransformFunction(*transform, state));
    case CSSValueMatrix3d:
        return makeFunction(createMatrix3dTransformFunction(*transform, state));
    case CSSValueRotate:
        return makeFunction(createRotateTransformFunction(*transform, state));
    case CSSValueRotate3d:
        return makeFunction(createRotate3dTransformFunction(*transform, state));
    case CSSValueRotateX:
        return makeFunction(createRotateXTransformFunction(*transform, state));
    case CSSValueRotateY:
        return makeFunction(createRotateYTransformFunction(*transform, state));
    case CSSValueRotateZ:
        return makeFunction(createRotateZTransformFunction(*transform, state));
    case CSSValueSkew:
        return makeFunction(createSkewTransformFunction(*transform, state));
    case CSSValueSkewX:
        return makeFunction(createSkewXTransformFunction(*transform, state));
    case CSSValueSkewY:
        return makeFunction(createSkewYTransformFunction(*transform, state));
    case CSSValueScale:
        return makeFunction(createScaleTransformFunction(*transform, state));
    case CSSValueScale3d:
        return makeFunction(createScale3dTransformFunction(*transform, state));
    case CSSValueScaleX:
        return makeFunction(createScaleXTransformFunction(*transform, state));
    case CSSValueScaleY:
        return makeFunction(createScaleYTransformFunction(*transform, state));
    case CSSValueScaleZ:
        return makeFunction(createScaleZTransformFunction(*transform, state));
    case CSSValueTranslate:
        return makeFunction(createTranslateTransformFunction(*transform, state));
    case CSSValueTranslate3d:
        return makeFunction(createTranslate3dTransformFunction(*transform, state));
    case CSSValueTranslateX:
        return makeFunction(createTranslateXTransformFunction(*transform, state));
    case CSSValueTranslateY:
        return makeFunction(createTranslateYTransformFunction(*transform, state));
    case CSSValueTranslateZ:
        return makeFunction(createTranslateZTransformFunction(*transform, state));
    case CSSValuePerspective:
        return makeFunction(createPerspectiveTransformFunction(*transform, state));
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto CSSValueCreation<TransformFunction>::operator()(CSSValuePool& pool, const RenderStyle& style, const TransformFunction& value) -> Ref<CSSValue>
{
    auto translateLength = [&](const auto& length) -> Ref<CSSValue> {
        if (length.isKnownZero())
            return createCSSValue(pool, style, Length<> { 0_css_px });
        else
            return createCSSValue(pool, style, length);
    };

    auto includeLength = [](const auto& length) -> bool {
        return !length.isKnownZero() || length.isPercent();
    };

    auto& function = value.function();
    switch (function.type()) {
    case TransformFunctionType::TranslateX:
        return CSSFunctionValue::create(CSSValueTranslateX, translateLength(uncheckedDowncast<TranslateTransformFunction>(function).x()));
    case TransformFunctionType::TranslateY:
        return CSSFunctionValue::create(CSSValueTranslateY, translateLength(uncheckedDowncast<TranslateTransformFunction>(function).y()));
    case TransformFunctionType::TranslateZ:
        return CSSFunctionValue::create(CSSValueTranslateZ, translateLength(uncheckedDowncast<TranslateTransformFunction>(function).z()));
    case TransformFunctionType::Translate:
    case TransformFunctionType::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformFunction>(function);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y()))
                return CSSFunctionValue::create(CSSValueTranslate, translateLength(translate.x()));
            return CSSFunctionValue::create(CSSValueTranslate,
                translateLength(translate.x()),
                translateLength(translate.y()));
        }
        return CSSFunctionValue::create(CSSValueTranslate3d,
            translateLength(translate.x()),
            translateLength(translate.y()),
            translateLength(translate.z()));
    }
    case TransformFunctionType::ScaleX:
        return CSSFunctionValue::create(CSSValueScaleX,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function).x()));
    case TransformFunctionType::ScaleY:
        return CSSFunctionValue::create(CSSValueScaleY,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function).y()));
    case TransformFunctionType::ScaleZ:
        return CSSFunctionValue::create(CSSValueScaleZ,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function).z()));
    case TransformFunctionType::Scale:
    case TransformFunctionType::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformFunction>(function);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y())
                return CSSFunctionValue::create(CSSValueScale, createCSSValue(pool, style, scale.x()));
            return CSSFunctionValue::create(CSSValueScale,
                createCSSValue(pool, style, scale.x()),
                createCSSValue(pool, style, scale.y()));
        }
        return CSSFunctionValue::create(CSSValueScale3d,
            createCSSValue(pool, style, scale.x()),
            createCSSValue(pool, style, scale.y()),
            createCSSValue(pool, style, scale.z()));
    }
    case TransformFunctionType::RotateX:
        return CSSFunctionValue::create(CSSValueRotateX,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function).angle()));
    case TransformFunctionType::RotateY:
        return CSSFunctionValue::create(CSSValueRotateY,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function).angle()));
    case TransformFunctionType::RotateZ:
        return CSSFunctionValue::create(CSSValueRotateZ,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function).angle()));
    case TransformFunctionType::Rotate:
        return CSSFunctionValue::create(CSSValueRotate,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function).angle()));
    case TransformFunctionType::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformFunction>(function);
        return CSSFunctionValue::create(CSSValueRotate3d,
            createCSSValue(pool, style, rotate.x()),
            createCSSValue(pool, style, rotate.y()),
            createCSSValue(pool, style, rotate.z()),
            createCSSValue(pool, style, rotate.angle()));
    }
    case TransformFunctionType::SkewX:
        return CSSFunctionValue::create(CSSValueSkewX,
            createCSSValue(pool, style, uncheckedDowncast<SkewTransformFunction>(function).angleX()));
    case TransformFunctionType::SkewY:
        return CSSFunctionValue::create(CSSValueSkewY,
            createCSSValue(pool, style, uncheckedDowncast<SkewTransformFunction>(function).angleY()));
    case TransformFunctionType::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformFunction>(function);
        if (skew.angleY().isZero())
            return CSSFunctionValue::create(CSSValueSkew,
                createCSSValue(pool, style, skew.angleX()));
        return CSSFunctionValue::create(CSSValueSkew,
            createCSSValue(pool, style, skew.angleX()),
            createCSSValue(pool, style, skew.angleY()));
    }
    case TransformFunctionType::Perspective:
        return CSSFunctionValue::create(CSSValuePerspective,
            createCSSValue(pool, style, uncheckedDowncast<PerspectiveTransformFunction>(function).perspective()));
    case TransformFunctionType::Matrix:
    case TransformFunctionType::Matrix3D: {
        TransformationMatrix transform;
        function.apply(transform, { });
        return createCSSValue(pool, style, transform);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return createCSSValue(pool, style, CSS::Keyword::None { });
}

auto CSSValueCreation<TransformationMatrix>::operator()(CSSValuePool&, const RenderStyle& style, const TransformationMatrix& transform) -> Ref<CSSValue>
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        double values[] = { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(CSSPrimitiveValue::create(value));
        return CSSFunctionValue::create(CSSValueMatrix, WTFMove(arguments));
    }

    double values[] = {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    CSSValueListBuilder arguments;
    for (auto value : values)
        arguments.append(CSSPrimitiveValue::create(value));
    return CSSFunctionValue::create(CSSValueMatrix3d, WTFMove(arguments));
}

// MARK: - Serialization

void Serialize<TransformFunction>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const TransformFunction& value)
{
    auto translateLength = [&](const auto& length) {
        if (length.isKnownZero())
            serializationForCSS(builder, context, style, Length<> { 0_css_px });
        else
            serializationForCSS(builder, context, style, length);
    };

    auto includeLength = [](const auto& length) -> bool {
        return !length.isKnownZero() || length.isPercent();
    };

    auto& function = value.function();
    switch (function.type()) {
    case TransformFunctionType::TranslateX:
        builder.append(nameLiteral(CSSValueTranslateX), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function).x());
        builder.append(')');
        return;
    case TransformFunctionType::TranslateY:
        builder.append(nameLiteral(CSSValueTranslateY), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function).y());
        builder.append(')');
        return;
    case TransformFunctionType::TranslateZ:
        builder.append(nameLiteral(CSSValueTranslateZ), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function).z());
        builder.append(')');
        return;
    case TransformFunctionType::Translate:
    case TransformFunctionType::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformFunction>(function);
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
    case TransformFunctionType::ScaleX:
        builder.append(nameLiteral(CSSValueScaleX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function).x());
        builder.append(')');
        return;
    case TransformFunctionType::ScaleY:
        builder.append(nameLiteral(CSSValueScaleY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function).y());
        builder.append(')');
        return;
    case TransformFunctionType::ScaleZ:
        builder.append(nameLiteral(CSSValueScaleZ), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function).z());
        builder.append(')');
        return;
    case TransformFunctionType::Scale:
    case TransformFunctionType::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformFunction>(function);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y()) {
                builder.append(nameLiteral(CSSValueScale), '(');
                serializationForCSS(builder, context, style, scale.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueScale), '(');
            serializationForCSS(builder, context, style, scale.x());
            builder.append(", "_s);
            serializationForCSS(builder, context, style, scale.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueScale3d), '(');
        serializationForCSS(builder, context, style, scale.x());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, scale.y());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, scale.z());
        builder.append(')');
        return;
    }
    case TransformFunctionType::RotateX:
        builder.append(nameLiteral(CSSValueRotateX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function).angle());
        builder.append(')');
        return;
    case TransformFunctionType::RotateY:
        builder.append(nameLiteral(CSSValueRotateY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function).angle());
        builder.append(')');
        return;
    case TransformFunctionType::RotateZ:
        builder.append(nameLiteral(CSSValueRotateZ), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function).angle());
        builder.append(')');
        return;
    case TransformFunctionType::Rotate:
        builder.append(nameLiteral(CSSValueRotate), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function).angle());
        builder.append(')');
        return;
    case TransformFunctionType::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformFunction>(function);
        builder.append(nameLiteral(CSSValueRotate3d), '(');
        serializationForCSS(builder, context, style, rotate.x());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate.y());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate.z());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate.angle());
        builder.append(')');
        return;
    }
    case TransformFunctionType::SkewX:
        builder.append(nameLiteral(CSSValueSkewX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<SkewTransformFunction>(function).angleX());
        builder.append(')');
        return;
    case TransformFunctionType::SkewY:
        builder.append(nameLiteral(CSSValueSkewY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<SkewTransformFunction>(function).angleY());
        builder.append(')');
        return;
    case TransformFunctionType::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformFunction>(function);
        if (skew.angleY().isZero()) {
            builder.append(nameLiteral(CSSValueSkew), '(');
            serializationForCSS(builder, context, style, skew.angleX());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueSkew), '(');
        serializationForCSS(builder, context, style, skew.angleX());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, skew.angleY());
        builder.append(')');
        return;
    }
    case TransformFunctionType::Perspective:
        builder.append(nameLiteral(CSSValuePerspective), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<PerspectiveTransformFunction>(function).perspective());
        builder.append(')');
        return;
    case TransformFunctionType::Matrix:
    case TransformFunctionType::Matrix3D: {
        TransformationMatrix transform;
        function.apply(transform, { });
        serializationForCSS(builder, context, style, transform);
        return;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void Serialize<TransformationMatrix>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        std::array values { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        builder.append(nameLiteral(CSSValueMatrix), '(', interleave(values, [&](auto& builder, auto& value) {
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
        }, ", "_s), ')');
        return;
    }

    std::array values {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    builder.append(nameLiteral(CSSValueMatrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

// MARK: - Blending

auto Blending<TransformFunction>::blend(const TransformFunction& from, const TransformFunction& to, const Interpolation::Context& context) -> TransformFunction
{
    return TransformFunction { to.function().blend(&from.function(), context) };
}

// MARK: - Platform

auto ToPlatform<TransformFunction>::operator()(const TransformFunction& value, const FloatSize& size) -> Ref<TransformOperation>
{
    return value.value->toPlatform(size);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const TransformFunction& value)
{
    return ts << value.function();
}

} // namespace Style
} // namespace WebCore
