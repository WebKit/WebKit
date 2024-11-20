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
#include "StyleTransformList.h"

#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TransformList.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

auto TransformList::toPlatform(const FloatSize& referenceBox) const -> TransformList::Platform
{
    TransformList::Platform result;
    forEach([&](const auto& function) { result.list.append(Style::toPlatform(function, referenceBox)); });
    return result;
}

// MARK: - Blending

enum class UseDiscreteAnimation : bool { No, Yes };

bool shouldFallBackToDiscreteAnimation(const TransformList& from, const TransformList& to, const LayoutSize& referenceBox)
{
    return from.containsNonInvertibleMatrix(referenceBox) || to.containsNonInvertibleMatrix(referenceBox);
}

static TransformFunction createSuffixMatrixBlend(const TransformList& from, const TransformList& to, const BlendingContext& context, const LayoutSize& referenceBox, unsigned start, UseDiscreteAnimation useDiscreteAnimation)
{
    TransformationMatrix fromTransform;
    from.applyTransform(fromTransform, referenceBox, start);

    TransformationMatrix toTransform;
    to.applyTransform(toTransform, referenceBox, start);

    if (useDiscreteAnimation == UseDiscreteAnimation::Yes)
        toTransform.blend(fromTransform, context.progress < 0.5 ? 0 : 1, CompositeOperation::Replace);
    else
        toTransform.blend(fromTransform, context.progress, context.compositeOperation);

    return Matrix3DFunction { { toTransform } };
}

static bool shouldPerformSuffixMatrixBlend(const std::optional<TransformFunctionProxy>& from, const std::optional<TransformFunctionProxy>& to, unsigned start, std::optional<unsigned> prefixLength)
{
    return (prefixLength && start >= *prefixLength)
        || (from && to && !haveSharedPrimitiveType(*from, *to));
}

TransformList blend(const TransformList& from, const TransformList& to, const BlendingContext& context, const LayoutSize& referenceBox, std::optional<unsigned> prefixLength)
{
    using List = SpaceSeparatedVariantList<TransformFunction>::VariantList;

    if (shouldFallBackToDiscreteAnimation(from, to, referenceBox))
        return { createSuffixMatrixBlend(from, to, context, referenceBox, 0, UseDiscreteAnimation::Yes) };

    List blendedFunctions;

    auto fromCurrent = from.begin();
    auto fromEnd = from.end();

    auto toCurrent = to.begin();
    auto toEnd = to.end();

    // If either of the transform list is empty, then we should not attempt to do a matrix blend.
    bool mayAttemptSuffixMatrixBlend = !from.isEmpty() && !to.isEmpty();

    // The number of transform functions already blended.
    unsigned count = 0;

    while (fromCurrent != fromEnd || toCurrent != toEnd) {
        std::optional fromFunction = fromCurrent != fromEnd ? std::make_optional(*fromCurrent) : std::nullopt;
        std::optional toFunction = toCurrent != toEnd ? std::make_optional(*toCurrent) : std::nullopt;

        if (mayAttemptSuffixMatrixBlend && shouldPerformSuffixMatrixBlend(fromFunction, toFunction, count, prefixLength)) {
            blendedFunctions.append(createSuffixMatrixBlend(from, to, context, referenceBox, count, UseDiscreteAnimation::No));
            return { WTFMove(blendedFunctions) };
        }

        auto blendedFunction = [&] {
            if (fromFunction && toFunction)
                return blend(fromFunction->asVariant(), toFunction->asVariant(), context);
            if (toFunction) {
                auto toFunctionVariant = toFunction->asVariant();
                return blend(identity(toFunctionVariant), toFunctionVariant, context);
            }
            auto fromFunctionVariant = fromFunction->asVariant();
            return blend(fromFunctionVariant, identity(fromFunctionVariant), context);
        };
        blendedFunctions.append(blendedFunction());

        ++count;
        if (fromCurrent != fromEnd)
            fromCurrent = std::next(fromCurrent);
        if (toCurrent != toEnd)
            toCurrent = std::next(toCurrent);
    }

    return { WTFMove(blendedFunctions) };
}

// MARK: - Conversion

auto ToCSS<TransformList>::operator()(const TransformList& value, const RenderStyle& style) -> CSS::TransformList
{
    using VariantList = CSS::TransformList::List::VariantList;

    VariantList::Sizer sizer;
    value.value.forEach(
        [&](const Matrix3DFunction& matrix3D) {
            // NOTE: An affine Style::Matrix3D gets resolved to a CSS::Matrix to match historical serialization.
            if (matrix3D->value.isAffine())
                sizer.append<CSS::MatrixFunction>();
            else
                sizer.append<decltype(toCSS(matrix3D, style))>();
        },
        [&](const auto& function) {
            sizer.append<decltype(toCSS(function, style))>();
        }
    );

    VariantList result(sizer);
    value.value.forEach(
        [&](const Matrix3DFunction& matrix3D) {
            // NOTE: An affine Style::Matrix3D gets resolved to a CSS::Matrix to match historical serialization.
            if (matrix3D->value.isAffine())
                result.append(toCSS(MatrixFunction { { matrix3D->value.toAffineTransform() } }, style));
            else
                result.append(toCSS(matrix3D, style));
        },
        [&](const auto& function) {
            result.append(toCSS(function, style));
        }
    );

    return { WTFMove(result) };
}

auto ToStyle<CSS::TransformList>::operator()(const CSS::TransformList& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) -> TransformList
{
    using VariantList = TransformList::List::VariantList;

    VariantList::Sizer sizer;
    value.value.forEach([&](const auto& element) { sizer.template append<decltype(toStyle(element, conversionData, symbolTable))>(); });

    VariantList result(sizer);
    value.value.forEach([&](const auto& element) { result.append(toStyle(element, conversionData, symbolTable)); });

    return { WTFMove(result) };
}

auto ToStyle<CSS::TransformList>::operator()(const CSS::TransformList& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable) -> TransformList
{
    using VariantList = TransformList::List::VariantList;

    VariantList::Sizer sizer;
    value.value.forEach([&](const auto& element) { sizer.template append<decltype(toStyle(element, builderState, symbolTable))>(); });

    VariantList result(sizer);
    value.value.forEach([&](const auto& element) { result.append(toStyle(element, builderState, symbolTable)); });

    return { WTFMove(result) };
}

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream& ts, const TransformList& value)
{
    auto separator = ""_s;
    value.value.forEach([&](const auto& function) {
        ts << std::exchange(separator, " "_s);
        ts << function;
    });

    return ts;
}

} // namespace Style
} // namespace WebCore
