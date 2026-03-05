/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
#include "FilterOperations.h"
#include "StyleBuilderChecking.h"
#include "StyleFilterInterpolation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

bool Filter::hasReferenceFilter() const
{
    return hasFilterOfType<FilterReference>();
}

bool Filter::isReferenceFilter() const
{
    return size() == 1 && WTF::holdsAlternative<FilterReference>(first());
}

bool Filter::hasFilterThatRequiresRepaintForCurrentColorChange() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) {
        return WTF::switchOn(filterValue,
            [](auto& op) {
                return op->requiresRepaintForCurrentColorChange();
            }
        );
    });
}

bool Filter::hasFilterThatAffectsOpacity() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) {
        return WTF::switchOn(filterValue,
            [](auto& op) {
                return op->affectsOpacity();
            }
        );
    });
}

bool Filter::hasFilterThatMovesPixels() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) {
        return WTF::switchOn(filterValue,
            [](auto& op) {
                return op->movesPixels();
            }
        );
    });
}

bool Filter::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) {
        return WTF::switchOn(filterValue,
            [](auto& op) {
                return op->shouldBeRestrictedBySecurityOrigin();
            }
        );
    });
}

IntOutsets Filter::calculateOutsets(ZoomFactor zoom) const
{
    IntOutsets totalOutsets;
    for (auto& filterValue : *this) {
        WTF::switchOn(filterValue,
            [&](const BlurFunction& blurFunction) {
                totalOutsets += blurFunction->calculateOutsets(zoom);
            },
            [&](const DropShadowFunction& dropShadowFunction) {
                totalOutsets += dropShadowFunction->calculateOutsets(zoom);
            },
            [](const FilterReference&) {
                ASSERT_NOT_REACHED();
            },
            [](const auto&) { }
        );
    }
    return totalOutsets;
}

// MARK: - Conversions

// MARK: (FilterValueList)

auto ToCSS<FilterValueList>::operator()(const FilterValueList& value, const RenderStyle& style) -> CSS::FilterValueList
{
    return CSS::FilterValueList::map(value, [&](const auto& x) -> CSS::FilterValue { return toCSS(x, style); });
}

auto ToStyle<CSS::FilterValueList>::operator()(const CSS::FilterValueList& value, const BuilderState& state) -> FilterValueList
{
    return FilterValueList::map(value, [&](const auto& x) -> FilterValue { return toStyle(x, state); });
}

// MARK: (Filter)

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

// MARK: - Blending

auto Blending<Filter>::canBlend(const Filter& from, const Filter& to, CompositeOperation compositeOperation) -> bool
{
    // We can't interpolate between lists if a reference filter is involved.
    if (from.hasReferenceFilter() || to.hasReferenceFilter())
        return false;

    return canBlendFilterLists(from.m_value, to.m_value, compositeOperation);
}

auto Blending<Filter>::blend(const Filter& from, const Filter& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> Filter
{
    auto blendedFilterList = blendFilterLists(from.m_value, to.m_value, fromStyle, toStyle, context);

    if (blendedFilterList.isEmpty())
        return CSS::Keyword::None { };

    return Filter { WTF::move(blendedFilterList) };
}

// MARK: - Platform

auto ToPlatform<FilterValue>::operator()(const FilterValue& value, const RenderStyle& style) -> Ref<FilterOperation>
{
    return WTF::switchOn(value,
        [&](const BlurFunction& blurFunction) -> Ref<FilterOperation> {
            return toPlatform(blurFunction, style);
        },
        [&](const DropShadowFunction& dropShadowFunction) -> Ref<FilterOperation> {
            return toPlatform(dropShadowFunction, style);
        },
        [](const FilterReference&) -> Ref<FilterOperation> {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [](const auto& function) -> Ref<FilterOperation> {
            return toPlatform(function);
        }
    );
}

auto ToPlatform<Filter>::operator()(const Filter& value, const RenderStyle& style) -> FilterOperations
{
    return FilterOperations { WTF::map(value, [&](auto& filterValue) -> Ref<FilterOperation> {
        return toPlatform(filterValue, style);
    }) };
}

} // namespace Style
} // namespace WebCore
