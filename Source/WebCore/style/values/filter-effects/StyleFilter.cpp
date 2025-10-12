/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleFilter.h"

#include "CSSFilterValue.h"
#include "Document.h"
#include "DropShadowFilterOperationWithStyleColor.h"
#include "FEGaussianBlur.h"
#include "FilterOperations.h"
#include "ReferenceFilterOperation.h"
#include "StyleBlurFunction.h"
#include "StyleBrightnessFunction.h"
#include "StyleBuilderChecking.h"
#include "StyleContrastFunction.h"
#include "StyleDropShadowFunction.h"
#include "StyleFilterInterpolation.h"
#include "StyleFilterReference.h"
#include "StyleGrayscaleFunction.h"
#include "StyleHueRotateFunction.h"
#include "StyleInvertFunction.h"
#include "StyleOpacityFunction.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include "StyleSaturateFunction.h"
#include "StyleSepiaFunction.h"

namespace WebCore {
namespace Style {

bool Filter::hasReferenceFilter() const
{
    return hasFilterOfType<FilterOperation::Type::Reference>();
}

bool Filter::isReferenceFilter() const
{
    return size() == 1 && first()->type() == FilterOperation::Type::Reference;
}

bool Filter::hasFilterThatRequiresRepaintForCurrentColorChange() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->requiresRepaintForCurrentColorChange(); });
}

bool Filter::hasFilterThatAffectsOpacity() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->affectsOpacity(); });
}

bool Filter::hasFilterThatMovesPixels() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->movesPixels(); });
}

bool Filter::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->shouldBeRestrictedBySecurityOrigin(); });
}

IntOutsets Filter::outsets() const
{
    IntOutsets totalOutsets;
    for (auto& value : *this) {
        Ref operation = value.value;
        switch (operation->type()) {
        case FilterOperation::Type::Blur: {
            auto& blurOperation = downcast<BlurFilterOperation>(operation.get());
            float stdDeviation = blurOperation.stdDeviation();
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });
            IntOutsets outsets(outsetSize.height(), outsetSize.width(), outsetSize.height(), outsetSize.width());
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::Type::DropShadow:
        case FilterOperation::Type::DropShadowWithStyleColor: {
            auto& dropShadowOperation = downcast<DropShadowFilterOperationBase>(operation.get());
            float stdDeviation = dropShadowOperation.stdDeviation();
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });

            int top = std::max(0, outsetSize.height() - dropShadowOperation.y());
            int right = std::max(0, outsetSize.width() + dropShadowOperation.x());
            int bottom = std::max(0, outsetSize.height() + dropShadowOperation.y());
            int left = std::max(0, outsetSize.width() - dropShadowOperation.x());

            auto outsets = IntOutsets { top, right, bottom, left };
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::Type::Reference:
            ASSERT_NOT_REACHED();
            break;
        default:
            break;
        }
    }
    return totalOutsets;
}

// MARK: - Conversions

// MARK: (FilterValue)

auto ToCSS<FilterValue>::operator()(const FilterValue& value, const RenderStyle& style) -> CSS::FilterValue
{
    Ref op = value.value;
    switch (op->type()) {
    case FilterOperation::Type::Reference:
        return CSS::FilterValue { CSS::FilterReference { toCSSFilterReference(downcast<ReferenceFilterOperation>(op), style) } };
    case FilterOperation::Type::Grayscale:
        return CSS::FilterValue { CSS::GrayscaleFunction { toCSSGrayscale(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Sepia:
        return CSS::FilterValue { CSS::SepiaFunction { toCSSSepia(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Saturate:
        return CSS::FilterValue { CSS::SaturateFunction { toCSSSaturate(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::HueRotate:
        return CSS::FilterValue { CSS::HueRotateFunction { toCSSHueRotate(downcast<BasicColorMatrixFilterOperation>(op), style) } };
    case FilterOperation::Type::Invert:
        return CSS::FilterValue { CSS::InvertFunction { toCSSInvert(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Opacity:
        return CSS::FilterValue { CSS::OpacityFunction { toCSSOpacity(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Brightness:
        return CSS::FilterValue { CSS::BrightnessFunction { toCSSBrightness(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Contrast:
        return CSS::FilterValue { CSS::ContrastFunction { toCSSContrast(downcast<BasicComponentTransferFilterOperation>(op), style) } };
    case FilterOperation::Type::Blur:
        return CSS::FilterValue { CSS::BlurFunction { toCSSBlur(downcast<BlurFilterOperation>(op), style) } };
    case FilterOperation::Type::DropShadowWithStyleColor:
        return CSS::FilterValue { CSS::DropShadowFunction { toCSSDropShadow(downcast<DropShadowFilterOperationWithStyleColor>(op), style) } };
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto ToStyle<CSS::FilterValue>::operator()(const CSS::FilterValue& value, const BuilderState& state) -> FilterValue
{
    return WTF::switchOn(value,
        [&](const auto& function) -> FilterValue {
            return FilterValue { createFilterOperation(function, state) };
        }
    );
}

// MARK: (Filter)

auto ToCSS<Filter>::operator()(const Filter& value, const RenderStyle& style) -> CSS::Filter
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::None keyword) -> CSS::Filter {
            return keyword;
        },
        [&](const FilterValueList& list) -> CSS::Filter {
            return CSS::FilterValueList::map(list, [&](const FilterValue& value) {
                return toCSS(value, style);
            });
        }
    );
}

auto ToStyle<CSS::Filter>::operator()(const CSS::Filter& value, const BuilderState& state) -> Filter
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::None keyword) -> Filter {
            return keyword;
        },
        [&](const CSS::FilterValueList& list) -> Filter {
            return FilterValueList::map(list, [&](const CSS::FilterValue& value) {
                return toStyle(value, state);
            });
        }
    );
}

auto CSSValueConversion<Filter>::operator()(BuilderState& state, const CSSValue& value) -> Filter
{
    if (value.valueID() == CSSValueNone)
        return CSS::Keyword::None { };

    RefPtr filter = requiredDowncast<CSSFilterValue>(state, value);
    if (!filter)
        return CSS::Keyword::None { };

    return toStyle(filter->filter(), state);
}

Ref<CSSValue> CSSValueCreation<Filter>::operator()(CSSValuePool&, const RenderStyle& style, const Filter& value)
{
    return CSSFilterValue::create(toCSS(value, style));
}

// MARK: - Serialization

void Serialize<Filter>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const Filter& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Blending

auto Blending<Filter>::canBlend(const Filter& from, const Filter& to, CompositeOperation compositeOperation) -> bool
{
    return canBlendFilterLists(from.m_value, to.m_value, compositeOperation);
}

auto Blending<Filter>::blend(const Filter& from, const Filter& to, const BlendingContext& context) -> Filter
{
    auto blendedFilterList = blendFilterLists(from.m_value, to.m_value, context);

    if (blendedFilterList.isEmpty())
        return CSS::Keyword::None { };

    return Filter { WTFMove(blendedFilterList) };
}

// MARK: - Platform

auto ToPlatform<FilterValue>::operator()(const FilterValue& value) -> Ref<FilterOperation>
{
    return value.value;
}

auto ToPlatform<Filter>::operator()(const Filter& value) -> FilterOperations
{
    return FilterOperations { WTF::map(value, [](auto& filterValue) {
        return toPlatform(filterValue);
    }) };
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const FilterValue& value)
{
    Ref platform = value.value;
    return ts << platform;
}

} // namespace Style
} // namespace WebCore
