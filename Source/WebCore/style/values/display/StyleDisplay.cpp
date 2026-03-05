/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleDisplay.h"

#include "AnimationUtilities.h"
#include "CSSPropertyParserConsumer+Display.h"
#include "StyleBuilderChecking.h"
#include <wtf/EnumeratedArray.h>

namespace WebCore {
namespace Style {

using DisplayOutsideInsideToDisplayTypeMap = EnumeratedArray<CSSPropertyParserHelpers::DisplayOutside, EnumeratedArray<CSSPropertyParserHelpers::DisplayInside, std::optional<DisplayType>>>;

consteval DisplayOutsideInsideToDisplayTypeMap NODELETE makeDisplayOutsideInsideToDisplayTypeMap()
{
    using enum CSSPropertyParserHelpers::DisplayOutside;
    using enum CSSPropertyParserHelpers::DisplayInside;

    DisplayOutsideInsideToDisplayTypeMap result;

    result[NoOutside][NoInside]  = std::nullopt;

    result[Block][NoInside]      = DisplayType::BlockFlow;
    result[Block][Flow]          = DisplayType::BlockFlow;
    result[Block][FlowRoot]      = DisplayType::BlockFlowRoot;
    result[Block][Table]         = DisplayType::BlockTable;
    result[Block][Flex]          = DisplayType::BlockFlex;
    result[Block][Grid]          = DisplayType::BlockGrid;
    result[Block][GridLanes]     = DisplayType::BlockGridLanes;
    result[Block][Ruby]          = DisplayType::BlockRuby;

    result[Inline][NoInside]     = DisplayType::InlineFlow;
    result[Inline][Flow]         = DisplayType::InlineFlow;
    result[Inline][FlowRoot]     = DisplayType::InlineFlowRoot;
    result[Inline][Table]        = DisplayType::InlineTable;
    result[Inline][Flex]         = DisplayType::InlineFlex;
    result[Inline][Grid]         = DisplayType::InlineGrid;
    result[Inline][GridLanes]    = DisplayType::InlineGridLanes;
    result[Inline][Ruby]         = DisplayType::InlineRuby;

    result[NoOutside][Flow]      = result[Block][Flow];
    result[NoOutside][FlowRoot]  = result[Block][FlowRoot];
    result[NoOutside][Table]     = result[Block][Table];
    result[NoOutside][Flex]      = result[Block][Flex];
    result[NoOutside][Grid]      = result[Block][Grid];
    result[NoOutside][GridLanes] = result[Block][GridLanes];
    result[NoOutside][Ruby]      = result[Inline][Ruby];

    return result;
}

constexpr auto displayOutsideInsideToDisplayTypeMap = makeDisplayOutsideInsideToDisplayTypeMap();

template<CSSPropertyParserHelpers::DisplayOutside outside, CSSPropertyParserHelpers::DisplayInside inside>
consteval DisplayType NODELETE mappedDisplayType()
{
    return *displayOutsideInsideToDisplayTypeMap[outside][inside];
}

// MARK: - Conversion

auto CSSValueConversion<Display>::operator()(BuilderState& state, const CSSValue& value) -> Display
{
    using enum CSSPropertyParserHelpers::DisplayOutside;
    using enum CSSPropertyParserHelpers::DisplayInside;

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        // [ <display-outside> || <display-inside> ]
        case CSSValueBlock:
            return DisplayType::BlockFlow;
        case CSSValueFlowRoot:
            return DisplayType::BlockFlowRoot;
        case CSSValueTable:
            return DisplayType::BlockTable;
        case CSSValueFlex:
            return DisplayType::BlockFlex;
        case CSSValueGrid:
            return DisplayType::BlockGrid;
        case CSSValueGridLanes:
            return DisplayType::BlockGridLanes;

        case CSSValueInline:
            return DisplayType::InlineFlow;
        case CSSValueInlineBlock:
            return DisplayType::InlineFlowRoot;
        case CSSValueInlineTable:
            return DisplayType::InlineTable;
        case CSSValueInlineFlex:
            return DisplayType::InlineFlex;
        case CSSValueInlineGrid:
            return DisplayType::InlineGrid;
        case CSSValueInlineGridLanes:
            return DisplayType::InlineGridLanes;
        case CSSValueRuby:
            return DisplayType::InlineRuby;

        // <display-listitem>
        case CSSValueListItem:
            return DisplayType::BlockFlowListItem;

        // <display-internal>
        case CSSValueTableRowGroup:
            return DisplayType::TableRowGroup;
        case CSSValueTableHeaderGroup:
            return DisplayType::TableHeaderGroup;
        case CSSValueTableFooterGroup:
            return DisplayType::TableFooterGroup;
        case CSSValueTableRow:
            return DisplayType::TableRow;
        case CSSValueTableColumnGroup:
            return DisplayType::TableColumnGroup;
        case CSSValueTableColumn:
            return DisplayType::TableColumn;
        case CSSValueTableCell:
            return DisplayType::TableCell;
        case CSSValueTableCaption:
            return DisplayType::TableCaption;
        case CSSValueRubyBase:
            return DisplayType::RubyBase;
        case CSSValueRubyText:
            return DisplayType::RubyText;

        // <display-box>
        case CSSValueContents:
            return DisplayType::Contents;
        case CSSValueNone:
            return DisplayType::None;

        // <-webkit-display>
        case CSSValueWebkitBox:
            return DisplayType::BlockDeprecatedFlex;
        case CSSValueWebkitInlineBox:
            return DisplayType::InlineDeprecatedFlex;

        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return DisplayType::InlineFlow;
        }
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return DisplayType::InlineFlow;

    auto handleInside = []<CSSPropertyParserHelpers::DisplayOutside outside>(BuilderState& state, CSSValueID inside) {
        switch (inside) {
        case CSSValueFlow:
            return mappedDisplayType<outside, Flow>();

        case CSSValueFlowRoot:
            return mappedDisplayType<outside, FlowRoot>();

        case CSSValueTable:
            return mappedDisplayType<outside, Table>();

        case CSSValueFlex:
            return mappedDisplayType<outside, Flex>();

        case CSSValueGrid:
            return mappedDisplayType<outside, Grid>();

        case CSSValueGridLanes:
            return mappedDisplayType<outside, GridLanes>();

        case CSSValueRuby:
            return mappedDisplayType<outside, Ruby>();

        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return DisplayType::InlineFlow;
        }
    };

    Ref first = pair->first;
    Ref second = pair->second;

    switch (first->valueID()) {
    case CSSValueBlock:
        return handleInside.template operator()<Block>(state, second->valueID());

    case CSSValueInline:
        return handleInside.template operator()<Inline>(state, second->valueID());

    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return DisplayType::InlineFlow;
    }
}

// MARK: - Blending

auto Blending<Display>::blend(Display a, Display b, const BlendingContext& context) -> Display
{
    // "In general, the display property's animation type is discrete. However, similar to interpolation of
    //  visibility, during interpolation between none and any other display value, p values between 0 and 1
    //  map to the non-none value. Additionally, the element is inert as long as its display value would
    //  compute to none when ignoring the Transitions and Animations cascade origins."
    // (https://drafts.csswg.org/css-display-4/#display-animation)

    if (a != DisplayType::None && b != DisplayType::None)
        return context.progress < 0.5 ? a : b;
    if (context.progress <= 0)
        return a;
    if (context.progress >= 1)
        return b;
    return a == DisplayType::None ? b : a;
}

} // namespace Style
} // namespace WebCore
