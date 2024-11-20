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

#include "config.h"
#include "StyleTransformFunctions.h"

#include "CSSCalcValue.h"
#include "FloatConversion.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

// MARK: - Matrix

auto ToCSS<Matrix>::operator()(const Matrix& value, const RenderStyle& style) -> CSS::Matrix
{
    double zoom = style.usedZoom();

    return {
        .value {
            CSS::Number<> { CSS::NumberRaw<> { value.value.a() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.b() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.c() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.d() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.e() / zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.f() / zoom } },
        }
    };
}

auto ToStyle<CSS::Matrix>::operator()(const CSS::Matrix& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Matrix
{
    auto conversionData = state.useSVGZoomRulesForLength()
        ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0)
        : state.cssToLengthConversionData();

    return toStyle(value, conversionData, symbolTable);
}

auto ToStyle<CSS::Matrix>::operator()(const CSS::Matrix& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Matrix
{
    double zoom = conversionData.zoom();

    return {
        .value = AffineTransform {
            toStyle(get<0>(value.value), conversionData, symbolTable).value,
            toStyle(get<1>(value.value), conversionData, symbolTable).value,
            toStyle(get<2>(value.value), conversionData, symbolTable).value,
            toStyle(get<3>(value.value), conversionData, symbolTable).value,
            toStyle(get<4>(value.value), conversionData, symbolTable).value * zoom,
            toStyle(get<5>(value.value), conversionData, symbolTable).value * zoom
        }
    };
}

// MARK: - Matrix3D

auto ToCSS<Matrix3D>::operator()(const Matrix3D& value, const RenderStyle& style) -> CSS::Matrix3D
{
    double zoom = style.usedZoom();

    return {
        .value = {
            CSS::Number<> { CSS::NumberRaw<> { value.value.m11() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m12() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m13() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m14() * zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m21() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m22() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m23() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m24() * zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m31() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m32() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m33() } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m34() * zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m41() / zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m42() / zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m43() / zoom } },
            CSS::Number<> { CSS::NumberRaw<> { value.value.m44() } }
        }
    };
}

auto ToStyle<CSS::Matrix3D>::operator()(const CSS::Matrix3D& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Matrix3D
{
    auto conversionData = state.useSVGZoomRulesForLength()
        ? state.cssToLengthConversionData().copyWithAdjustedZoom(1.0)
        : state.cssToLengthConversionData();

    return toStyle(value, conversionData, symbolTable);
}

auto ToStyle<CSS::Matrix3D>::operator()(const CSS::Matrix3D& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Matrix3D
{
    double zoom = conversionData.zoom();

    return {
        .value = TransformationMatrix {
            toStyle(get<0>(value.value), conversionData, symbolTable).value,
            toStyle(get<1>(value.value), conversionData, symbolTable).value,
            toStyle(get<2>(value.value), conversionData, symbolTable).value,
            toStyle(get<3>(value.value), conversionData, symbolTable).value / zoom,
            toStyle(get<4>(value.value), conversionData, symbolTable).value,
            toStyle(get<5>(value.value), conversionData, symbolTable).value,
            toStyle(get<6>(value.value), conversionData, symbolTable).value,
            toStyle(get<7>(value.value), conversionData, symbolTable).value / zoom,
            toStyle(get<8>(value.value), conversionData, symbolTable).value,
            toStyle(get<9>(value.value), conversionData, symbolTable).value,
            toStyle(get<10>(value.value), conversionData, symbolTable).value,
            toStyle(get<11>(value.value), conversionData, symbolTable).value / zoom,
            toStyle(get<12>(value.value), conversionData, symbolTable).value * zoom,
            toStyle(get<13>(value.value), conversionData, symbolTable).value * zoom,
            toStyle(get<14>(value.value), conversionData, symbolTable).value * zoom,
            toStyle(get<15>(value.value), conversionData, symbolTable).value
        }
    };
}

// MARK: - Skew

auto ToCSS<Skew>::operator()(const Skew& value, const RenderStyle& style) -> CSS::Skew
{
    return {
        .x = toCSS(value.x, style),
        .y = value.y.value ? std::make_optional(toCSS(value.y, style)) : std::nullopt,
    };
}

auto ToStyle<CSS::Skew>::operator()(const CSS::Skew& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Skew
{
    return toStyle(value, state.cssToLengthConversionData(), symbolTable);
}

auto ToStyle<CSS::Skew>::operator()(const CSS::Skew& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Skew
{
    return {
        .x = toStyle(value.x, conversionData, symbolTable),
        .y = value.y ? toStyle(*value.y, conversionData, symbolTable) : Angle<> { 0 },
    };
}

// MARK: - Scale

auto ToCSS<Scale>::operator()(const Scale& value, const RenderStyle& style) -> CSS::Scale
{
    return {
        .x = toCSS(value.x, style),
        .y = value.x != value.y ? std::make_optional(toCSS(value.y, style)) : std::nullopt,
    };
}

auto ToStyle<CSS::Scale>::operator()(const CSS::Scale& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Scale
{
    return toStyle(value, state.cssToLengthConversionData(), symbolTable);
}

auto ToStyle<CSS::Scale>::operator()(const CSS::Scale& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Scale
{
    auto x = toStyle(value.x, conversionData, symbolTable);
    auto y = value.y ? toStyle(*value.y, conversionData, symbolTable) : x;
    return {
        .x = WTFMove(x),
        .y = WTFMove(y),
    };
}

// MARK: - Translate

auto ToCSS<Translate>::operator()(const Translate& value, const RenderStyle& style) -> CSS::Translate
{
    return {
        .x = toCSS(value.x, style),
        .y = !value.y.isLength() || !value.y.value.isZero() ? std::make_optional(toCSS(value.y, style)) : std::nullopt,
    };
}

auto ToStyle<CSS::Translate>::operator()(const CSS::Translate& value, const BuilderState& state, const CSSCalcSymbolTable& symbolTable) -> Translate
{
    return toStyle(value, state.cssToLengthConversionData(), symbolTable);
}

auto ToStyle<CSS::Translate>::operator()(const CSS::Translate& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> Translate
{
    return {
        .x = toStyle(value.x, conversionData, symbolTable),
        .y = value.y ? toStyle(*value.y, conversionData, symbolTable) : LengthPercentage<> { Length<> { 0 } },
    };
}

// MARK: - ToPlatform

// MARK: Matrix

auto Matrix::toPlatform() const -> Matrix::Platform
{
    return { value };
}

// MARK: Matrix3D

auto Matrix3D::toPlatform() const -> Matrix3D::Platform
{
    return { value };
}

// MARK: Rotate3D

auto Rotate3D::toPlatform() const -> Rotate3D::Platform
{
    return { x.value, y.value, z.value, angle.value };
}

// MARK: Rotate

auto Rotate::toPlatform() const -> Rotate::Platform
{
    return { value.value };
}

// MARK: RotateX

auto RotateX::toPlatform() const -> RotateX::Platform
{
    return toPrimitive3DTransform().toPlatform();
}

// MARK: RotateY

auto RotateY::toPlatform() const -> RotateY::Platform
{
    return toPrimitive3DTransform().toPlatform();
}

// MARK: RotateZ

auto RotateZ::toPlatform() const -> RotateZ::Platform
{
    return toPrimitive3DTransform().toPlatform();
}

// MARK: Skew

auto Skew::toPlatform() const -> Skew::Platform
{
    return { x.value, y.value };
}

// MARK: SkewX

auto SkewX::toPlatform() const -> SkewX::Platform
{
    return { value.value };
}

// MARK: SkewY

auto SkewY::toPlatform() const -> SkewY::Platform
{
    return { value.value };
}

// MARK: Scale3D

auto Scale3D::toPlatform() const -> Scale3D::Platform
{
    return { x.value, y.value, z.value };
}

// MARK: Scale

auto Scale::toPlatform() const -> Scale::Platform
{
    return { x.value, y.value };
}

// MARK: ScaleX

auto ScaleX::toPlatform() const -> ScaleX::Platform
{
    return toPrimitive2DTransform().toPlatform();
}

// MARK: ScaleY

auto ScaleY::toPlatform() const -> ScaleY::Platform
{
    return toPrimitive2DTransform().toPlatform();
}

// MARK: ScaleZ

auto ScaleZ::toPlatform() const -> ScaleZ::Platform
{
    return toPrimitive3DTransform().toPlatform();
}

// MARK: Translate3D

bool Translate3D::isIdentity() const
{
    return Style::evaluate(x, 1.0f) == 0.0f && Style::evaluate(y, 1.0f) == 0.0f && z == 0.0f;
}

auto Translate3D::toPlatform(const FloatSize& referenceBox) const -> Translate3D::Platform
{
    return { Style::evaluate(x, referenceBox.width()), Style::evaluate(y, referenceBox.height()), z.value };
}

// MARK: Translate

bool Translate::isIdentity() const
{
    return Style::evaluate(x, 1.0f) == 0.0f && Style::evaluate(y, 1.0f) == 0.0f;
}

auto Translate::toPlatform(const FloatSize& referenceBox) const -> Translate::Platform
{
    return { Style::evaluate(x, referenceBox.width()), Style::evaluate(y, referenceBox.height()) };
}

// MARK: TranslateX

bool TranslateX::isIdentity() const
{
    return Style::evaluate(value, 1.0f) == 0.0f;
}

auto TranslateX::toPlatform(const FloatSize& referenceBox) const -> TranslateX::Platform
{
    return toPrimitive2DTransform().toPlatform(referenceBox);
}

// MARK: TranslateY

bool TranslateY::isIdentity() const
{
    return Style::evaluate(value, 1.0f) == 0.0f;
}

auto TranslateY::toPlatform(const FloatSize& referenceBox) const -> TranslateY::Platform
{
    return toPrimitive2DTransform().toPlatform(referenceBox);
}

// MARK: TranslateZ

auto TranslateZ::toPlatform(const FloatSize& referenceBox) const -> TranslateZ::Platform
{
    return toPrimitive3DTransform().toPlatform(referenceBox);
}

// MARK: Perspective

auto Perspective::toPlatform() const -> Perspective::Platform
{
    return WTF::switchOn(value,
        [](const Length<CSS::Nonnegative>& length) -> Perspective::Platform {
            return { length.value };
        },
        [](None) -> Perspective::Platform {
            return { std::nullopt };
        }
    );
}

// MARK: - Blending

// MARK: Matrix

Matrix Blending<Matrix>::blend(const Matrix& a, const Matrix& b, const BlendingContext& context)
{
    auto blended = WebCore::blend(a.toPlatform(), b.toPlatform(), context);
    return { blended.value };
}

// MARK: Matrix3D

auto Blending<Matrix3D>::blend(const Matrix3D& a, const Matrix3D& b, const BlendingContext& context) -> Matrix3D
{
    auto blended = WebCore::blend(a.toPlatform(), b.toPlatform(), context);
    return { blended.value };
}

// MARK: Rotate3D

auto Blending<Rotate3D>::blend(const Rotate3D& a, const Rotate3D& b, const BlendingContext& context) -> Rotate3D
{
    auto blended = WebCore::blend(a.toPlatform(), b.toPlatform(), context);
    return { { blended.x }, { blended.y }, { blended.z }, { blended.angle } };
}

// MARK: Scale3D

Scale3D Blending<Scale3D>::blend(const Scale3D& a, const Scale3D& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.x.value, b.x.value, context),
        blendScaleComponent(a.y.value, b.y.value, context),
        blendScaleComponent(a.z.value, b.z.value, context),
    };
}

// MARK: Scale

Scale Blending<Scale>::blend(const Scale& a, const Scale& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.x.value, b.x.value, context),
        blendScaleComponent(a.y.value, b.y.value, context),
    };
}

// MARK: ScaleX

ScaleX Blending<ScaleX>::blend(const ScaleX& a, const ScaleX& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.value.value, b.value.value, context),
    };
}

// MARK: ScaleY

ScaleY Blending<ScaleY>::blend(const ScaleY& a, const ScaleY& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.value.value, b.value.value, context),
    };
}

// MARK: ScaleZ

ScaleZ Blending<ScaleZ>::blend(const ScaleZ& a, const ScaleZ& b, const BlendingContext& context)
{
    return {
        blendScaleComponent(a.value.value, b.value.value, context),
    };
}

// MARK: Perspective

Perspective Blending<Perspective>::blend(const Perspective& a, const Perspective& b, const BlendingContext& context)
{
    auto blended = WebCore::blend(a.toPlatform(), b.toPlatform(), context);
    if (blended.value)
        return Perspective { Length<CSS::Nonnegative> { narrowPrecisionToFloat(*blended.value) } };
    return Perspective { None { } };
}

// MARK: TransformFunction

bool haveSharedPrimitiveType(const TransformFunction& a, const TransformFunction& b)
{
    return std::visit(WTF::makeVisitor(
        []<CSSValueID NameA, typename A, CSSValueID NameB, typename B>(const FunctionNotation<NameA, A>&, const FunctionNotation<NameB, B>&) -> bool {
            return std::is_same_v<A, B> || HaveSharedPrimitive2DTransform<A, B>() || HaveSharedPrimitive3DTransform<A, B>();
        }
    ), a, b);
}

bool haveSharedPrimitiveType(const TransformFunctionProxy& a, const TransformFunctionProxy& b)
{
    // FIXME: Make a version of std::visit that works with VariantListProxy objects.
    return haveSharedPrimitiveType(a.asVariant(), b.asVariant());
}

auto Blending<TransformFunction>::canBlend(const TransformFunction& a, const TransformFunction& b) -> bool
{
    return std::visit(WTF::makeVisitor(
        []<CSSValueID NameA, typename A, CSSValueID NameB, typename B>(const FunctionNotation<NameA, A>& a, const FunctionNotation<NameB, B>& b) -> bool {
            if constexpr (std::is_same_v<A, B>)
                return WebCore::Style::canBlend(*a, *b);
            else if constexpr (HaveSharedPrimitive2DTransform<A, B>())
                return WebCore::Style::canBlend(a->toPrimitive2DTransform(), b->toPrimitive2DTransform());
            else if constexpr (HaveSharedPrimitive3DTransform<A, B>())
                return WebCore::Style::canBlend(a->toPrimitive3DTransform(), b->toPrimitive3DTransform());
            else
                return false;
        }
    ), a, b);
}

auto Blending<TransformFunction>::blend(const TransformFunction& a, const TransformFunction& b, const BlendingContext& context) -> TransformFunction
{
    return std::visit(WTF::makeVisitor(
        [&]<CSSValueID NameA, typename A, CSSValueID NameB, typename B>(const FunctionNotation<NameA, A>& a, const FunctionNotation<NameB, B>& b) -> TransformFunction {
            if constexpr (std::is_same_v<A, B>) {
                return FunctionNotation<NameA, A> {
                    WebCore::Style::blend(*a, *b, context)
                };
            } else if constexpr (HaveSharedPrimitive2DTransform<A, B>()) {
                return FunctionNotation<Primitive2DTransform<A>::name, Primitive2DTransform<A>> {
                    WebCore::Style::blend(a->toPrimitive2DTransform(), b->toPrimitive2DTransform(), context)
                };
            } else if constexpr (HaveSharedPrimitive3DTransform<A, B>()) {
                return FunctionNotation<Primitive3DTransform<A>::name, Primitive3DTransform<A>> {
                    WebCore::Style::blend(a->toPrimitive3DTransform(), b->toPrimitive3DTransform(), context)
                };
            } else
                return b;
        }
    ), a, b);
}

// MARK: - Conversion

auto ToCSS<TransformFunction>::operator()(const TransformFunction& value, const RenderStyle& style) -> CSS::TransformFunction
{
    return WTF::switchOn(value,
        [&](const Matrix3DFunction& matrix3D) -> CSS::TransformFunction {
            // NOTE: An affine Style::Matrix3D gets resolved to a CSS::Matrix to match historical serialization.
            if (matrix3D->value.isAffine())
                return toCSS(MatrixFunction { { matrix3D->value.toAffineTransform() } }, style);
            else
                return toCSS(matrix3D, style);
        },
        [&](const auto& function) -> CSS::TransformFunction {
            return toCSS(function, style);
        }
    );
}

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream& ts, const Matrix& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Matrix3D& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Rotate& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Rotate3D& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const RotateX& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const RotateY& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const RotateZ& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Skew& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const SkewX& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const SkewY& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Scale& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Scale3D& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ScaleX& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ScaleY& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ScaleZ& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Translate& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Translate3D& value)
{
    return WTF::streamCommaSeparatedForTupleLike(ts, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const TranslateX& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const TranslateY& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const TranslateZ& value)
{
    return ts << value.value;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const Perspective& value)
{
    return WTF::switchOn(value.value, [&](const auto& value) -> WTF::TextStream& { return ts << value; });
}

} // namespace Style

// MARK: - TransformFunction/TransformFunctionProxy (primitive transform type)

static TransformFunctionType computePrimitiveTransformType(const Style::IsTransformFunction auto& function)
{
    return WTF::switchOn(function,
        [](const Style::MatrixFunction&) {
            return TransformFunctionType::Matrix;
        },
        [](const Style::Matrix3DFunction&) {
            return TransformFunctionType::Matrix3D;
        },
        []<Style::IsRotate T>(const T&) {
            return Style::Is3D<T> ? TransformFunctionType::Rotate3D : TransformFunctionType::Rotate;
        },
        []<Style::IsScale T>(const T&) {
            return Style::Is3D<T> ? TransformFunctionType::Scale3D : TransformFunctionType::Scale;
        },
        []<Style::IsTranslate T>(const T&) {
            return Style::Is3D<T> ? TransformFunctionType::Translate3D : TransformFunctionType::Translate;
        },
        [](const Style::SkewFunction&) {
            return TransformFunctionType::Skew;
        },
        [](const Style::SkewXFunction&) {
            return TransformFunctionType::SkewX;
        },
        [](const Style::SkewYFunction&) {
            return TransformFunctionType::SkewY;
        },
        [](const Style::PerspectiveFunction&) {
            return TransformFunctionType::Perspective;
        }
    );
}

TransformFunctionType PrimitiveTransformType<Style::TransformFunction>::operator()(const Style::TransformFunction& function)
{
    return computePrimitiveTransformType(function);
}

TransformFunctionType PrimitiveTransformType<Style::TransformFunctionProxy>::operator()(const Style::TransformFunctionProxy& function)
{
    return computePrimitiveTransformType(function);
}

} // namespace WebCore
