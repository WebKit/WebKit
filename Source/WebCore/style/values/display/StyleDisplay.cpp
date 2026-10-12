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
#include "CSSKeywordValue.h"
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

    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        // [ <display-outside> || <display-inside> ]
        case CSSValueID::Block:
            return DisplayType::BlockFlow;
        case CSSValueID::FlowRoot:
            return DisplayType::BlockFlowRoot;
        case CSSValueID::Table:
            return DisplayType::BlockTable;
        case CSSValueID::Flex:
            return DisplayType::BlockFlex;
        case CSSValueID::Grid:
            return DisplayType::BlockGrid;
        case CSSValueID::GridLanes:
            return DisplayType::BlockGridLanes;

        case CSSValueID::Inline:
            return DisplayType::InlineFlow;
        case CSSValueID::InlineBlock:
            return DisplayType::InlineFlowRoot;
        case CSSValueID::InlineTable:
            return DisplayType::InlineTable;
        case CSSValueID::InlineFlex:
            return DisplayType::InlineFlex;
        case CSSValueID::InlineGrid:
            return DisplayType::InlineGrid;
        case CSSValueID::InlineGridLanes:
            return DisplayType::InlineGridLanes;
        case CSSValueID::Ruby:
            return DisplayType::InlineRuby;

        // <display-listitem>
        case CSSValueID::ListItem:
            return DisplayType::BlockFlowListItem;

        // <display-internal>
        case CSSValueID::TableRowGroup:
            return DisplayType::TableRowGroup;
        case CSSValueID::TableHeaderGroup:
            return DisplayType::TableHeaderGroup;
        case CSSValueID::TableFooterGroup:
            return DisplayType::TableFooterGroup;
        case CSSValueID::TableRow:
            return DisplayType::TableRow;
        case CSSValueID::TableColumnGroup:
            return DisplayType::TableColumnGroup;
        case CSSValueID::TableColumn:
            return DisplayType::TableColumn;
        case CSSValueID::TableCell:
            return DisplayType::TableCell;
        case CSSValueID::TableCaption:
            return DisplayType::TableCaption;
        case CSSValueID::RubyBase:
            return DisplayType::RubyBase;
        case CSSValueID::RubyText:
            return DisplayType::RubyText;

        // <display-box>
        case CSSValueID::Contents:
            return DisplayType::Contents;
        case CSSValueID::None:
            return DisplayType::None;

        // <-webkit-display>
        case CSSValueID::WebkitBox:
            return DisplayType::BlockDeprecatedFlex;
        case CSSValueID::WebkitInlineBox:
            return DisplayType::InlineDeprecatedFlex;

        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return DisplayType::InlineFlow;
        }
    }

    auto pair = requiredPairDowncast<CSSKeywordValue>(state, value);
    if (!pair)
        return DisplayType::InlineFlow;

    auto handleInside = []<CSSPropertyParserHelpers::DisplayOutside outside>(BuilderState& state, CSSValueID inside) {
        switch (inside) {
        case CSSValueID::Flow:
            return mappedDisplayType<outside, Flow>();

        case CSSValueID::FlowRoot:
            return mappedDisplayType<outside, FlowRoot>();

        case CSSValueID::Table:
            return mappedDisplayType<outside, Table>();

        case CSSValueID::Flex:
            return mappedDisplayType<outside, Flex>();

        case CSSValueID::Grid:
            return mappedDisplayType<outside, Grid>();

        case CSSValueID::GridLanes:
            return mappedDisplayType<outside, GridLanes>();

        case CSSValueID::Ruby:
            return mappedDisplayType<outside, Ruby>();

        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return DisplayType::InlineFlow;
        }
    };

    Ref first = pair->first;
    Ref second = pair->second;

    switch (first->valueID()) {
    case CSSValueID::Block:
        return handleInside.template operator()<Block>(state, second->valueID());

    case CSSValueID::Inline:
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
