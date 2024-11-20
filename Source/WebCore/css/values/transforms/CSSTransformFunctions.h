/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveNumericTypes.h"
#include <wtf/Markable.h>

namespace WebCore {
namespace CSS {

// MARK: - Transform Functions

struct Matrix;
struct Matrix3D;
struct Rotate;
struct Rotate3D;
struct RotateX;
struct RotateY;
struct RotateZ;
struct Skew;
struct SkewX;
struct SkewY;
struct Scale;
struct Scale3D;
struct ScaleX;
struct ScaleY;
struct ScaleZ;
struct Translate;
struct Translate3D;
struct TranslateX;
struct TranslateY;
struct TranslateZ;
struct Perspective;

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-matrix
// matrix() = matrix( <number>#{6} )
struct Matrix {
    CommaSeparatedArray<Number<>, 6> value;

    bool operator==(const Matrix&) const = default;
};
using MatrixFunction = FunctionNotation<CSSValueMatrix, Matrix>;
DEFINE_CSS_TYPE_WRAPPER(Matrix)

// https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
// matrix3d() = matrix3d( <number>#{16} )
struct Matrix3D {
    CommaSeparatedArray<Number<>, 16> value;

    bool operator==(const Matrix3D&) const = default;
};
using Matrix3DFunction = FunctionNotation<CSSValueMatrix3d, Matrix3D>;
DEFINE_CSS_TYPE_WRAPPER(Matrix3D)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
// rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )
struct Rotate3D {
    Number<> x;
    Number<> y;
    Number<> z;
    Angle<> angle;

    bool operator==(const Rotate3D&) const = default;
};
using Rotate3DFunction = FunctionNotation<CSSValueRotate3d, Rotate3D>;

template<size_t I> const auto& get(const Rotate3D& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
    else if constexpr (I == 2)
        return value.z;
    else if constexpr (I == 3)
        return value.angle;
}

template<> struct Serialize<Rotate3D> { void operator()(StringBuilder&, const Rotate3D&); };

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
// rotate() = rotate( [ <angle> | <zero> ] )
struct Rotate {
    Angle<> value;

    bool operator==(const Rotate&) const = default;
};
using RotateFunction = FunctionNotation<CSSValueRotate, Rotate>;
DEFINE_CSS_TYPE_WRAPPER(Rotate)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
// rotateX() = rotateX( [ <angle> | <zero> ] )
struct RotateX {
    Angle<> value;

    bool operator==(const RotateX&) const = default;
};
using RotateXFunction = FunctionNotation<CSSValueRotateX, RotateX>;
DEFINE_CSS_TYPE_WRAPPER(RotateX)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
// rotateY() = rotateY( [ <angle> | <zero> ] )
struct RotateY {
    Angle<> value;

    bool operator==(const RotateY&) const = default;
};
using RotateYFunction = FunctionNotation<CSSValueRotateY, RotateY>;
DEFINE_CSS_TYPE_WRAPPER(RotateY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
// rotateZ() = rotateZ( [ <angle> | <zero> ] )
struct RotateZ {
    Angle<> value;

    bool operator==(const RotateZ&) const = default;
};
using RotateZFunction = FunctionNotation<CSSValueRotateZ, RotateZ>;
DEFINE_CSS_TYPE_WRAPPER(RotateZ)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
// skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
struct Skew {
    Angle<> x;
    Markable<Angle<>, Angle<>::MarkableTraits> y; // Defaults to `0`.

    bool operator==(const Skew&) const = default;
};
using SkewFunction = FunctionNotation<CSSValueSkew, Skew>;

template<size_t I> const auto& get(const Skew& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
}

template<> struct Serialize<Skew> { void operator()(StringBuilder&, const Skew&); };

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
// skewX() = skewX( [ <angle> | <zero> ] )
struct SkewX {
    Angle<> value;

    bool operator==(const SkewX&) const = default;
};
using SkewXFunction = FunctionNotation<CSSValueSkewX, SkewX>;
DEFINE_CSS_TYPE_WRAPPER(SkewX)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
// skewY() = skewY( [ <angle> | <zero> ] )
struct SkewY {
    Angle<> value;

    bool operator==(const SkewY&) const = default;
};
using SkewYFunction = FunctionNotation<CSSValueSkewY, SkewY>;
DEFINE_CSS_TYPE_WRAPPER(SkewY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
// scale3d() = scale3d( [ <number> | <percentage> ]#{3} )
struct Scale3D {
    NumberOrPercentageResolvedToNumber x;
    NumberOrPercentageResolvedToNumber y;
    NumberOrPercentageResolvedToNumber z;

    bool operator==(const Scale3D&) const = default;
};
using Scale3DFunction = FunctionNotation<CSSValueScale3d, Scale3D>;

template<size_t I> const auto& get(const Scale3D& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
    else if constexpr (I == 2)
        return value.z;
}

template<> struct Serialize<Scale3D> { void operator()(StringBuilder&, const Scale3D&); };

// https://drafts.csswg.org/css-transforms-2/#funcdef-scale
// scale() = scale( [ <number> | <percentage> ]#{1,2} )
struct Scale {
    NumberOrPercentageResolvedToNumber x;
    std::optional<NumberOrPercentageResolvedToNumber> y; // Defaults to value of `x`.

    bool operator==(const Scale&) const = default;
};
using ScaleFunction = FunctionNotation<CSSValueScale, Scale>;

template<size_t I> const auto& get(const Scale& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
}

template<> struct Serialize<Scale> { void operator()(StringBuilder&, const Scale&); };

// https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
// scaleX() = scaleX( [ <number> | <percentage> ] )
struct ScaleX {
    NumberOrPercentageResolvedToNumber value;

    bool operator==(const ScaleX&) const = default;
};
using ScaleXFunction = FunctionNotation<CSSValueScaleX, ScaleX>;
DEFINE_CSS_TYPE_WRAPPER(ScaleX)

// https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
// scaleY() = scaleY( [ <number> | <percentage> ] )
struct ScaleY {
    NumberOrPercentageResolvedToNumber value;

    bool operator==(const ScaleY&) const = default;
};
using ScaleYFunction = FunctionNotation<CSSValueScaleY, ScaleY>;
DEFINE_CSS_TYPE_WRAPPER(ScaleY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
// scaleZ() = scaleZ( [ <number> | <percentage> ] )
struct ScaleZ {
    NumberOrPercentageResolvedToNumber value;

    bool operator==(const ScaleZ&) const = default;
};
using ScaleZFunction = FunctionNotation<CSSValueScaleZ, ScaleZ>;
DEFINE_CSS_TYPE_WRAPPER(ScaleZ)

// https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
// translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )
struct Translate3D {
    LengthPercentage<> x;
    LengthPercentage<> y;
    Length<> z;

    bool operator==(const Translate3D&) const = default;
};
using Translate3DFunction = FunctionNotation<CSSValueTranslate3d, Translate3D>;

template<size_t I> const auto& get(const Translate3D& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
    else if constexpr (I == 2)
        return value.z;
}

template<> struct Serialize<Translate3D> { void operator()(StringBuilder&, const Translate3D&); };

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
// translate() = translate( <length-percentage> , <length-percentage>? )
struct Translate {
    LengthPercentage<> x;
    Markable<LengthPercentage<>, LengthPercentage<>::MarkableTraits> y; // Defaults to `0`.

    bool operator==(const Translate&) const = default;
};
using TranslateFunction = FunctionNotation<CSSValueTranslate, Translate>;

template<size_t I> const auto& get(const Translate& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
}

template<> struct Serialize<Translate> { void operator()(StringBuilder&, const Translate&); };

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
// translateX() = translateX( <length-percentage> )
struct TranslateX {
    LengthPercentage<> value;

    bool operator==(const TranslateX&) const = default;
};
using TranslateXFunction = FunctionNotation<CSSValueTranslateX, TranslateX>;
DEFINE_CSS_TYPE_WRAPPER(TranslateX)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
// translateY() = translateY( <length-percentage> )
struct TranslateY {
    LengthPercentage<> value;

    bool operator==(const TranslateY&) const = default;
};
using TranslateYFunction = FunctionNotation<CSSValueTranslateY, TranslateY>;
DEFINE_CSS_TYPE_WRAPPER(TranslateY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
// translateZ() = translateZ( <length> )
struct TranslateZ {
    Length<> value;

    bool operator==(const TranslateZ&) const = default;
};
using TranslateZFunction = FunctionNotation<CSSValueTranslateZ, TranslateZ>;
DEFINE_CSS_TYPE_WRAPPER(TranslateZ)

// https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
// perspective() = perspective( [ <length [0,∞]> | none ] )
struct Perspective {
    std::variant<Length<Nonnegative>, None> value;

    bool operator==(const Perspective&) const = default;
};
using PerspectiveFunction = FunctionNotation<CSSValuePerspective, Perspective>;
DEFINE_CSS_TYPE_WRAPPER(Perspective)

// https://drafts.csswg.org/css-transforms-2/#typedef-transform-function
// <transform-function> = <matrix()> | <translate()> | <translateX()> | <translateY()> | <scale()> | <scaleX()> | <scaleY()> | <rotate()> | <skew()> | <skewX()> | <skewY()> | <matrix3d()> | <translate3d()> | <translateZ()> | <scale3d()> | <scaleZ()> | <rotate3d()> | <rotateX()> | <rotateY()> | <rotateZ()> | <perspective()
using TransformFunction = std::variant<
    MatrixFunction,
    Matrix3DFunction,
    RotateFunction,
    Rotate3DFunction,
    RotateXFunction,
    RotateYFunction,
    RotateZFunction,
    SkewFunction,
    SkewXFunction,
    SkewYFunction,
    ScaleFunction,
    Scale3DFunction,
    ScaleXFunction,
    ScaleYFunction,
    ScaleZFunction,
    TranslateFunction,
    Translate3DFunction,
    TranslateXFunction,
    TranslateYFunction,
    TranslateZFunction,
    PerspectiveFunction
>;

using TransformFunctionProxy = WTF::VariantListProxy<TransformFunction>;

// MARK: - Concepts

template<typename T> concept IsTransformFunction
    = std::same_as<T, TransformFunction>
   || std::same_as<T, TransformFunctionProxy>;

} // namespace CSS
} // namespace WebCore

CSS_TUPLE_LIKE_CONFORMANCE(Rotate3D, 4)
CSS_TUPLE_LIKE_CONFORMANCE(Skew, 2)
CSS_TUPLE_LIKE_CONFORMANCE(Scale, 2)
CSS_TUPLE_LIKE_CONFORMANCE(Scale3D, 3)
CSS_TUPLE_LIKE_CONFORMANCE(Translate, 2)
CSS_TUPLE_LIKE_CONFORMANCE(Translate3D, 3)
