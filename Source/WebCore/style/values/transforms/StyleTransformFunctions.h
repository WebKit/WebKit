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

#include "CSSTransformFunctions.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleTransformInterfaces.h"
#include "TransformFunctions.h"
#include "TransformListSharedPrimitivesPrefix.h"

namespace WebCore {
namespace Style {

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
    static constexpr auto name = CSSValueMatrix;
    using Platform = WebCore::Matrix;

    AffineTransform value;

    static Matrix identity() { return { .value = WebCore::identity }; }
    bool isIdentity() const { return value.isIdentity(); }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Platform toPlatform() const;

    bool operator==(const Matrix&) const = default;
};
using MatrixFunction = FunctionNotation<CSSValueMatrix, Matrix>;
DEFINE_STYLE_TYPE_WRAPPER(Matrix)

template<> struct ToCSS<Matrix> {
    auto operator()(const Matrix&, const RenderStyle&) -> CSS::Matrix;
};
template<> struct ToStyle<CSS::Matrix> {
    auto operator()(const CSS::Matrix&, const BuilderState&, const CSSCalcSymbolTable&) -> Matrix;
    auto operator()(const CSS::Matrix&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> Matrix;
};

template<> struct Blending<Matrix> {
    constexpr auto canBlend(const Matrix&, const Matrix&) -> bool { return true; }
    auto blend(const Matrix&, const Matrix&, const BlendingContext&) -> Matrix;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d
// matrix3d() = matrix3d( <number>#{16} )
struct Matrix3D {
    static constexpr auto name = CSSValueMatrix3d;
    using Platform = WebCore::Matrix3D;

    TransformationMatrix value;

    static Matrix3D identity() { return { .value = TransformationMatrix::identity }; }
    bool isIdentity() const { return value.isIdentity(); }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return value.isAffine(); }

    Platform toPlatform() const;

    bool operator==(const Matrix3D&) const = default;
};
using Matrix3DFunction = FunctionNotation<CSSValueMatrix3d, Matrix3D>;
DEFINE_STYLE_TYPE_WRAPPER(Matrix3D)

template<> struct ToCSS<Matrix3D> {
    auto operator()(const Matrix3D&, const RenderStyle&) -> CSS::Matrix3D;
};
template<> struct ToStyle<CSS::Matrix3D> {
    auto operator()(const CSS::Matrix3D&, const BuilderState&, const CSSCalcSymbolTable&) -> Matrix3D;
    auto operator()(const CSS::Matrix3D&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> Matrix3D;
};

template<> struct Blending<Matrix3D> {
    constexpr auto canBlend(const Matrix3D&, const Matrix3D&) -> bool { return true; }
    auto blend(const Matrix3D&, const Matrix3D&, const BlendingContext&) -> Matrix3D;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
// rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )
struct Rotate3D {
    static constexpr auto name = CSSValueRotate3d;
    using Platform = WebCore::Rotate3D;
    using Primitive3DTransform = Rotate3D;

    Number<> x;
    Number<> y;
    Number<> z;
    Angle<> angle;

    static Rotate3D identity() { return { .x = { 0 }, .y = { 0 }, .z = { 1 }, .angle = { 0 } }; }
    bool isIdentity() const { return angle == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return (x == 0 && y == 0) || angle == 0; }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    Platform toPlatform() const;

    constexpr bool operator==(const Rotate3D&) const = default;
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
DEFINE_CSS_STYLE_MAPPING(CSS::Rotate3D, Rotate3D)

template<> struct Blending<Rotate3D> {
    constexpr auto canBlend(const Rotate3D&, const Rotate3D&) -> bool { return true; }
    auto blend(const Rotate3D&, const Rotate3D&, const BlendingContext&) -> Rotate3D;
};

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-rotate
// rotate() = rotate( [ <angle> | <zero> ] )
struct Rotate {
    static constexpr auto name = CSSValueRotate;
    using Platform = WebCore::Rotate;
    using Primitive2DTransform = Rotate;
    using Primitive3DTransform = Rotate3D;

    Angle<> value;

    static Rotate identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Primitive2DTransform toPrimitive2DTransform() const { return *this; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 0 }, .y = { 0 }, .z = { 1 }, .angle = value }; }

    Platform toPlatform() const;

    constexpr bool operator==(const Rotate&) const = default;
};
using RotateFunction = FunctionNotation<CSSValueRotate, Rotate>;
DEFINE_STYLE_TYPE_WRAPPER(Rotate)
DEFINE_CSS_STYLE_MAPPING(CSS::Rotate, Rotate)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
// rotateX() = rotateX( [ <angle> | <zero> ] )
struct RotateX {
    static constexpr auto name = CSSValueRotateX;
    using Platform = WebCore::Rotate3D;
    using Primitive3DTransform = Rotate3D;

    Angle<> value;

    static RotateX identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return isIdentity(); }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 1 }, .y = { 0 }, .z = { 0 }, .angle = value }; }

    Platform toPlatform() const;

    constexpr bool operator==(const RotateX&) const = default;
};
using RotateXFunction = FunctionNotation<CSSValueRotateX, RotateX>;
DEFINE_STYLE_TYPE_WRAPPER(RotateX)
DEFINE_CSS_STYLE_MAPPING(CSS::RotateX, RotateX)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
// rotateY() = rotateY( [ <angle> | <zero> ] )
struct RotateY {
    static constexpr auto name = CSSValueRotateY;
    using Platform = WebCore::Rotate3D;
    using Primitive3DTransform = Rotate3D;

    Angle<> value;

    static RotateY identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return isIdentity(); }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 0 }, .y = { 1 }, .z = { 0 }, .angle = value }; }

    Platform toPlatform() const;

    constexpr bool operator==(const RotateY&) const = default;
};
using RotateYFunction = FunctionNotation<CSSValueRotateY, RotateY>;
DEFINE_STYLE_TYPE_WRAPPER(RotateY)
DEFINE_CSS_STYLE_MAPPING(CSS::RotateY, RotateY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez
// rotateZ() = rotateZ( [ <angle> | <zero> ] )
struct RotateZ {
    static constexpr auto name = CSSValueRotateZ;
    using Platform = WebCore::Rotate3D;
    using Primitive3DTransform = Rotate3D;

    Angle<> value;

    static RotateZ identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 0 }, .y = { 0 }, .z = { 1 }, .angle = value }; }

    Platform toPlatform() const;

    constexpr bool operator==(const RotateZ&) const = default;
};
using RotateZFunction = FunctionNotation<CSSValueRotateZ, RotateZ>;
DEFINE_STYLE_TYPE_WRAPPER(RotateZ)
DEFINE_CSS_STYLE_MAPPING(CSS::RotateZ, RotateZ)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skew
// skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
struct Skew {
    static constexpr auto name = CSSValueSkew;
    using Platform = WebCore::Skew;

    Angle<> x;
    Angle<> y;

    static Skew identity() { return { .x = { 0 }, .y = { 0 } }; }
    bool isIdentity() const { return x == 0 && y == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Platform toPlatform() const;

    constexpr bool operator==(const Skew&) const = default;
};
using SkewFunction = FunctionNotation<CSSValueSkew, Skew>;

template<size_t I> const auto& get(const Skew& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
}

template<> struct ToCSS<Skew> {
    auto operator()(const Skew&, const RenderStyle&) -> CSS::Skew;
};
template<> struct ToStyle<CSS::Skew> {
    auto operator()(const CSS::Skew&, const BuilderState&, const CSSCalcSymbolTable&) -> Skew;
    auto operator()(const CSS::Skew&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> Skew;
};

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewx
// skewX() = skewX( [ <angle> | <zero> ] )
struct SkewX {
    static constexpr auto name = CSSValueSkewX;
    using Platform = WebCore::SkewX;

    Angle<> value;

    static SkewX identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Platform toPlatform() const;

    constexpr bool operator==(const SkewX&) const = default;
};
using SkewXFunction = FunctionNotation<CSSValueSkewX, SkewX>;
DEFINE_STYLE_TYPE_WRAPPER(SkewX)
DEFINE_CSS_STYLE_MAPPING(CSS::SkewX, SkewX)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-skewy
// skewY() = skewY( [ <angle> | <zero> ] )
struct SkewY {
    static constexpr auto name = CSSValueSkewY;
    using Platform = WebCore::SkewY;

    Angle<> value;

    static SkewY identity() { return { .value = { 0 } }; }
    bool isIdentity() const { return value == 0; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Platform toPlatform() const;

    constexpr bool operator==(const SkewY&) const = default;
};
using SkewYFunction = FunctionNotation<CSSValueSkewY, SkewY>;
DEFINE_STYLE_TYPE_WRAPPER(SkewY)
DEFINE_CSS_STYLE_MAPPING(CSS::SkewY, SkewY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-scale3d
// scale3d() = scale3d( [ <number> | <percentage> ]#{3} )
struct Scale3D {
    static constexpr auto name = CSSValueScale3d;
    using Platform = WebCore::Scale3D;
    using Primitive3DTransform = Scale3D;

    NumberOrPercentageResolvedToNumber x;
    NumberOrPercentageResolvedToNumber y;
    NumberOrPercentageResolvedToNumber z;

    static Scale3D identity() { return { .x = { 1 }, .y = { 1 }, .z = { 1 } }; }
    bool isIdentity() const { return x == 1 && y == 1 && z == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return z == 1; }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    Platform toPlatform() const;

    constexpr bool operator==(const Scale3D&) const = default;
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
DEFINE_CSS_STYLE_MAPPING(CSS::Scale3D, Scale3D)

template<> struct Blending<Scale3D> {
    constexpr auto canBlend(const Scale3D&, const Scale3D&) -> bool { return true; }
    auto blend(const Scale3D&, const Scale3D&, const BlendingContext&) -> Scale3D;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-scale
// scale() = scale( [ <number> | <percentage> ]#{1,2} )
struct Scale {
    static constexpr auto name = CSSValueScale;
    using Platform = WebCore::Scale;
    using Primitive2DTransform = Scale;
    using Primitive3DTransform = Scale3D;

    NumberOrPercentageResolvedToNumber x;
    NumberOrPercentageResolvedToNumber y;

    static Scale identity() { return { .x = { 1 }, .y = { 1 } }; }
    bool isIdentity() const { return x == 1 && y == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Primitive2DTransform toPrimitive2DTransform() const { return *this; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = x, .y = y, .z = { 1 } }; }

    Platform toPlatform() const;

    constexpr bool operator==(const Scale&) const = default;
};
using ScaleFunction = FunctionNotation<CSSValueScale, Scale>;

template<size_t I> const auto& get(const Scale& value)
{
    if constexpr (!I)
        return value.x;
    else if constexpr (I == 1)
        return value.y;
}

template<> struct ToCSS<Scale> {
    auto operator()(const Scale&, const RenderStyle&) -> CSS::Scale;
};
template<> struct ToStyle<CSS::Scale> {
    auto operator()(const CSS::Scale&, const BuilderState&, const CSSCalcSymbolTable&) -> Scale;
    auto operator()(const CSS::Scale&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> Scale;
};

template<> struct Blending<Scale> {
    constexpr auto canBlend(const Scale&, const Scale&) -> bool { return true; }
    auto blend(const Scale&, const Scale&, const BlendingContext&) -> Scale;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-scalex
// scaleX() = scaleX( [ <number> | <percentage> ] )
struct ScaleX {
    static constexpr auto name = CSSValueScaleX;
    using Platform = WebCore::Scale;
    using Primitive2DTransform = Scale;
    using Primitive3DTransform = Scale3D;

    NumberOrPercentageResolvedToNumber value;

    static ScaleX identity() { return { .value = { 1 } }; }
    bool isIdentity() const { return value == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Primitive2DTransform toPrimitive2DTransform() const { return { .x = value, .y = { 1 } }; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = value, .y = { 1 }, .z = { 1 } }; }

    Platform toPlatform() const;

    constexpr bool operator==(const ScaleX&) const = default;
};
using ScaleXFunction = FunctionNotation<CSSValueScaleX, ScaleX>;
DEFINE_STYLE_TYPE_WRAPPER(ScaleX)
DEFINE_CSS_STYLE_MAPPING(CSS::ScaleX, ScaleX)

template<> struct Blending<ScaleX> {
    constexpr auto canBlend(const ScaleX&, const ScaleX&) -> bool { return true; }
    auto blend(const ScaleX&, const ScaleX&, const BlendingContext&) -> ScaleX;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-scaley
// scaleY() = scaleY( [ <number> | <percentage> ] )
struct ScaleY {
    static constexpr auto name = CSSValueScaleY;
    using Platform = WebCore::Scale;
    using Primitive2DTransform = Scale;
    using Primitive3DTransform = Scale3D;

    NumberOrPercentageResolvedToNumber value;

    static ScaleY identity() { return { .value = { 1 } }; }
    bool isIdentity() const { return value == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return true; }

    Primitive2DTransform toPrimitive2DTransform() const { return { .x = { 1 }, .y = value }; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 1 }, .y = value, .z = { 1 } }; }

    Platform toPlatform() const;

    constexpr bool operator==(const ScaleY&) const = default;
};
using ScaleYFunction = FunctionNotation<CSSValueScaleY, ScaleY>;
DEFINE_STYLE_TYPE_WRAPPER(ScaleY)
DEFINE_CSS_STYLE_MAPPING(CSS::ScaleY, ScaleY)

template<> struct Blending<ScaleY> {
    constexpr auto canBlend(const ScaleY&, const ScaleY&) -> bool { return true; }
    auto blend(const ScaleY&, const ScaleY&, const BlendingContext&) -> ScaleY;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-scalez
// scaleZ() = scaleZ( [ <number> | <percentage> ] )
struct ScaleZ {
    static constexpr auto name = CSSValueScaleZ;
    using Platform = WebCore::Scale3D;
    using Primitive3DTransform = Scale3D;

    NumberOrPercentageResolvedToNumber value;

    static ScaleZ identity() { return { .value = { 1 } }; }
    bool isIdentity() const { return value == 1; }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    bool isRepresentableIn2D() const { return isIdentity(); }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = { 1 }, .y = { 1 }, .z = value }; }

    Platform toPlatform() const;

    constexpr bool operator==(const ScaleZ&) const = default;
};
using ScaleZFunction = FunctionNotation<CSSValueScaleZ, ScaleZ>;
DEFINE_STYLE_TYPE_WRAPPER(ScaleZ)
DEFINE_CSS_STYLE_MAPPING(CSS::ScaleZ, ScaleZ)

template<> struct Blending<ScaleZ> {
    constexpr auto canBlend(const ScaleZ&, const ScaleZ&) -> bool { return true; }
    auto blend(const ScaleZ&, const ScaleZ&, const BlendingContext&) -> ScaleZ;
};

// https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
// translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )
struct Translate3D {
    static constexpr auto name = CSSValueTranslate3d;
    using Platform = WebCore::Translate3D;
    using Primitive3DTransform = Translate3D;

    LengthPercentage<> x;
    LengthPercentage<> y;
    Length<> z;

    static Translate3D identity() { return { .x = Length<> { 0 }, .y = Length<> { 0 }, .z = Length<> { 0 } }; }
    bool isIdentity() const;
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    bool isRepresentableIn2D() const { return z == 0; }
    bool isWidthDependent() const { return !x.isLength(); }
    bool isHeightDependent() const { return !y.isLength(); }

    Primitive3DTransform toPrimitive3DTransform() const { return *this; }

    Platform toPlatform(const FloatSize& referenceBox) const;

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

DEFINE_CSS_STYLE_MAPPING(CSS::Translate3D, Translate3D)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translate
// translate() = translate( <length-percentage> , <length-percentage>? )
struct Translate {
    static constexpr auto name = CSSValueTranslate;
    using Platform = WebCore::Translate;
    using Primitive2DTransform = Translate;
    using Primitive3DTransform = Translate3D;

    LengthPercentage<> x;
    LengthPercentage<> y;

    static Translate identity() { return { .x = Length<> { 0 }, .y = Length<> { 0 } }; }
    bool isIdentity() const;
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    constexpr bool isRepresentableIn2D() const { return true; }
    bool isWidthDependent() const { return !x.isLength(); }
    bool isHeightDependent() const { return !y.isLength(); }

    Primitive2DTransform toPrimitive2DTransform() const { return *this; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = x, .y = y, .z = { 0 } }; }

    Platform toPlatform(const FloatSize& referenceBox) const;

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

template<> struct ToCSS<Translate> {
    auto operator()(const Translate&, const RenderStyle&) -> CSS::Translate;
};
template<> struct ToStyle<CSS::Translate> {
    auto operator()(const CSS::Translate&, const BuilderState&, const CSSCalcSymbolTable&) -> Translate;
    auto operator()(const CSS::Translate&, const CSSToLengthConversionData&, const CSSCalcSymbolTable&) -> Translate;
};

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatex
// translateX() = translateX( <length-percentage> )
struct TranslateX {
    static constexpr auto name = CSSValueTranslateX;
    using Platform = WebCore::Translate;
    using Primitive2DTransform = Translate;
    using Primitive3DTransform = Translate3D;

    LengthPercentage<> value;

    static TranslateX identity() { return { .value = Length<> { 0 } }; }
    bool isIdentity() const;
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    constexpr bool isRepresentableIn2D() const { return true; }
    bool isWidthDependent() const { return !value.isLength(); }

    Primitive2DTransform toPrimitive2DTransform() const { return { .x = value, .y = Length<> { 0 } }; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = value, .y = Length<> { 0 }, .z = Length<> { 0 } }; }

    Platform toPlatform(const FloatSize& referenceBox) const;

    bool operator==(const TranslateX&) const = default;
};
using TranslateXFunction = FunctionNotation<CSSValueTranslateX, TranslateX>;
DEFINE_STYLE_TYPE_WRAPPER(TranslateX)
DEFINE_CSS_STYLE_MAPPING(CSS::TranslateX, TranslateX)

// https://drafts.csswg.org/css-transforms-1/#funcdef-transform-translatey
// translateY() = translateY( <length-percentage> )
struct TranslateY {
    static constexpr auto name = CSSValueTranslateY;
    using Platform = WebCore::Translate;
    using Primitive2DTransform = Translate;
    using Primitive3DTransform = Translate3D;

    LengthPercentage<> value;

    static TranslateY identity() { return { .value = Length<> { 0 } }; }
    bool isIdentity() const;
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    constexpr bool isRepresentableIn2D() const { return true; }
    bool isHeightDependent() const { return !value.isLength(); }

    Primitive2DTransform toPrimitive2DTransform() const { return { .x = Length<> { 0 }, .y = value }; }
    Primitive3DTransform toPrimitive3DTransform() const { return { .x = Length<> { 0 }, .y = value, .z = Length<> { 0 } }; }

    Platform toPlatform(const FloatSize& referenceBox) const;

    bool operator==(const TranslateY&) const = default;
};
using TranslateYFunction = FunctionNotation<CSSValueTranslateY, TranslateY>;
DEFINE_STYLE_TYPE_WRAPPER(TranslateY)
DEFINE_CSS_STYLE_MAPPING(CSS::TranslateY, TranslateY)

// https://drafts.csswg.org/css-transforms-2/#funcdef-translatez
// translateZ() = translateZ( <length> )
struct TranslateZ {
    static constexpr auto name = CSSValueTranslateZ;
    using Platform = WebCore::Translate3D;
    using Primitive3DTransform = Translate3D;

    Length<> value;

    static TranslateZ identity() { return { .value = Length<> { 0 } }; }
    bool isIdentity() const { return value == 0; }
    constexpr bool isAffectedByTransformOrigin() const { return false; }
    constexpr bool isRepresentableIn2D() const { return value == 0; }

    Primitive3DTransform toPrimitive3DTransform() const { return { .x = Length<> { 0 }, .y = Length<> { 0 }, .z = value }; }

    Platform toPlatform(const FloatSize& referenceBox) const;

    constexpr bool operator==(const TranslateZ&) const = default;
};
using TranslateZFunction = FunctionNotation<CSSValueTranslateZ, TranslateZ>;
DEFINE_STYLE_TYPE_WRAPPER(TranslateZ)
DEFINE_CSS_STYLE_MAPPING(CSS::TranslateZ, TranslateZ)

// https://drafts.csswg.org/css-transforms-2/#funcdef-perspective
// perspective() = perspective( [ <length [0,∞]> | none ] )
struct Perspective {
    static constexpr auto name = CSSValuePerspective;
    using Platform = WebCore::Perspective;

    std::variant<Length<CSS::Nonnegative>, None> value;

    static Perspective identity() { return { .value = None { } }; }
    bool isIdentity() const { return std::holds_alternative<None>(value); }
    bool isAffectedByTransformOrigin() const { return !isIdentity(); }
    constexpr bool isRepresentableIn2D() const { return false; }

    Platform toPlatform() const;

    constexpr bool operator==(const Perspective&) const = default;
};
using PerspectiveFunction = FunctionNotation<CSSValuePerspective, Perspective>;
DEFINE_STYLE_TYPE_WRAPPER(Perspective)
DEFINE_CSS_STYLE_MAPPING(CSS::Perspective, Perspective)

template<> struct Blending<Perspective> {
    constexpr auto canBlend(const Perspective&, const Perspective&) -> bool { return true; }
    auto blend(const Perspective&, const Perspective&, const BlendingContext&) -> Perspective;
};

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

template<typename T> concept Is3D
    = std::same_as<T, ScaleZFunction>
   || std::same_as<T, ScaleZ>
   || std::same_as<T, Scale3DFunction>
   || std::same_as<T, Scale3D>
   || std::same_as<T, TranslateZFunction>
   || std::same_as<T, TranslateZ>
   || std::same_as<T, Translate3DFunction>
   || std::same_as<T, Translate3D>
   || std::same_as<T, RotateXFunction>
   || std::same_as<T, RotateX>
   || std::same_as<T, RotateYFunction>
   || std::same_as<T, RotateY>
   || std::same_as<T, Rotate3DFunction>
   || std::same_as<T, Rotate3D>
   || std::same_as<T, Matrix3DFunction>
   || std::same_as<T, Matrix3D>
   || std::same_as<T, PerspectiveFunction>
   || std::same_as<T, Perspective>;

inline bool is3D(const IsTransformFunction auto& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return Is3D<T>; });
}

template<typename T> concept IsRotate
    = std::same_as<T, RotateXFunction>
   || std::same_as<T, RotateX>
   || std::same_as<T, RotateYFunction>
   || std::same_as<T, RotateY>
   || std::same_as<T, RotateZFunction>
   || std::same_as<T, RotateZ>
   || std::same_as<T, RotateFunction>
   || std::same_as<T, Rotate>
   || std::same_as<T, Rotate3DFunction>
   || std::same_as<T, Rotate3D>;

inline bool isRotate(const IsTransformFunction auto& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsRotate<T>; });
}

template<typename T> concept IsScale
    = std::same_as<T, ScaleXFunction>
   || std::same_as<T, ScaleX>
   || std::same_as<T, ScaleYFunction>
   || std::same_as<T, ScaleY>
   || std::same_as<T, ScaleZFunction>
   || std::same_as<T, ScaleZ>
   || std::same_as<T, ScaleFunction>
   || std::same_as<T, Scale>
   || std::same_as<T, Scale3DFunction>
   || std::same_as<T, Scale3D>;

inline bool isScale(const IsTransformFunction auto& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsScale<T>; });
}

template<typename T> concept IsTranslate
    = std::same_as<T, TranslateXFunction>
   || std::same_as<T, TranslateX>
   || std::same_as<T, TranslateYFunction>
   || std::same_as<T, TranslateY>
   || std::same_as<T, TranslateZFunction>
   || std::same_as<T, TranslateZ>
   || std::same_as<T, TranslateFunction>
   || std::same_as<T, Translate>
   || std::same_as<T, Translate3DFunction>
   || std::same_as<T, Translate3D>;

inline bool isTranslate(const IsTransformFunction auto& function)
{
    return WTF::switchOn(function, []<typename T>(const T&) { return IsTranslate<T>; });
}

// MARK: - Blending

// Returns whether the two functions share a common primitive.
// https://drafts.csswg.org/css-transforms-1/#transform-primitives
bool haveSharedPrimitiveType(const TransformFunction&, const TransformFunction&);
bool haveSharedPrimitiveType(const TransformFunctionProxy&, const TransformFunctionProxy&);

template<> struct Blending<TransformFunction> {
    auto canBlend(const TransformFunction&, const TransformFunction&) -> bool;
    auto blend(const TransformFunction&, const TransformFunction&, const BlendingContext&) -> TransformFunction;
};

// MARK: - Conversion

// A specialization of ToCSS for TransformFunction is needed to convert affine CSS::Matrix3D functions into CSS::Matrix.
template<> struct ToCSS<TransformFunction> { auto operator()(const TransformFunction&, const RenderStyle&) -> CSS::TransformFunction; };

// MARK: - ToPlatfrom

template<IsTransformFunction T> struct ToPlatform<T> {
    decltype(auto) operator()(const T& value, const FloatSize& referenceBox)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> WebCore::TransformFunction { return toPlatform(alternative, referenceBox); });
    }
};

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream&, const Matrix&);
WTF::TextStream& operator<<(WTF::TextStream&, const Matrix3D&);
WTF::TextStream& operator<<(WTF::TextStream&, const Rotate&);
WTF::TextStream& operator<<(WTF::TextStream&, const Rotate3D&);
WTF::TextStream& operator<<(WTF::TextStream&, const RotateX&);
WTF::TextStream& operator<<(WTF::TextStream&, const RotateY&);
WTF::TextStream& operator<<(WTF::TextStream&, const RotateZ&);
WTF::TextStream& operator<<(WTF::TextStream&, const Skew&);
WTF::TextStream& operator<<(WTF::TextStream&, const SkewX&);
WTF::TextStream& operator<<(WTF::TextStream&, const SkewY&);
WTF::TextStream& operator<<(WTF::TextStream&, const Scale&);
WTF::TextStream& operator<<(WTF::TextStream&, const Scale3D&);
WTF::TextStream& operator<<(WTF::TextStream&, const ScaleX&);
WTF::TextStream& operator<<(WTF::TextStream&, const ScaleY&);
WTF::TextStream& operator<<(WTF::TextStream&, const ScaleZ&);
WTF::TextStream& operator<<(WTF::TextStream&, const Translate&);
WTF::TextStream& operator<<(WTF::TextStream&, const Translate3D&);
WTF::TextStream& operator<<(WTF::TextStream&, const TranslateX&);
WTF::TextStream& operator<<(WTF::TextStream&, const TranslateY&);
WTF::TextStream& operator<<(WTF::TextStream&, const TranslateZ&);
WTF::TextStream& operator<<(WTF::TextStream&, const Perspective&);

} // namespace Style

// Conformance for `PrimitiveTransformType`. Used to adapt Style types for `TransformListSharedPrimitivesPrefix`.
template<> struct PrimitiveTransformType<Style::TransformFunction> { TransformFunctionType operator()(const Style::TransformFunction&); };
template<> struct PrimitiveTransformType<Style::TransformFunctionProxy> { TransformFunctionType operator()(const Style::TransformFunctionProxy&); };

} // namespace WebCore

STYLE_TUPLE_LIKE_CONFORMANCE(Rotate3D, 4)
STYLE_TUPLE_LIKE_CONFORMANCE(Skew, 2)
STYLE_TUPLE_LIKE_CONFORMANCE(Scale, 2)
STYLE_TUPLE_LIKE_CONFORMANCE(Scale3D, 3)
STYLE_TUPLE_LIKE_CONFORMANCE(Translate, 2)
STYLE_TUPLE_LIKE_CONFORMANCE(Translate3D, 3)
