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
#include "CSSKeywordValue.h"
#include "CSSTransformListValue.h"
#include "CSSValueList.h"
#include "DeprecatedCSSOMValue.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleInterpolationContext.h"
#include "StyleKeyword+CSSValueConversion.h"
#include "StyleMatrix3DTransformFunction.h"
#include "StyleMatrixTransformFunction.h"
#include "StylePerspectiveTransformFunction.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "StyleRotateTransformFunction.h"
#include "StyleScaleTransformFunction.h"
#include "StyleSkewTransformFunction.h"
#include "StyleTranslateTransformFunction.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: Matrix

static RefPtr<const TransformFunctionBase> createMatrixTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
    // matrix() = matrix( <number>#{6} )

    auto function = requiredFunctionDowncast<CSSValueID::Matrix, CSSPrimitiveValue, 6>(state, value);
    if (!function)
        return { };

    return MatrixTransformFunction::create(
        toStyleFromCSSValue<Number<>>(state, protect(function->item(0))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(1))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(2))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(3))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(4))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(5))).value
    );
}

static RefPtr<const TransformFunctionBase> createMatrix3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
    // matrix3d() = matrix3d( <number>#{16} )

    auto function = requiredFunctionDowncast<CSSValueID::Matrix3d, CSSPrimitiveValue, 16>(state, value);
    if (!function)
        return { };

    return Matrix3DTransformFunction::create(TransformationMatrix(
        toStyleFromCSSValue<Number<>>(state, protect(function->item(0))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(1))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(2))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(3))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(4))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(5))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(6))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(7))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(8))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(9))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(10))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(11))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(12))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(13))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(14))).value,
        toStyleFromCSSValue<Number<>>(state, protect(function->item(15))).value
    ));
}

// MARK: Rotate

static RefPtr<const TransformFunctionBase> createRotateTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
    // rotate() = rotate( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::Rotate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 0_css_number;
    auto z = 1_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::Rotate);
}

static RefPtr<const TransformFunctionBase> createRotate3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
    // rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::Rotate3d, CSSPrimitiveValue, 4>(state, value);
    if (!function)
        return { };

    auto x = toStyleFromCSSValue<Number<>>(state, protect(function->item(0)));
    auto y = toStyleFromCSSValue<Number<>>(state, protect(function->item(1)));
    auto z = toStyleFromCSSValue<Number<>>(state, protect(function->item(2)));
    auto angle = toStyleFromCSSValue<Angle<>>(state, protect(function->item(3)));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::Rotate3D);
}

static RefPtr<const TransformFunctionBase> createRotateXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
    // rotateX() = rotateX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::RotateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 1_css_number;
    auto y = 0_css_number;
    auto z = 0_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateX);
}

static RefPtr<const TransformFunctionBase> createRotateYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
    // rotateY() = rotateY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::RotateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 1_css_number;
    auto z = 0_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateY);
}

static RefPtr<const TransformFunctionBase> createRotateZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
    // rotateZ() = rotateZ( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::RotateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto x = 0_css_number;
    auto y = 0_css_number;
    auto z = 1_css_number;
    auto angle = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));

    return RotateTransformFunction::create(x, y, z, angle, TransformFunctionType::RotateZ);
}

// MARK: Skew

static RefPtr<const TransformFunctionBase> createSkewTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
    // skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )

    auto function = requiredFunctionDowncast<CSSValueID::Skew, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));
    auto angleY = function->size() > 1 ? toStyleFromCSSValue<Angle<>>(state, protect(function->item(1))) : Angle<> { 0_css_deg };

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::Skew);
}

static RefPtr<const TransformFunctionBase> createSkewXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
    // skewX() = skewX( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::SkewX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));
    auto angleY = 0_css_deg;

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::SkewX);
}

static RefPtr<const TransformFunctionBase> createSkewYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
    // skewY() = skewY( [ <angle> | <zero> ] )

    auto function = requiredFunctionDowncast<CSSValueID::SkewY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto angleX = 0_css_deg;
    auto angleY = toStyleFromCSSValue<Angle<>>(state, protect(function->item(0)));

    return SkewTransformFunction::create(angleX, angleY, TransformFunctionType::SkewY);
}

// MARK: Scale

static RefPtr<const TransformFunctionBase> createScaleTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale
    // scale() = scale( [ <number> | <percentage> ]#{1,2} )

    auto function = requiredFunctionDowncast<CSSValueID::Scale, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(0)));
    auto sy = function->size() > 1 ? toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(1))) : sx;
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::Scale);
}

static RefPtr<const TransformFunctionBase> createScale3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
    // scale3d() = scale3d( [ <number> | <percentage> ]#{3} )

    auto function = requiredFunctionDowncast<CSSValueID::Scale3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(0)));
    auto sy = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(1)));
    auto sz = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(2)));

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::Scale3D);
}

static RefPtr<const TransformFunctionBase> createScaleXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
    // scaleX() = scaleX( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueID::ScaleX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto sx = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(0)));
    auto sy = 1_css_number;
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleX);
}

static RefPtr<const TransformFunctionBase> createScaleYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
    // scaleY() = scaleY( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueID::ScaleY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };


    auto sx = 1_css_number;
    auto sy = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(0)));
    auto sz = 1_css_number;

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleY);
}

static RefPtr<const TransformFunctionBase> createScaleZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
    // scaleZ() = scaleZ( [ <number> | <percentage> ] )

    auto function = requiredFunctionDowncast<CSSValueID::ScaleZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };


    auto sx = 1_css_number;
    auto sy = 1_css_number;
    auto sz = toStyleFromCSSValue<NumberOrPercentageResolvedToNumber<>>(state, protect(function->item(0)));

    return ScaleTransformFunction::create(sx, sy, sz, TransformFunctionType::ScaleZ);
}

// MARK: Translate

static RefPtr<const TransformFunctionBase> createTranslateTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
    // translate() = translate( <length-percentage> , <length-percentage>? )

    auto function = requiredFunctionDowncast<CSSValueID::Translate, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = toStyleFromCSSValue<TranslateTransformFunction::X>(state, protect(function->item(0)));
    auto ty = function->size() > 1 ? toStyleFromCSSValue<TranslateTransformFunction::Y>(state, protect(function->item(1))) : TranslateTransformFunction::Y { 0_css_px };
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTF::move(tx), WTF::move(ty), WTF::move(tz), TransformFunctionType::Translate);
}

static RefPtr<const TransformFunctionBase> createTranslate3dTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
    // translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )

    auto function = requiredFunctionDowncast<CSSValueID::Translate3d, CSSPrimitiveValue, 3>(state, value);
    if (!function)
        return { };

    auto tx = toStyleFromCSSValue<TranslateTransformFunction::X>(state, protect(function->item(0)));
    auto ty = toStyleFromCSSValue<TranslateTransformFunction::Y>(state, protect(function->item(1)));
    auto tz = toStyleFromCSSValue<TranslateTransformFunction::Z>(state, protect(function->item(2)));

    return TranslateTransformFunction::create(WTF::move(tx), WTF::move(ty), WTF::move(tz), TransformFunctionType::Translate3D);
}

static RefPtr<const TransformFunctionBase> createTranslateXTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
    // translateX() = translateX( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueID::TranslateX, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = toStyleFromCSSValue<TranslateTransformFunction::X>(state, protect(function->item(0)));
    auto ty = 0_css_px;
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTF::move(tx), WTF::move(ty), WTF::move(tz), TransformFunctionType::TranslateX);
}

static RefPtr<const TransformFunctionBase> createTranslateYTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
    // translateY() = translateY( <length-percentage> )

    auto function = requiredFunctionDowncast<CSSValueID::TranslateY, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = 0_css_px;
    auto ty = toStyleFromCSSValue<TranslateTransformFunction::Y>(state, protect(function->item(0)));
    auto tz = 0_css_px;

    return TranslateTransformFunction::create(WTF::move(tx), WTF::move(ty), WTF::move(tz), TransformFunctionType::TranslateY);
}

static RefPtr<const TransformFunctionBase> createTranslateZTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
    // translateZ() = translateZ( <length> )

    auto function = requiredFunctionDowncast<CSSValueID::TranslateZ, CSSPrimitiveValue, 1>(state, value);
    if (!function)
        return { };

    auto tx = 0_css_px;
    auto ty = 0_css_px;
    auto tz = toStyleFromCSSValue<TranslateTransformFunction::Z>(state, protect(function->item(0)));

    return TranslateTransformFunction::create(WTF::move(tx), WTF::move(ty), WTF::move(tz), TransformFunctionType::TranslateZ);
}

// MARK: Perspective

static RefPtr<const TransformFunctionBase> createPerspectiveTransformFunction(const CSSFunctionValue& value, BuilderState& state)
{
    // https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
    // perspective() = perspective( [ <length [0,∞]> | none ] )

    auto function = requiredFunctionDowncast<CSSValueID::Perspective, CSSValue, 1>(state, value);
    if (!function)
        return { };

    Ref parameter = function->item(0);
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(parameter)) {
        switch (keywordValue->valueID()) {
        case CSSValueID::None:
            return PerspectiveTransformFunction::create(CSS::Keyword::None { });
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }
    }

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, parameter);
    if (!primitiveValue)
        return { };

    if (primitiveValue->isLength())
        return PerspectiveTransformFunction::create(toStyleFromCSSValue<Length<CSS::Nonnegative>>(state, *primitiveValue));

    // FIXME: Support for <number> parameters for `perspective` is a quirk that should go away when 3d transforms are finalized.
    return PerspectiveTransformFunction::create(Length<CSS::Nonnegative> { toStyleFromCSSValue<Number<CSS::Nonnegative, float>>(state, *primitiveValue).value });
}

// MARK: - Conversion

auto CSSValueConversion<TransformFunction>::operator()(BuilderState& state, const CSSValue& value) -> TransformFunction
{
    RefPtr transform = requiredDowncast<CSSFunctionValue>(state, value);
    if (!transform)
        return TransformFunction { MatrixTransformFunction::createIdentity() };

    auto makeFunction = [](RefPtr<const TransformFunctionBase>&& function) {
        if (!function)
            return TransformFunction { MatrixTransformFunction::createIdentity() };
        return TransformFunction { function.releaseNonNull() };
    };

    switch (transform->name()) {
    case CSSValueID::Matrix:
        return makeFunction(createMatrixTransformFunction(*transform, state));
    case CSSValueID::Matrix3d:
        return makeFunction(createMatrix3dTransformFunction(*transform, state));
    case CSSValueID::Rotate:
        return makeFunction(createRotateTransformFunction(*transform, state));
    case CSSValueID::Rotate3d:
        return makeFunction(createRotate3dTransformFunction(*transform, state));
    case CSSValueID::RotateX:
        return makeFunction(createRotateXTransformFunction(*transform, state));
    case CSSValueID::RotateY:
        return makeFunction(createRotateYTransformFunction(*transform, state));
    case CSSValueID::RotateZ:
        return makeFunction(createRotateZTransformFunction(*transform, state));
    case CSSValueID::Skew:
        return makeFunction(createSkewTransformFunction(*transform, state));
    case CSSValueID::SkewX:
        return makeFunction(createSkewXTransformFunction(*transform, state));
    case CSSValueID::SkewY:
        return makeFunction(createSkewYTransformFunction(*transform, state));
    case CSSValueID::Scale:
        return makeFunction(createScaleTransformFunction(*transform, state));
    case CSSValueID::Scale3d:
        return makeFunction(createScale3dTransformFunction(*transform, state));
    case CSSValueID::ScaleX:
        return makeFunction(createScaleXTransformFunction(*transform, state));
    case CSSValueID::ScaleY:
        return makeFunction(createScaleYTransformFunction(*transform, state));
    case CSSValueID::ScaleZ:
        return makeFunction(createScaleZTransformFunction(*transform, state));
    case CSSValueID::Translate:
        return makeFunction(createTranslateTransformFunction(*transform, state));
    case CSSValueID::Translate3d:
        return makeFunction(createTranslate3dTransformFunction(*transform, state));
    case CSSValueID::TranslateX:
        return makeFunction(createTranslateXTransformFunction(*transform, state));
    case CSSValueID::TranslateY:
        return makeFunction(createTranslateYTransformFunction(*transform, state));
    case CSSValueID::TranslateZ:
        return makeFunction(createTranslateZTransformFunction(*transform, state));
    case CSSValueID::Perspective:
        return makeFunction(createPerspectiveTransformFunction(*transform, state));
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

auto CSSValueCreation<TransformFunction>::operator()(CSSValuePool& pool, const Style::ComputedStyle& style, const TransformFunction& value) -> Ref<CSSValue>
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

    Ref function = value.function();
    switch (function->type()) {
    case TransformFunctionType::TranslateX:
        return CSSFunctionValue::create(CSSValueID::TranslateX, translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).x()));
    case TransformFunctionType::TranslateY:
        return CSSFunctionValue::create(CSSValueID::TranslateY, translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).y()));
    case TransformFunctionType::TranslateZ:
        return CSSFunctionValue::create(CSSValueID::TranslateZ, translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).z()));
    case TransformFunctionType::Translate:
    case TransformFunctionType::Translate3D: {
        Ref translate = uncheckedDowncast<TranslateTransformFunction>(function.get());
        if (!translate->is3DOperation()) {
            if (!includeLength(translate->y()))
                return CSSFunctionValue::create(CSSValueID::Translate, translateLength(translate->x()));
            return CSSFunctionValue::create(CSSValueID::Translate,
                translateLength(translate->x()),
                translateLength(translate->y()));
        }
        return CSSFunctionValue::create(CSSValueID::Translate3d,
            translateLength(translate->x()),
            translateLength(translate->y()),
            translateLength(translate->z()));
    }
    case TransformFunctionType::ScaleX:
        return CSSFunctionValue::create(CSSValueID::ScaleX,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).x()));
    case TransformFunctionType::ScaleY:
        return CSSFunctionValue::create(CSSValueID::ScaleY,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).y()));
    case TransformFunctionType::ScaleZ:
        return CSSFunctionValue::create(CSSValueID::ScaleZ,
            createCSSValue(pool, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).z()));
    case TransformFunctionType::Scale:
    case TransformFunctionType::Scale3D: {
        Ref scale = uncheckedDowncast<ScaleTransformFunction>(function.get());
        if (!scale->is3DOperation()) {
            if (scale->x() == scale->y())
                return CSSFunctionValue::create(CSSValueID::Scale, createCSSValue(pool, style, scale->x()));
            return CSSFunctionValue::create(CSSValueID::Scale,
                createCSSValue(pool, style, scale->x()),
                createCSSValue(pool, style, scale->y()));
        }
        return CSSFunctionValue::create(CSSValueID::Scale3d,
            createCSSValue(pool, style, scale->x()),
            createCSSValue(pool, style, scale->y()),
            createCSSValue(pool, style, scale->z()));
    }
    case TransformFunctionType::RotateX:
        return CSSFunctionValue::create(CSSValueID::RotateX,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle()));
    case TransformFunctionType::RotateY:
        return CSSFunctionValue::create(CSSValueID::RotateY,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle()));
    case TransformFunctionType::RotateZ:
        return CSSFunctionValue::create(CSSValueID::RotateZ,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle()));
    case TransformFunctionType::Rotate:
        return CSSFunctionValue::create(CSSValueID::Rotate,
            createCSSValue(pool, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle()));
    case TransformFunctionType::Rotate3D: {
        Ref rotate = uncheckedDowncast<RotateTransformFunction>(function.get());
        return CSSFunctionValue::create(CSSValueID::Rotate3d,
            createCSSValue(pool, style, rotate->x()),
            createCSSValue(pool, style, rotate->y()),
            createCSSValue(pool, style, rotate->z()),
            createCSSValue(pool, style, rotate->angle()));
    }
    case TransformFunctionType::SkewX:
        return CSSFunctionValue::create(CSSValueID::SkewX,
            createCSSValue(pool, style, uncheckedDowncast<SkewTransformFunction>(function.get()).angleX()));
    case TransformFunctionType::SkewY:
        return CSSFunctionValue::create(CSSValueID::SkewY,
            createCSSValue(pool, style, uncheckedDowncast<SkewTransformFunction>(function.get()).angleY()));
    case TransformFunctionType::Skew: {
        Ref skew = uncheckedDowncast<SkewTransformFunction>(function.get());
        if (skew->angleY().isZero()) {
            return CSSFunctionValue::create(CSSValueID::Skew,
                createCSSValue(pool, style, skew->angleX()));
        }
        return CSSFunctionValue::create(CSSValueID::Skew,
            createCSSValue(pool, style, skew->angleX()),
            createCSSValue(pool, style, skew->angleY()));
    }
    case TransformFunctionType::Perspective:
        return CSSFunctionValue::create(CSSValueID::Perspective,
            createCSSValue(pool, style, uncheckedDowncast<PerspectiveTransformFunction>(function.get()).perspective()));
    case TransformFunctionType::Matrix:
    case TransformFunctionType::Matrix3D: {
        TransformationMatrix transform;
        function->apply(transform, { }, ZoomFactor::none());
        return createCSSValue(pool, style, transform);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return createCSSValue(pool, style, CSS::Keyword::None { });
}

auto CSSValueCreation<TransformationMatrix>::operator()(CSSValuePool&, const Style::ComputedStyle&, const TransformationMatrix& transform) -> Ref<CSSValue>
{
    if (transform.isAffine()) {
        auto values = std::array<double, 6> {
            transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f(),
        };

        CSSValueListBuilder arguments;
        for (auto value : values)
            arguments.append(CSSPrimitiveValue::create(value));
        return CSSFunctionValue::create(CSSValueID::Matrix, WTF::move(arguments));
    }

    auto values = std::array<double, 16> {
        transform.m11(), transform.m12(), transform.m13(), transform.m14(),
        transform.m21(), transform.m22(), transform.m23(), transform.m24(),
        transform.m31(), transform.m32(), transform.m33(), transform.m34(),
        transform.m41(), transform.m42(), transform.m43(), transform.m44(),
    };

    CSSValueListBuilder arguments;
    for (auto value : values)
        arguments.append(CSSPrimitiveValue::create(value));
    return CSSFunctionValue::create(CSSValueID::Matrix3d, WTF::move(arguments));
}

Ref<DeprecatedCSSOMValue> DeprecatedCSSOMValueCreation<TransformFunction>::operator()(CSSValuePool& pool, const Style::ComputedStyle& style, CSSStyleDeclaration& owner, const TransformFunction& value)
{
    return createCSSValue(pool, style, value)->createDeprecatedCSSOMWrapper(owner);
}

// MARK: - Serialization

void Serialize<TransformFunction>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle& style, const TransformFunction& value)
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

    Ref function = value.function();
    switch (function->type()) {
    case TransformFunctionType::TranslateX:
        builder.append(nameLiteral(CSSValueID::TranslateX), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).x());
        builder.append(')');
        return;
    case TransformFunctionType::TranslateY:
        builder.append(nameLiteral(CSSValueID::TranslateY), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).y());
        builder.append(')');
        return;
    case TransformFunctionType::TranslateZ:
        builder.append(nameLiteral(CSSValueID::TranslateZ), '(');
        translateLength(uncheckedDowncast<TranslateTransformFunction>(function.get()).z());
        builder.append(')');
        return;
    case TransformFunctionType::Translate:
    case TransformFunctionType::Translate3D: {
        Ref translate = uncheckedDowncast<TranslateTransformFunction>(function.get());
        if (!translate->is3DOperation()) {
            if (!includeLength(translate->y())) {
                builder.append(nameLiteral(CSSValueID::Translate), '(');
                translateLength(translate->x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueID::Translate), '(');
            translateLength(translate->x());
            builder.append(", "_s);
            translateLength(translate->y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueID::Translate3d), '(');
        translateLength(translate->x());
        builder.append(", "_s);
        translateLength(translate->y());
        builder.append(", "_s);
        translateLength(translate->z());
        builder.append(')');
        return;
    }
    case TransformFunctionType::ScaleX:
        builder.append(nameLiteral(CSSValueID::ScaleX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).x());
        builder.append(')');
        return;
    case TransformFunctionType::ScaleY:
        builder.append(nameLiteral(CSSValueID::ScaleY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).y());
        builder.append(')');
        return;
    case TransformFunctionType::ScaleZ:
        builder.append(nameLiteral(CSSValueID::ScaleZ), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<ScaleTransformFunction>(function.get()).z());
        builder.append(')');
        return;
    case TransformFunctionType::Scale:
    case TransformFunctionType::Scale3D: {
        Ref scale = uncheckedDowncast<ScaleTransformFunction>(function.get());
        if (!scale->is3DOperation()) {
            if (scale->x() == scale->y()) {
                builder.append(nameLiteral(CSSValueID::Scale), '(');
                serializationForCSS(builder, context, style, scale->x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueID::Scale), '(');
            serializationForCSS(builder, context, style, scale->x());
            builder.append(", "_s);
            serializationForCSS(builder, context, style, scale->y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueID::Scale3d), '(');
        serializationForCSS(builder, context, style, scale->x());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, scale->y());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, scale->z());
        builder.append(')');
        return;
    }
    case TransformFunctionType::RotateX:
        builder.append(nameLiteral(CSSValueID::RotateX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle());
        builder.append(')');
        return;
    case TransformFunctionType::RotateY:
        builder.append(nameLiteral(CSSValueID::RotateY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle());
        builder.append(')');
        return;
    case TransformFunctionType::RotateZ:
        builder.append(nameLiteral(CSSValueID::RotateZ), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle());
        builder.append(')');
        return;
    case TransformFunctionType::Rotate:
        builder.append(nameLiteral(CSSValueID::Rotate), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<RotateTransformFunction>(function.get()).angle());
        builder.append(')');
        return;
    case TransformFunctionType::Rotate3D: {
        Ref rotate = uncheckedDowncast<RotateTransformFunction>(function.get());
        builder.append(nameLiteral(CSSValueID::Rotate3d), '(');
        serializationForCSS(builder, context, style, rotate->x());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate->y());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate->z());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, rotate->angle());
        builder.append(')');
        return;
    }
    case TransformFunctionType::SkewX:
        builder.append(nameLiteral(CSSValueID::SkewX), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<SkewTransformFunction>(function.get()).angleX());
        builder.append(')');
        return;
    case TransformFunctionType::SkewY:
        builder.append(nameLiteral(CSSValueID::SkewY), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<SkewTransformFunction>(function.get()).angleY());
        builder.append(')');
        return;
    case TransformFunctionType::Skew: {
        Ref skew = uncheckedDowncast<SkewTransformFunction>(function.get());
        if (skew->angleY().isZero()) {
            builder.append(nameLiteral(CSSValueID::Skew), '(');
            serializationForCSS(builder, context, style, skew->angleX());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueID::Skew), '(');
        serializationForCSS(builder, context, style, skew->angleX());
        builder.append(", "_s);
        serializationForCSS(builder, context, style, skew->angleY());
        builder.append(')');
        return;
    }
    case TransformFunctionType::Perspective:
        builder.append(nameLiteral(CSSValueID::Perspective), '(');
        serializationForCSS(builder, context, style, uncheckedDowncast<PerspectiveTransformFunction>(function.get()).perspective());
        builder.append(')');
        return;
    case TransformFunctionType::Matrix:
    case TransformFunctionType::Matrix3D: {
        TransformationMatrix transform;
        function->apply(transform, { }, ZoomFactor::none());
        serializationForCSS(builder, context, style, transform);
        return;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void Serialize<TransformationMatrix>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle&, const TransformationMatrix& transform)
{
    if (transform.isAffine()) {
        auto values = std::array<double, 6> {
            transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f(),
        };
        builder.append(nameLiteral(CSSValueID::Matrix), '(', interleave(values, [&](auto& builder, auto& value) {
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
        }, ", "_s), ')');
        return;
    }

    auto values = std::array<double, 16> {
        transform.m11(), transform.m12(), transform.m13(), transform.m14(),
        transform.m21(), transform.m22(), transform.m23(), transform.m24(),
        transform.m31(), transform.m32(), transform.m33(), transform.m34(),
        transform.m41(), transform.m42(), transform.m43(), transform.m44(),
    };
    builder.append(nameLiteral(CSSValueID::Matrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

// MARK: - Blending

auto Blending<TransformFunction>::blend(const TransformFunction& from, const TransformFunction& to, const Interpolation::Context& context) -> TransformFunction
{
    return TransformFunction { protect(to.function())->blend(protect(&from.function()), context) };
}

// MARK: - Platform

auto ToPlatform<TransformFunction>::operator()(const TransformFunction& value, const FloatSize& size, ZoomFactor zoom) -> Ref<TransformOperation>
{
    return protect(value.value)->toPlatform(size, zoom);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const TransformFunction& value)
{
    return ts << protect(value.function());
}

} // namespace Style
} // namespace WebCore
